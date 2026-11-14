# Codec 2 encoding on the N70: feasibility comes first

**The Python server works with Codec 2 frames, but a real-time N70 encoder has
not been demonstrated.** The phone application must be treated as a software
floating-point target. Server tests on a modern Mac say nothing about its speed.

The first phone deliverable is a correct, measured encoder benchmark. A complete
streaming UI and transport integration should follow a successful CPU gate.

## What the sources actually establish

The upstream README's microcontroller section explicitly states that real-time
operation requires a hardware FPU. This is a substantial constraint on using
the ordinary implementation on N70, rather than a minor build adjustment.
[Upstream microcontroller requirements](https://github.com/drowe67/codec2#building-codec-2-for-microcontrollers)

The pinned 1.2.0 sources use floating-point operations throughout speech analysis,
not only at the public PCM boundary. Examples include the pitch estimator in
`nlp.c`, LPC analysis in `lpc.c`, and spectral calculations/quantization in
`sine.c` and `quantise.c`. Changing the PCM input to integers does not change
those internal operations.
[Reference sources](https://github.com/drowe67/codec2/tree/1.2.0/src)

KISS FFT has a `FIXED_POINT` option, but that option controls its FFT scalar type.
It does not supply a fixed-point implementation of the complete Codec 2 encoder.
Enabling it in isolation is not a valid conversion of the surrounding floating-
point interfaces and scaling.
[KISS FFT types](https://github.com/drowe67/codec2/blob/1.2.0/src/kiss_fft.h)

The [M17 Codec2-mod](https://github.com/M17-Project/Codec2-mod) project is a useful
candidate for evaluation: it extracts and optimizes mode 3200. Its README also
says removing floating-point math was not its goal, and its speed measurements
use an STM32 with its FPU enabled. Those results cannot establish N70 feasibility.

A [September 2026 announcement by Bruce Perens](https://sourceforge.net/p/freetel/mailman/freetel-codec2/thread/CAK2MWOsZh2nQSVrwOJN2euruAdUPe%3Doeo1SFN4nSNJ15-wpntQ%40mail.gmail.com/)
describes an experimental fixed-point Rust reimplementation. This is a research
lead, not a verified Symbian C implementation or evidence of N70 throughput.
Its current code, numerical behavior, interoperability, licensing, and possible
translation to the old toolchain need independent evaluation before adoption.

## Three different technical claims

| Claim | Current evidence |
| --- | --- |
| The server can decode mode 3200 and serve browsers | Exercised with native libcodec2 and integration tests |
| Codec 2 sources can be made to build for this Symbian SDK | Porting work still required |
| The N70 can continuously encode microphone audio fast enough | Not measured; floating-point requirements are a major risk |

Do not infer the third claim from either of the first two. A successful build
with software floating-point helpers establishes executability, not throughput.

## Phase A: measure a baseline on the phone

Build a minimal release-device benchmark that reads a known PCM16/8000/mono
recording into memory and repeatedly calls the mode-3200 encoder. Do not include
microphone capture, networking, browser work, or file reads in the timed calls.

Record the exact compiler, optimization settings, math/runtime implementation,
codec revision, device/firmware, stack/heap sizes, and timer resolution. Request
code compatible with the actual N70 CPU and bundled SDK ABI. Do not copy modern
ARM compiler flags into the old toolchain or enable unavailable VFP/NEON code.

Compare the total encoding time with input duration and collect per-frame mean,
high-percentile, and maximum timings. Each mode-3200 frame contains 20 ms of
audio. Sustained encoding above 20 ms/frame is below real time; sustained results
only slightly below it leave little room for capture, networking, and the UI.
The guide's 10-12 ms/frame figure is a desired engineering margin, not an N70
measurement or a guarantee that software floating point can achieve it.

Validate saved packed frames with the existing server decoder and compare
reference vectors. Report CPU time and correctness together. A faster broken
analysis or quantizer is not an acceptable port.

If a physical N70 is unavailable, prepare the buildable benchmark and its SIS,
document the exact device procedure, and leave the throughput gate unverified.
An SDK emulator or host compiler cannot supply the missing device measurement.

## Phase B: optimize or investigate fixed point

Profile the measured implementation before choosing a rewrite. Potential work
includes reducing accidental double-precision calculations, precomputing fixed
tables, and removing unused work. Benchmark each change with the same vectors.
These measures may help; they do not guarantee sufficient speed without an FPU.

For a fixed-point encoder, the work includes:

- Selecting representations and scaling for windows, filtered samples, FFT
  values, correlations, LPC/LSP parameters, energy, and pitch calculations.
- Providing bounded intermediate precision, saturation, and normalization.
- Handling divisions, roots, logarithms, and trigonometric/root-search operations.
- Preserving numerical stability, especially for silence and low-energy speech.
- Keeping the mode-3200 bitstream layout and parameter meanings interoperable
  with the existing server decoder.

Q15 or Q31 is a representation choice for a specific quantity, not a global
replacement for the C `float` keyword. Wide integer operations may also be
expensive; measure the generated code on the target. Test with speech, silence,
near-clipping input, and background noise. Compare packed outputs and decoded
quality against an identified reference; explain numerical differences.

Keep a reference implementation and isolated tests for each converted kernel.
Replacing only the FFT is not evidence that the complete encode path is integer.
Likewise, software floating-point helpers still perform floating-point arithmetic.

Do not assume the phone's internal multimedia/DSP hardware is available for an
arbitrary custom codec. There is no demonstrated SDK integration for that here.

## Phase C: integrate only after the CPU gate

Once encoding is correct and has measured headroom on N70, add microphone capture,
the bounded asynchronous TCP writer, and the Start/Stop UI. Repeat the benchmark
with capture and networking active. Check long-run queue depth and end-to-end
delay; a worker thread or a larger buffer cannot compensate for a sustained
encoding deficit.

The existing Python server can remain the decoder for an interoperable fixed-
point mode-3200 encoder. Different internal arithmetic does not itself require
a different network protocol. If parameter encoding or frame layout is changed,
it is a protocol/codec change and needs coordinated server work.

If no implementation passes the gate, report that result. Do not claim the live
Codec 2 use case has been achieved. A different codec or offline voice messages
would be a separate design decision, not an implicit substitution.
