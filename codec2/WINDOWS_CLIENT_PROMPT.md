# Prompt for implementing the Nokia N70 publisher on Windows

Copy the text below into a new coding-agent session opened in this repository
on the Windows computer with the Symbian SDK. The Python server is already
implemented. Its wire protocol is the compatibility contract for the phone.
The first deliverable is a measured encoder feasibility prototype for an FPU-less
phone, before a complete streaming client. Read [N70_FEASIBILITY.md](N70_FEASIBILITY.md).

Before testing on the phone, configure the server's reachable IPv4/hostname,
TCP port, and publishing token in the phone application. These are runtime
settings: do not commit real server credentials in source code.

```text
Implement a native Nokia N70 microphone publisher in this repository.
The server is already implemented in Python under codec2/. Work on the
phone application now and preserve compatibility with the existing server.

First read:
- SKILL.md: repository-specific S60 2nd FP3 build instructions.
- codec2/README.md: architecture, protocol, capture and porting details.
- codec2/N70_FEASIBILITY.md: CPU gate and fixed-point investigation.
- codec2/common.py and codec2/proxy.py: the actual wire-format implementation.
- codec2/send_pcm.py and codec2/test_proxy.py: working publisher examples.
- TgPager/build.cmd, TgPager/group/tgpager.mmp, and the existing socket engine.

Target: Nokia N70, S60 2nd Edition FP3, Symbian OS 8.1a.
Discover the installed SDK and compatible Perl paths on this Windows machine.
Do not assume the old hardcoded build.cmd paths still match this machine.
Use the SDK's supported device toolchain and build a release SIS for N70.

Critical constraint: treat N70 as a software floating-point target. Upstream's
ordinary Codec 2 implementation explicitly expects a hardware FPU for real-time
operation. A successful C port or server test is not evidence that N70 encoding
can keep up. Do not assume 220 MHz is enough or copy Cortex-M-with-FPU timings.

FIRST DELIVERABLE: a minimal, correct encoder benchmark in a release device
build, using known PCM already in memory. Measure mean/high-percentile/max encode
time and compare total processing time with audio duration. The 3200 mode has
a 20ms frame budget and needs spare time for capture and networking. If no phone
is attached, produce the benchmark SIS and exact measurement instructions;
keep the CPU gate explicitly unverified.

Do not invest in the full streaming UI/transport integration until the CPU gate
passes. If the baseline misses its budget, profile it and investigate an optimized
or fixed-point encoder preserving the existing 3200 bitstream. Codec2-mod is an
optimization candidate but still uses floats; its published FPU benchmarks do
not answer this question. Experimental fixed-point implementations are research
leads and require independent correctness, toolchain, and throughput evaluation.

There is no whole-codec fixed-point compiler switch. KISS FFT's FIXED_POINT
option only affects FFT types. A real conversion needs scaling, normalization,
saturation, intermediate-width analysis, stable LPC/LSP calculations, and
validated replacements for the nonlinear math actually used. Do not replace
float keywords with integers, enable unsupported VFP/NEON, assume access to an
internal DSP, or call an encoder fixed-point while hot paths still use soft float.

Create a separate native application under AudioPublisher/ with its own UID,
resources, build script, packaging, and README. Use the existing HelloWorld
or SDK example as a starting point. Keep the current Telegram app functional.

Required behavior:
1. Settings for server IPv4/hostname, TCP port (default 7778), and publishing
   token. Persist settings locally, without logging the token.
2. Explicit Start and Stop controls and visible connection/recording state.
3. Open an outbound TCP connection and authenticate before starting capture.
4. Capture the microphone as signed PCM16, 8000 Hz, mono, using the FP3 SDK's
   CMdaAudioInputStream and its asynchronous callbacks.
5. Encode on the phone with Codec 2 mode 3200. The server decodes; the phone
   does not need playback or a return audio path.
6. Transmit continuously with bounded queues and asynchronous socket writes.
7. Stop capture, cancel pending operations, close the socket, and free resources
   safely on Stop/Exit. Reconnect after network failures using timed backoff;
   do not retry bad credentials forever.

Use upstream Codec 2 release 1.2.0 as the numerical/bitstream reference for the
baseline port and any subsequent fixed-point adaptation. Preserve the speech
algorithm and documented parameter encoding. Retain its license/notices, record
the exact revision and adaptation patches, and include all speech dependencies
and generated codebook sources. Modern Linux/ELF libraries cannot be linked
directly into the old Symbian executable. Address C99 and math/runtime issues
for this SDK, with a narrow C/C++ compatibility boundary.

Wire protocol, which MUST remain compatible with the Python server:
- Send ASCII: C2LIVE 1 <publishing-token> 3200 followed by one actual LF byte.
- No CR, no BOM; exactly one space between fields.
- Wait for OK followed by LF. Handle ERR auth and ERR busy replies.
- Then send binary records: 12-byte header followed by packed codec bytes.
- Header is network byte order: uint32 sequence, uint32 first_sample,
  uint16 frame_count, uint16 payload_bytes. Do not send a native padded struct.
- Codec 2 3200 consumes 160 signed samples and emits 8 bytes per 20 ms frame.
  CODEC2_MODE_3200 is mode identifier zero, NOT integer 3200.
- Normally batch 5 frames: frame_count=5, payload_bytes=40, audio duration=100ms.
- The first record in each TCP session has sequence=0 and first_sample=0.
  Subsequent normal records are (1,800,5,40), (2,1600,5,40), etc.
- frame_count must be 1..10; payload_bytes must equal frame_count*8.
- Counters wrap as unsigned 32-bit values. first_sample counts 8kHz capture
  samples, not milliseconds or bytes. Account for dropped audio in timestamps.
- No base64, WAV headers, Telegram framing, or per-record acknowledgement.
- On reconnect use a fresh encoder and reset both counters to zero.

Correct ownership/concurrency is essential:
- Capture descriptor storage must outlive ReadL and callback use.
- Accumulate arbitrary callback sizes; extract complete 320-byte PCM inputs.
  Preserve leftover bytes and ensure short-sample alignment.
- Keep callbacks short; schedule encoding or use a worker thread if measured
  encode time blocks capture. A worker does not fix insufficient CPU capacity.
- Keep one socket write outstanding. Preserve its descriptor and bytes unchanged
  until completion/cancellation. Never drop the middle of an in-flight record.
- Bound unsent records (initial target: five 100ms records); drop complete old
  unsent records and preserve timestamp gaps. Reconnect on prolonged write stalls.
- Avoid cross-thread heap ownership mistakes and per-frame allocation churn.
- Do not sleep or perform synchronous network I/O on the UI scheduler thread.

Implement and verify incrementally:
1. Build/install the minimal encoder benchmark with the discovered toolchain.
2. Verify packed outputs against the server/reference and measure on N70.
   If too slow, complete the optimization/fixed-point investigation first;
   do not mark streaming feasible while the CPU gate is failing or unmeasured.
3. After the gate passes, record clean microphone PCM and validate it on a desktop.
4. Send prerecorded/pre-encoded audio from the phone using the documented
   protocol to separate transport debugging from live capture debugging.
5. Connect live capture, encoding, and networking; test with codec2/proxy.py
   and the existing browser listener.

Acceptance:
- Produce a buildable native project, a device SIS when the SDK is available,
  and reproducible build/install/test instructions.
- Confirm phone-generated frames decode with the existing Python server.
- Confirm browser listening, Stop/Start, reconnect, bounded queues, and cleanup.
- Report actual measurements separately from estimates. If encoding cannot
  keep up in real time, provide the benchmark evidence and remaining work.
- Do not claim device tests or real-time feasibility without device results.
- Do not silently switch to raw PCM or move encoding to the server: this task
  specifically requires Codec 2 encoding on the N70.

Implement the work, not just a proposal. If physical phone verification is
unavailable, finish the buildable benchmark, reference-vector checks, and device
measurement instructions on this Windows machine. State that the CPU gate and
full live client remain outstanding; do not skip the gate to produce a UI-only
implementation or claim that server tests prove real-time phone encoding.
```
