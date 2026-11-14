# Как Nokia N70 обменивается сообщениями через интернет: разбираем код пейджера

В TgPager сообщение с кнопочной Nokia проходит через два сетевых участка: телефон соединяется с собственным TCP-прокси, а прокси обращается к Telegram. На телефоне работает C++ под Symbian 8.1a, на сервере — Python 3.

```text
Nokia N70                         Python-прокси              Telegram
┌──────────────────┐             ┌─────────────┐
│ C++ / Symbian    │── TCP ──────►│ tg_proxy.py │── HTTPS ──► Bot API
│ приём и отправка │◄─────────────│             │◄──────────
└──────────────────┘             └─────────────┘
       порт сервера по умолчанию: 7777
```

Рассмотрим этот путь по исходникам. Фрагменты ниже показывают отдельные методы и участки функций; это объяснение существующего приложения, а не самостоятельная программа.

## 1. Приложение получает доступ к сетевому стеку Symbian

Сначала движок открывает сессию локального сервера сокетов и создаёт два объекта соединений:

```cpp
void CTgEngine::ConstructL()
    {
    User::LeaveIfError(iServ.Connect());
    iPoll = new (ELeave) CTgPollConn(iServ, *this);
    iSend = new (ELeave) CTgSendConn(iServ, *this);
    }
```

`iServ` имеет тип `RSocketServ`. Его `Connect()` соединяет приложение **с системной службой на телефоне**. Соединение с удалённым прокси будет открыто позже через `RSocket`.

А где настройка мобильного интернета? В TgPager нет собственных полей APN, логина оператора и выбора IAP — точки доступа Symbian. Нет и явного запуска соединения через `RConnection`. Клиент использует неявное подключение через сетевые API; выбор доступа остаётся за системой и её настройками. Такой способ через `RSocket` или `RHostResolver` описан в [сохранённой документации Nokia по ESock](https://docs.huihoo.com/symbian/s60-5th-edition-cpp-developers-library-v2.1/GUID-35228542-8C95-4849-A73F-2B4F082F0C44/sdk/doc_source/guide/Communications-Infrastructure-subsystem-guide/esock/SocketsClientOverview.guide.html).

Поэтому на телефоне должен быть настроен рабочий доступ к сети. Как именно система предложит выбрать точку доступа на конкретной прошивке, приложение не определяет. В настройках самого пейджера задаются адрес прокси, порт и код доступа.

## 2. Найти адрес сервера и открыть TCP-сокет

Адрес из настроек сначала пробуют разобрать как числовой IP. Если это получилось, DNS не нужен:

```cpp
TInetAddr addr;
if (addr.Input(iHost) == KErrNone)
    {
    addr.SetPort(iPort);
    return ConnectAddr(addr);
    }
```

Если вместо IP указано доменное имя, запускается разрешение имени:

```cpp
TInt err = iResolver.Open(iServ, KAfInet, KProtocolInetUdp);
if (err != KErrNone)
    return err;
iState = EResolve;
iResolver.GetByName(iHost, iNameEntry, iStatus);
SetActive();
```

`RHostResolver` получает адрес по имени. Параметр `KProtocolInetUdp` здесь относится к открытию резолвера: сами сообщения пейджера передаются по TCP.

После успешного DNS-запроса адрес извлекается из `iNameEntry` и передаётся в тот же `ConnectAddr()`:

```cpp
TInt CTgConn::ConnectAddr(TInetAddr& aAddr)
    {
    TInt err = iSock.Open(iServ, KAfInet, KSockStream, KProtocolInetTcp);
    if (err != KErrNone)
        return err;
    iSockOpen = ETrue;
    iState = EConnect;
    iSock.Connect(aAddr, iStatus);
    SetActive();
    return KErrNone;
    }
```

`KAfInet` выбирает семейство интернет-адресов, `KSockStream` — потоковый сокет, `KProtocolInetTcp` — TCP. Именно `iSock.Connect()` начинает соединение с адресом и портом прокси.

Возвращённый `KErrNone` в конце метода означает, что асинхронная операция запущена. Её окончательный результат появится в `iStatus`.

## 3. Дождаться сети, сохранив управление интерфейсом

`CTgConn` наследуется от `CActive`. Он запускает асинхронную операцию и вызывает `SetActive()`. Когда операция завершается, active scheduler вызывает `RunL()`.

Внутри `RunL()` состояние `EConnect` обрабатывается так:

```cpp
case EConnect:
    if (err != KErrNone)
        {
        CloseSocket();
        iState = EIdle;
        OnError(err, EFalse);
        return;
        }
    StartAuth();
    break;
```

Таким образом, код не ждёт завершения TCP-подключения в блокирующем цикле. Пока сеть выполняет запрос, планировщик может обрабатывать другие события приложения.

После подключения клиент передаёт первую строку протокола:

```cpp
void CTgConn::StartAuth()
    {
    iTx8.Zero();
    iTx8.Append(_L8("AUTH "));
    iTx8.Append(iCode);
    iTx8.Append(_L8("\n"));
    iState = EAuthW;
    iSock.Write(iTx8, iStatus);
    SetActive();
    }
```

Прокси проверяет код и возвращает `OK` либо `ERR auth`. Это код доступа к собственному серверу; токен Telegram-бота хранится только на сервере. Для кода доступа в этой реализации следует использовать ASCII: в отличие от текста сообщения, он не проходит через UTF-8-кодировщик.

## 4. Два соединения: ждать сообщение и отправлять ответ

У приёмного и отправляющего каналов разные задачи:

| Канал | Последовательность |
| --- | --- |
| `CTgPollConn` | Подключение → `AUTH` → повторяющиеся `POLL` |
| `CTgSendConn` | Подключение → `AUTH` → один `SEND` → ответ → закрытие |

Два TCP-сокета позволяют отправлять текст, пока приёмный канал ждёт данные. Они не означают, что приложению обязательно нужны два независимых подключения к мобильному оператору.

На проводе приём выглядит так. Стрелки показывают направление, каждая команда и ответ заканчиваются LF:

```text
N70 → прокси: AUTH example-code
прокси → N70: OK
N70 → прокси: POLL
             ...ожидание сообщения до 50 секунд...
прокси → N70: MSG Привет, Nokia!
прокси → N70: END
N70 → прокси: POLL
```

`POLL` реализует long polling: сервер отвечает при появлении сообщения либо по истечении ожидания. После `END` телефон сразу повторяет запрос по тому же соединению. Прокси отвечает в уже открытый телефоном канал, поэтому подключаться к Nokia извне не требуется.

Чтение тоже асинхронное:

```cpp
TInt CTgConn::IssueRead()
    {
    if (IsActive())
        return KErrInUse;
    iState = EBusyR;
    iSock.RecvOneOrMore(iRx8, 0, iStatus, iXfr);
    SetActive();
    return KErrNone;
    }
```

TCP возвращает произвольные фрагменты байтов, поэтому один `RecvOneOrMore()` не равен одному сообщению. Метод `Feed()` накапливает данные до `\n`, а затем передаёт готовую строку обработчику. Строка `MSG` декодируется из UTF-8 в UTF-16 и через движок передаётся интерфейсу.

При отправке выполняется обратное преобразование. Участок `CTgSendConn::Send()` готовит полезную нагрузку:

```cpp
iPayload.Zero();
iPayload.Append(_L8("SEND "));
TBuf8<2048> enc;
if (!TgUtf::Encode(aText, enc))
    return KErrTooBig;
if (iPayload.Length() + enc.Length() > iPayload.MaxLength())
    return KErrTooBig;
iPayload.Append(enc);
```

`TgUtf::Encode()` кодирует текст в UTF-8 и экранирует обратные слеши и переводы строк. После авторизации команда передаётся через `IssueWrite()`, который добавляет завершающий LF. Текст с несколькими строками остаётся одной строкой протокола.

## 5. Прокси превращает SEND в HTTPS-запрос

Телефон не формирует HTTP-запрос к Telegram. Прокси разбирает `SEND`, восстанавливает текст и вызывает `sendMessage`. Внутри `tg_call()` создаётся обычный POST с JSON:

```python
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
```

По умолчанию `TG_API_BASE` равен `https://api.telegram.org`. HTTPS и JSON обрабатывает Python на сервере; публичный Bot API требует HTTPS согласно [документации Telegram](https://core.telegram.org/bots/api#making-requests).

Для входящих сообщений отдельный поток прокси вызывает `getUpdates` с ожиданием до 25 секунд. Полученные тексты попадают в общую очередь и будят обработчик телефонного `POLL`. Эти два ожидания выполняются параллельно: 25 и 50 секунд не складываются в обязательную задержку доставки.

Ответ `OK` после `SEND` означает успешный вызов Telegram API. Подтверждения прочтения собеседником в протоколе нет.

## 6. Что происходит при обрыве

После обрабатываемой сетевой ошибки приёмного канала `CTgPollConn::OnError()` отмечает потерю связи и запускает паузу:

```cpp
iRetry = ETrue;
iEngine.PollDown();
IssueWait(10000000);
```

Аргумент задан в микросекундах: это 10 секунд. Внутри `IssueWait()` работает асинхронный `RTimer`; после его завершения канал повторяет подключение. При неверном коде доступа автоматические попытки прекращаются.

Эти 10 секунд отсчитываются после обнаруженной ошибки. Отдельного прикладного таймаута для каждой сетевой операции на телефоне нет, поэтому обещать обнаружение любого зависания за 10 секунд нельзя.

Участок Nokia → прокси передаёт текст и код доступа без шифрования. Прокси обеспечивает HTTPS только на своём участке до Telegram. Это прямое следствие выбранной архитектуры, которое нужно учитывать при использовании пейджера.

Исходники: [сетевой движок Nokia](https://github.com/alexex1993/s60-tgpager/blob/421714a/TgPager/src/tgpagerengine.cpp), [UTF-8-кодировщик](https://github.com/alexex1993/s60-tgpager/blob/421714a/TgPager/inc/tgutf.h), [Python-прокси](https://github.com/alexex1993/s60-tgpager/blob/421714a/server/tg_proxy.py).
