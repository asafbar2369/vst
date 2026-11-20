# CPU and Real-Time Benchmark Report

Status: **MEASURED** (single machine, single run per configuration; repeated runs vary by roughly +/-5 % on this shared VM).
No competitor software was benchmarked (none is available in this environment), so **no comparative CPU claim is made**.

- Environment: Linux x86_64 container (Ubuntu 24.04), Intel Xeon @ 2.10 GHz (4 vCPU, shared cloud VM), GCC 13.3, JUCE 9.0.6, Release + LTO, single-threaded audio rendering
- Workload: factory preset *Ocean Drive Poly* (2 analog oscillators with 6 + 3 unison voices = 54 oscillator voices
  for a 6-note chord, JN OTA filter, dimension chorus, algorithmic reverb), quality High (2x oversampling).
- Realtime factor = audio duration / processing time on one core (higher is better; 1.0 = exactly real time).

## Optimisation history (same workload, 48 kHz / 256)

| Build | Realtime factor | Change |
|---|---:|---|
| 59eed18 (before) | 4.65x | baseline |
| 9104fdb | 5.22x | shared per-sample filter coefficients, single evaluation of oscillator shapes, pure-shape snapping |

Profile (callgrind, steady state inside processBlock, before optimisation): analog oscillator 39 %, OTA filter 18 %,
oscillator mixing loop 12 %, voice tick 6 %, envelopes 4 %, reverb 2 %.

## Interpretation

- Latency: 3 samples reported at every tested sample rate and buffer size with quality High (2x); this is the
  oversampling filters' delay. Eco and Ultra were not measured in this run.
- 192 kHz with 2x oversampling (384 kHz internal) runs at about 1.2x realtime for this heavy chord on this machine:
  usable, but with little headroom. Use Eco or fewer unison voices at very high sample rates.
- Not yet implemented: SIMD processing of unison voices, multi-core voice rendering, AVX2 code paths (see DEVELOPMENT_PRIORITIES.md).
- Memory: the voice pool (40 voices, ~1.07 MB) is allocated once when the engine is created; no allocation happens in the
  audio callback for normal playback (measured by code review, not yet by an allocation-tracking test).

### Processor realtime factor and peak (Ocean Drive Poly, 6-note chord)

| Measurement | Value | Unit |
|---|---:|---|
| 44100 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 6.159 | x realtime |
| 44100 Hz / 32 peak | -0.087 | dBFS |
| 44100 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 5.960 | x realtime |
| 44100 Hz / 64 peak | -0.087 | dBFS |
| 44100 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 6.088 | x realtime |
| 44100 Hz / 128 peak | -0.087 | dBFS |
| 44100 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 6.128 | x realtime |
| 44100 Hz / 256 peak | -0.087 | dBFS |
| 44100 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 6.141 | x realtime |
| 44100 Hz / 512 peak | -0.087 | dBFS |
| 44100 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 6.120 | x realtime |
| 44100 Hz / 1024 peak | -0.087 | dBFS |
| 48000 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 5.488 | x realtime |
| 48000 Hz / 32 peak | -0.087 | dBFS |
| 48000 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 5.604 | x realtime |
| 48000 Hz / 64 peak | -0.087 | dBFS |
| 48000 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 5.709 | x realtime |
| 48000 Hz / 128 peak | -0.087 | dBFS |
| 48000 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 5.220 | x realtime |
| 48000 Hz / 256 peak | -0.087 | dBFS |
| 48000 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 5.713 | x realtime |
| 48000 Hz / 512 peak | -0.087 | dBFS |
| 48000 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 5.720 | x realtime |
| 48000 Hz / 1024 peak | -0.087 | dBFS |
| 88200 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 2.578 | x realtime |
| 88200 Hz / 32 peak | -0.087 | dBFS |
| 88200 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 2.879 | x realtime |
| 88200 Hz / 64 peak | -0.087 | dBFS |
| 88200 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 3.051 | x realtime |
| 88200 Hz / 128 peak | -0.087 | dBFS |
| 88200 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 2.992 | x realtime |
| 88200 Hz / 256 peak | -0.087 | dBFS |
| 88200 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 3.152 | x realtime |
| 88200 Hz / 512 peak | -0.087 | dBFS |
| 88200 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 3.181 | x realtime |
| 88200 Hz / 1024 peak | -0.087 | dBFS |
| 96000 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 2.827 | x realtime |
| 96000 Hz / 32 peak | -0.087 | dBFS |
| 96000 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 2.865 | x realtime |
| 96000 Hz / 64 peak | -0.087 | dBFS |
| 96000 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 2.822 | x realtime |
| 96000 Hz / 128 peak | -0.087 | dBFS |
| 96000 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 2.888 | x realtime |
| 96000 Hz / 256 peak | -0.087 | dBFS |
| 96000 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 2.788 | x realtime |
| 96000 Hz / 512 peak | -0.087 | dBFS |
| 96000 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 2.693 | x realtime |
| 96000 Hz / 1024 peak | -0.087 | dBFS |
| 192000 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 1.415 | x realtime |
| 192000 Hz / 32 peak | -0.087 | dBFS |
| 192000 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 1.384 | x realtime |
| 192000 Hz / 64 peak | -0.087 | dBFS |
| 192000 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 1.407 | x realtime |
| 192000 Hz / 128 peak | -0.087 | dBFS |
| 192000 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 1.425 | x realtime |
| 192000 Hz / 256 peak | -0.087 | dBFS |
| 192000 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 1.456 | x realtime |
| 192000 Hz / 512 peak | -0.087 | dBFS |
| 192000 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 1.369 | x realtime |
| 192000 Hz / 1024 peak | -0.087 | dBFS |

### Reported latency

| Measurement | Value | Unit |
|---|---:|---|
| 44100 Hz / 32 reported latency | 3.000 | samples |
| 44100 Hz / 64 reported latency | 3.000 | samples |
| 44100 Hz / 128 reported latency | 3.000 | samples |
| 44100 Hz / 256 reported latency | 3.000 | samples |
| 44100 Hz / 512 reported latency | 3.000 | samples |
| 44100 Hz / 1024 reported latency | 3.000 | samples |
| 48000 Hz / 32 reported latency | 3.000 | samples |
| 48000 Hz / 64 reported latency | 3.000 | samples |
| 48000 Hz / 128 reported latency | 3.000 | samples |
| 48000 Hz / 256 reported latency | 3.000 | samples |
| 48000 Hz / 512 reported latency | 3.000 | samples |
| 48000 Hz / 1024 reported latency | 3.000 | samples |
| 88200 Hz / 32 reported latency | 3.000 | samples |
| 88200 Hz / 64 reported latency | 3.000 | samples |
| 88200 Hz / 128 reported latency | 3.000 | samples |
| 88200 Hz / 256 reported latency | 3.000 | samples |
| 88200 Hz / 512 reported latency | 3.000 | samples |
| 88200 Hz / 1024 reported latency | 3.000 | samples |
| 96000 Hz / 32 reported latency | 3.000 | samples |
| 96000 Hz / 64 reported latency | 3.000 | samples |
| 96000 Hz / 128 reported latency | 3.000 | samples |
| 96000 Hz / 256 reported latency | 3.000 | samples |
| 96000 Hz / 512 reported latency | 3.000 | samples |
| 96000 Hz / 1024 reported latency | 3.000 | samples |
| 192000 Hz / 32 reported latency | 3.000 | samples |
| 192000 Hz / 64 reported latency | 3.000 | samples |
| 192000 Hz / 128 reported latency | 3.000 | samples |
| 192000 Hz / 256 reported latency | 3.000 | samples |
| 192000 Hz / 512 reported latency | 3.000 | samples |
| 192000 Hz / 1024 reported latency | 3.000 | samples |

### Voice allocation

| Measurement | Value | Unit |
|---|---:|---|
| active voices after 64 note-ons | 15.000 | voices |
| peak with 64 held notes | -0.087 | dBFS |

### State

| Measurement | Value | Unit |
|---|---:|---|
| state size | 18881.000 | bytes |
### Automation

| Measurement | Value | Unit |
|---|---:|---|
| peak under random automation (6000 changes) | -0.087 | dBFS |
### Long run

| Measurement | Value | Unit |
|---|---:|---|
| audio rendered | 300.000 | s |
| peak | -0.087 | dBFS |
| realtime factor | 6.437 | x realtime |
