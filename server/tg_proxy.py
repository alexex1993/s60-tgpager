#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tg_proxy.py — TCP-прокси между Nokia N70 (S60) и Telegram Bot API.

Архитектура:
    Nokia N70 (TCP, без TLS) <--> этот прокси (Debian 13) <--> https://api.telegram.org

Протокол телефон<->прокси: строки UTF-8, терминатор '\\n', один запрос — одна строка.
Экранирование полезной нагрузки в SEND/MSG (в обе стороны):
    '\\' (обратный слэш) -> '\\\\'   (два символа)
    перевод строки        -> '\\n'   (два символа)

Команды:
    AUTH <код>            -> OK | ERR auth (после ERR auth соединение закрывается)
    SEND <экр. текст>     -> OK | ERR telegram | ERR nochat
    POLL                  -> ноль и более 'MSG <экр. текст>' затем 'END' (соединение не закрывать)

Только стандартная библиотека. Потоки: фоновый getUpdates + поток на TCP-клиента.
"""

import collections
import json
import logging
import os
import select
import signal
import socket
import threading
import time
import urllib.error
import urllib.request

# --------------------------------------------------------------------------
# Конфигурация из окружения
# --------------------------------------------------------------------------

BOT_TOKEN = os.environ.get("BOT_TOKEN", "").strip()
LISTEN_HOST = os.environ.get("LISTEN_HOST", "0.0.0.0")
LISTEN_PORT = int(os.environ.get("LISTEN_PORT", "7777"))
REQUIRE_CODE = os.environ.get("REQUIRE_CODE", "")
# TG_API_BASE нужен только для локального тестирования (подменённый API);
# по умолчанию — настоящий Telegram.
TG_API_BASE = os.environ.get("TG_API_BASE", "https://api.telegram.org").rstrip("/")

MAX_TEXT = int(os.environ.get("MAX_TEXT", "400"))

_raw_chat = os.environ.get("ALLOWED_CHAT_ID", "").strip()
if _raw_chat:
    try:
        ALLOWED_CHAT_ID = int(_raw_chat)
    except ValueError:
        raise SystemExit(f"ALLOWED_CHAT_ID must be an integer, got: {_raw_chat!r}")
else:
    ALLOWED_CHAT_ID = None

# --------------------------------------------------------------------------
# Константы
# --------------------------------------------------------------------------

QUEUE_MAX = 100                 # очередь сообщений для POLL (старые вытесняются)
POLL_WAIT_SECONDS = 50.0        # сколько POLL ждёт сообщения при пустой очереди
POLL_WAIT_SLICE = 5.0           # ломка ожидания, чтобы поток замечал shutdown
CLIENT_IDLE_TIMEOUT = 120.0     # явный таймаут сокета: "зависший" клиент отваливается
SEND_HTTP_TIMEOUT = 15.0        # таймаут sendMessage
GETUPDATES_TIMEOUT = 25         # параметр long-poll Telegram
GETUPDATES_HTTP_TIMEOUT = 40.0  # 25 c удержания запроса + запас на сеть
MAX_LINE_BYTES = 32 * 1024      # защита от неограниченной строки
RETRY_MAX_DELAY = 60.0          # максимум паузы между повторами getUpdates

log = logging.getLogger("tg-pager")

# --------------------------------------------------------------------------
# Общее состояние
# --------------------------------------------------------------------------

shutdown = threading.Event()

_queue = collections.deque(maxlen=QUEUE_MAX)  # сырые (неэкранированные) усечённые тексты
_cond = threading.Condition()

_chat_lock = threading.Lock()
# Если ALLOWED_CHAT_ID задан — получатель известен сразу; иначе фиксируется
# первым валидным сообщением (process_update).
target_chat_id = ALLOWED_CHAT_ID

_clients = set()       # открытые клиентские сокеты (чтобы закрыть их при остановке)
_clients_lock = threading.Lock()
_client_threads = []

# --------------------------------------------------------------------------
# Экранирование
# --------------------------------------------------------------------------

def escape_text(text: str) -> str:
    """'\\' -> '\\\\', перевод строки -> '\\n'."""
    return text.replace("\\", "\\\\").replace("\n", "\\n")


def unescape_text(s: str) -> str:
    out = []
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == "\\" and i + 1 < n:
            nxt = s[i + 1]
            if nxt == "\\":
                out.append("\\")
                i += 2
                continue
            if nxt == "n":
                out.append("\n")
                i += 2
                continue
        # одиночный '\' перед чем угодно иным — оставляем как есть (толерантно)
        out.append(c)
        i += 1
    return "".join(out)

# --------------------------------------------------------------------------
# Telegram Bot API
# --------------------------------------------------------------------------

def tg_call(method: str, params: dict, timeout: float):
    """POST JSON к Bot API. Возвращает 'result'. Бросает исключения при ошибках."""
    url = f"{TG_API_BASE}/bot{BOT_TOKEN}/{method}"
    payload = json.dumps(params, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=payload,
        headers={"Content-Type": "application/json; charset=utf-8"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        body = json.loads(resp.read().decode("utf-8"))
    if not body.get("ok"):
        raise RuntimeError(f"telegram api error response: {body}")
    return body.get("result")


def tg_send_message(text: str) -> str:
    """Возвращает 'ok' | 'nochat' | 'telegram'."""
    with _chat_lock:
        chat_id = target_chat_id
    if chat_id is None:
        return "nochat"
    try:
        tg_call(
            "sendMessage",
            {
                "chat_id": chat_id,
                "text": text,
                "link_preview_options": {"is_disabled": True},
            },
            SEND_HTTP_TIMEOUT,
        )
        return "ok"
    except Exception as exc:
        log.error("sendMessage failed: %s", exc)
        return "telegram"


def process_update(upd: dict):
    """Валидирует update и возвращает текст для очереди, либо None."""
    global target_chat_id
    msg = upd.get("message")
    if not isinstance(msg, dict):
        return None  # не-text / служебные update — игнор
    text = msg.get("text")
    if not isinstance(text, str) or not text:
        return None  # не текстовые сообщения и пустые тексты игнорируются
    if text.startswith("/"):
        log.info("ignored command from telegram: %r", text[:64])
        return None

    chat = msg.get("chat") or {}
    chat_id = chat.get("id")
    if not isinstance(chat_id, int):
        return None

    if ALLOWED_CHAT_ID is not None:
        if chat_id != ALLOWED_CHAT_ID:
            log.info("ignored message from alien chat_id=%s (allowed=%s)", chat_id, ALLOWED_CHAT_ID)
            return None
    else:
        with _chat_lock:
            if target_chat_id is None:
                target_chat_id = chat_id
                log.info("target chat fixed to %s (first writer)", chat_id)

    log.info("accepted message from chat %s (%d chars)", chat_id, len(text))
    return text[:MAX_TEXT]


def enqueue_batch(texts: list) -> None:
    """Атомарно кладёт весь батч getUpdates в очередь и будит ждущие POLL."""
    if not texts:
        return
    with _cond:
        for text in texts:
            if len(_queue) >= QUEUE_MAX:
                log.warning("queue full (%d), dropping oldest message", QUEUE_MAX)
            _queue.append(text)
        _cond.notify_all()  # ждущие POLL разбуджены; сообщение достанется первому


def telegram_worker() -> None:
    """Фоновый цикл getUpdates с подтверждением offset и повторами 429/5xx."""
    offset = None
    backoff = 1.0
    while not shutdown.is_set():
        params = {"timeout": GETUPDATES_TIMEOUT, "allowed_updates": ["message"]}
        if offset is not None:
            params["offset"] = offset
        try:
            updates = tg_call("getUpdates", params, GETUPDATES_HTTP_TIMEOUT)
        except urllib.error.HTTPError as exc:
            delay = backoff
            if exc.code == 429:  # уважаем retry_after, если Telegram его прислал
                try:
                    err_body = json.loads(exc.read().decode("utf-8", "replace"))
                    ra = float(err_body.get("parameters", {}).get("retry_after", 0))
                    if ra > 0:
                        delay = min(max(ra, 1.0), RETRY_MAX_DELAY)
                except Exception:
                    pass
            log.error("getUpdates: HTTP %s, retry in %.1f s", exc.code, delay)
            backoff = min(backoff * 2, RETRY_MAX_DELAY)
            shutdown.wait(delay)
            continue
        except Exception as exc:
            log.error("getUpdates failed: %s; retry in %.1f s", exc, backoff)
            delay = backoff
            backoff = min(backoff * 2, RETRY_MAX_DELAY)
            shutdown.wait(delay)
            continue

        backoff = 1.0
        texts = []
        for upd in updates or []:
            if not isinstance(upd, dict) or "update_id" not in upd:
                continue
            offset = upd["update_id"] + 1  # следующий запрос подтверждает приём
            try:
                t = process_update(upd)
                if t is not None:
                    texts.append(t)
            except Exception:
                log.exception("failed to process update_id=%s", upd.get("update_id"))
        enqueue_batch(texts)

# --------------------------------------------------------------------------
# Работа с клиентским сокетом
# --------------------------------------------------------------------------

def recv_line(sock: socket.socket, buf: bytearray):
    """Одна строка (до '\\n') без терминатора; None при EOF. Бросает таймаут/ошибки."""
    while True:
        i = buf.find(b"\n")
        if i >= 0:
            line = bytes(buf[:i])
            del buf[: i + 1]
            return line.decode("utf-8", "replace").rstrip("\r")
        if len(buf) > MAX_LINE_BYTES:
            raise ConnectionError("line too long")
        chunk = sock.recv(4096)
        if not chunk:
            return None
        buf.extend(chunk)


def send_line(sock: socket.socket, s: str) -> None:
    sock.sendall((s + "\n").encode("utf-8"))


def peer_gone(sock: socket.socket) -> bool:
    """True, если удалённая сторона уже закрыла соединение (FIN)."""
    try:
        r, _, _ = select.select([sock], [], [], 0)
        if not r:
            return False
        return sock.recv(1, socket.MSG_PEEK | socket.MSG_DONTWAIT) == b""
    except OSError:
        return True


def handle_poll(sock: socket.socket) -> None:
    """Отдать накопившееся; если пусто — ждать до 50 с; затем END.

    Если клиент умер, пока его POLL ждал, недоставленные сообщения
    возвращаются в очередь — иначе сообщение досталось бы мёртвому POLL'у.
    """
    with _cond:
        deadline = time.monotonic() + POLL_WAIT_SECONDS
        while not _queue and not shutdown.is_set():
            if peer_gone(sock):
                log.info("poll: client gone while waiting")
                return
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            _cond.wait(min(remaining, POLL_WAIT_SLICE))
        msgs = list(_queue)
        _queue.clear()

    def requeue(rest):
        with _cond:
            _queue.extendleft(reversed(rest))
            _cond.notify_all()
        log.warning("client gone, %d message(s) re-queued", len(rest))

    if msgs and peer_gone(sock):
        requeue(msgs)
        return
    delivered = 0
    try:
        for text in msgs:
            send_line(sock, "MSG " + escape_text(text))
            delivered += 1
            log.info("-> MSG (%d chars)", len(text))
        send_line(sock, "END")
    except OSError:
        rest = msgs[delivered:]
        if rest:
            requeue(rest)
        raise


def handle_client(sock: socket.socket, addr) -> None:
    peer = f"{addr[0]}:{addr[1]}"
    buf = bytearray()
    try:
        sock.settimeout(CLIENT_IDLE_TIMEOUT)

        # --- первая строка в любом соединении: AUTH ---
        line = recv_line(sock, buf)
        if line is None:
            return
        if line == "AUTH":
            code = ""
        elif line.startswith("AUTH "):
            code = line[5:]
        else:
            code = None

        if REQUIRE_CODE:
            if code != REQUIRE_CODE:
                log.info("%s: AUTH rejected", peer)
                send_line(sock, "ERR auth")
                return  # закрываем соединение
        else:
            if code is None:  # REQUIRE_CODE пуст, но первая строка вообще не AUTH
                log.info("%s: first line is not AUTH: %r", peer, line[:64])
                send_line(sock, "ERR auth")
                return
        log.info("%s: AUTH ok", peer)
        send_line(sock, "OK")

        # --- цикл команд (SEND и/или POLL на одном соединении) ---
        while not shutdown.is_set():
            line = recv_line(sock, buf)
            if line is None:
                break
            if line == "POLL":
                handle_poll(sock)
            elif line == "SEND" or line.startswith("SEND "):
                payload = line[5:] if line.startswith("SEND ") else ""
                text = unescape_text(payload)[:MAX_TEXT]
                if peer_gone(sock):  # телефон уже ушёл — не слать в Telegram
                    log.info("%s: client gone before SEND, skipped", peer)
                    break
                status = tg_send_message(text)
                log.info("%s: SEND (%d chars) -> %s", peer, len(text), status)
                send_line(sock, "OK" if status == "ok" else f"ERR {status}")
            else:
                log.info("%s: unknown command: %r", peer, line[:64])
                send_line(sock, "ERR unknown")
    except (TimeoutError, ConnectionError, OSError) as exc:
        log.info("%s: connection lost (%s)", peer, exc.__class__.__name__)
    finally:
        with _clients_lock:
            _clients.discard(sock)
        try:
            sock.close()
        except OSError:
            pass
        log.info("%s: disconnected", peer)

# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def _close_all_clients() -> None:
    with _clients_lock:
        socks = list(_clients)
    for s in socks:
        try:
            s.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            s.close()
        except OSError:
            pass


def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s [%(threadName)s] %(message)s",
    )
    if not BOT_TOKEN:
        log.error("BOT_TOKEN is not set")
        raise SystemExit(1)

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((LISTEN_HOST, LISTEN_PORT))
    srv.listen(8)
    srv.settimeout(1.0)  # чтобы accept-цикл замечал shutdown
    log.info(
        "listening on %s:%d (require_code=%s, allowed_chat_id=%s, max_text=%d)",
        LISTEN_HOST, LISTEN_PORT,
        "yes" if REQUIRE_CODE else "no",
        ALLOWED_CHAT_ID if ALLOWED_CHAT_ID is not None else "first-writer",
        MAX_TEXT,
    )

    def request_stop(signum, _frame):
        log.info("signal %s received, shutting down", signal.Signals(signum).name)
        shutdown.set()
        try:
            srv.close()
        except OSError:
            pass
        _close_all_clients()

    signal.signal(signal.SIGTERM, request_stop)
    signal.signal(signal.SIGINT, request_stop)

    worker = threading.Thread(target=telegram_worker, name="tg-worker", daemon=True)
    worker.start()

    try:
        while not shutdown.is_set():
            try:
                conn, addr = srv.accept()
            except TimeoutError:
                continue
            except OSError:
                if shutdown.is_set():
                    break
                log.exception("accept failed")
                time.sleep(0.5)
                continue
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            with _clients_lock:
                _clients.add(conn)
            t = threading.Thread(
                target=handle_client, args=(conn, addr),
                name=f"client-{addr[0]}:{addr[1]}", daemon=True,
            )
            _client_threads.append(t)
            t.start()
            log.info("connection from %s:%d", addr[0], addr[1])
    finally:
        shutdown.set()
        try:
            srv.close()
        except OSError:
            pass
        _close_all_clients()
        deadline = time.monotonic() + 6.0
        for t in _client_threads:
            t.join(max(0.0, deadline - time.monotonic()))
        worker.join(2.0)
        log.info("stopped")


if __name__ == "__main__":
    main()
