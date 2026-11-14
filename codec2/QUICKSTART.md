# Run the Python audio server

The server accepts one Codec 2 microphone publisher over TCP and streams decoded
audio to browser listeners. You can test the complete server path on a Mac now,
then implement the N70 application on Windows using
[WINDOWS_CLIENT_PROMPT.md](WINDOWS_CLIENT_PROMPT.md).
The N70 encoder is still unproven: its floating-point limitation requires a
[device benchmark and possibly fixed-point work](N70_FEASIBILITY.md) before
the full phone client. Running this server does not establish phone feasibility.

## Install on macOS

Use Python 3.11+ and Homebrew. From this repository:

```sh
brew install codec2
cd codec2
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
export CODEC2_LIB="$(brew --prefix codec2)/lib/libcodec2.dylib"
```

The [Homebrew Codec 2 formula](https://formulae.brew.sh/formula/codec2) provides
the native codec. The networking and browser service are Python; compression
and decoding use this native library through ctypes.

## Install on a Linux server instead

For Debian/Ubuntu:

```sh
sudo apt-get update
sudo apt-get install python3-venv libcodec2-dev
cd /path/to/s60-tgpager/codec2
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
```

Linux normally discovers the shared library automatically. If it does not, set
`CODEC2_LIB` to its absolute path. The [full guide](README.md#5-building-the-pinned-codec-2-reference)
also explains building the native library from source without a system install.

## Initialize and start

From `codec2/`, in the activated virtual environment:

```sh
python proxy.py --init-config config.toml
python proxy.py --check
python proxy.py
```

Initialization creates `config.toml` with different random publishing and
listening tokens. It never overwrites an existing file. On macOS/Linux the file
is created with owner-only permissions and is ignored by Git. Keep it locally;
do not put it in the Windows client prompt or commit its tokens.

The default configuration file is `codec2/config.toml`, next to the script,
regardless of your working directory. For another file:

```sh
python proxy.py --config /path/to/private-settings.toml
```

`--check` validates configuration and performs native Codec 2 encoding/decoding
without opening any ports. Ctrl+C stops both listeners and active connections.

Open **http://127.0.0.1:8080/** on the server computer. Copy `listen_token`
from the config file into the page and click **Listen**. It waits for a publisher.

The public health endpoint is **http://127.0.0.1:8080/healthz**. It returns service
readiness without tokens or details about the microphone stream.

## Test without a phone or an audio file

In a second terminal, from `codec2/`:

```sh
. .venv/bin/activate
# macOS: repeat this if you have not saved codec2_lib in config.toml.
export CODEC2_LIB="$(brew --prefix codec2)/lib/libcodec2.dylib"
python send_pcm.py --config config.toml --tone 10
```

On Linux omit the Homebrew export. The sender generates a repeating 220 Hz tone
with short pauses, encodes it to Codec 2, and publishes it for ten seconds. You
should hear it on the browser page. This checks transport and playback; evaluate
speech quality with a spoken recording.

For a recording, install FFmpeg, convert it, and send:

```sh
ffmpeg -i speech.wav -ac 1 -ar 8000 -c:a pcm_s16le -f s16le speech-8k.pcm
python send_pcm.py --config config.toml speech-8k.pcm
```

To test a server on another computer, explicitly supply its address:

```sh
python send_pcm.py --config config.toml --host SERVER_IPV4 --tone 10
```

The sender reads credentials and the TCP port from the config. Its destination
defaults to loopback; a bind address such as `0.0.0.0` is not a remote destination.
Avoid copying the full server config to an untrusted sender: environment-only
mode with `C2_PUBLISH_TOKEN` and `--host`/`--port` is also supported.

## Configuration

All TOML settings are at the top level. Environment variables override file
values. Without a file, the earlier environment-only invocation still works.

| TOML key | Environment variable | Default / purpose |
| --- | --- | --- |
| `publish_token` | `C2_PUBLISH_TOKEN` | Required; put this in the N70's settings |
| `listen_token` | `C2_LISTEN_TOKEN` | Required and different; enter this in the browser |
| `tcp_host` | `C2_TCP_HOST` | `127.0.0.1`; use `0.0.0.0` to accept remote phones |
| `tcp_port` | `C2_TCP_PORT` | `7778` |
| `http_host` | `C2_HTTP_HOST` | `127.0.0.1` |
| `http_port` | `C2_HTTP_PORT` | `8080` |
| `viewer_origin` | `C2_VIEWER_ORIGIN` | Empty = HTTP service's origin; set public HTTPS origin behind a reverse proxy |
| `codec2_lib` | `CODEC2_LIB` | Empty = automatic discovery; otherwise a library path |
| `max_tcp_connections` | `C2_MAX_TCP_CONNECTIONS` | `16`, including publishers still authenticating |
| `max_viewers` | `C2_MAX_VIEWERS` | `8`, including WebSockets still authenticating |

On a Mac, you can save the absolute library path in `codec2_lib` to avoid exporting
it in each terminal. Find the prefix with `brew --prefix codec2` and append
`/lib/libcodec2.dylib`. Use an absolute path rather than a working-directory-relative
path. No SDK or Windows tools are needed on the Python server.

## Make the server reachable over the Internet

For a publicly reachable Linux server, edit the generated config:

```toml
tcp_host = "0.0.0.0"
tcp_port = 7778
http_host = "127.0.0.1"
http_port = 8080
viewer_origin = "https://audio.example.com"
```

Keep the generated token values in the file and replace the example domain with
your domain. Restart the service after changing settings. Allow inbound TCP
7778 and put an HTTPS reverse proxy in front of port 8080; the full guide contains
an [Nginx configuration](README.md#18-internet-deployment).

For a trusted-LAN test, `http_host = "0.0.0.0"` lets another computer open
`http://SERVER_LAN_IPV4:8080/`. Leave `viewer_origin` empty for that direct setup.
An ordinary home Mac is not automatically reachable from the cellular Internet:
use a reachable server or configure routing/port forwarding explicitly.

The browser uses HTTP/WS locally and HTTPS/WSS behind the reverse proxy. The
current N70 TCP transport is plaintext, including its publishing token. Browser
HTTPS does not encrypt that leg; use a trusted/protected network for this version.

## Compatibility contract for the Windows implementation

The phone needs the server's reachable host, TCP port, and publishing token.
It sends **Codec 2 mode 3200**, PCM source 8000 Hz/mono/16-bit, using the
`C2LIVE 1` handshake and binary records documented in the
[full guide](README.md#6-phone-to-proxy-protocol-handshake).

Do not connect the phone to the HTTP port or to the existing Telegram port 7777.
The complete future-client prompt is in
[WINDOWS_CLIENT_PROMPT.md](WINDOWS_CLIENT_PROMPT.md).

## Run server tests

With the virtual environment active and the native library available, run from
the repository root:

```sh
python -m unittest discover -s codec2 -p 'test_*.py' -v
```

The checks use real native encoding/decoding and loopback HTTP/TCP/WebSockets.
They cover configuration, authentication, packet fragmentation and truncation,
multiple listeners, capacity limits, timestamp gaps, sender pacing, and shutdown.
Browser audibility and cellular-network behavior still require a listening test.
