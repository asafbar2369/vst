# Audio Quality Report

Status: **MEASURED** on the build described below. Values come from the automated suite
`Tests/TestMeasurements.cpp` (log: `docs/qa/linux-measurements-c891484.log`, 3,806 measurements; the run passed
864,978 checks, 0 failed). They describe VICE SYNTH X itself; no competitor or hardware was measured, so **no
comparative claim is made**. Earlier report: build 9104fdb (2026-11-20), 6 oscillator modes and 18 effects.

- Build: commit c891484, Linux x86_64 container, GCC, JUCE, Release with debug symbols (build/prof), single-threaded
  rendering on a shared cloud VM. CPU figures depend on this machine and are not a product specification.
- Date: 2026-11-22
- Reproduce: `VSX_TEST_ONLY="DSP measurements" VSX_MEASURE_SECTION=<section> ./ViceSynthXTests`
  (sections: filters, oscillators, effects, library, processor, state, longrun, matching); tables are produced by
  `tools/make_reports.py <outdir> <log>`.

## Key findings

| Area | Result | Assessment |
|---|---|---|
| Pitch, every engine (A4) | within -0.62 .. +0.59 cents (Analog saw 0.16 cents) | Accurate |
| Analog saw C7 inharmonic / aliasing | -36.4 dB (1x), -51.8 dB (2x), -53.8 dB (4x), -53.5 dB (8x) | Four-point BLEP; 1x (Eco) still aliases on very high notes |
| Band-limited engines, C6 below 20 kHz (2x) | Wavetable -86.3, Analog -83.1, Spectral -86.9, Additive -87.4, FM -80.3, Physical -75.9 dB | Clean. Full-band figures (about -32 to -37 dB) are energy at 20-24 kHz, the 2x decimator's transition band |
| Phase Dist, C6 below 20 kHz | -40.8 dB | Real aliasing of the steep PD shapes: the weakest engine (limitation; work in progress) |
| Modal, Granular | -31.4, -6.0 dB inharmonic | By design (material modes; grain cloud), not aliasing |
| Saw high-frequency response (C3) | 10 kHz harmonic -2.5 dB (1x), -0.6 dB (2x), -0.15 dB (4x), -0.04 dB (8x) vs ideal | As expected from oversampling |
| Filter slopes | LP24 attenuates 44-76 dB three octaves above cutoff | All nine models pass the slope guards |
| Self-oscillation (res = 1, cutoff 1 kHz) | MG 993, Diode 968, MS 1000, JP 991, JN 994, P5 981, Analog SVF 1000 Hz | Within 3.2 % of the cutoff |
| Acid Diode, SEM | no sustained self-oscillation at res = 1 | By design; documented |
| Drive / THD (94 Hz, 0.5 FS, drive 0.6) | 1.7-7.1 % depending on model | Lower than the 9104fdb build (13.7-29.9 %) after the gain-staging rework |
| Effects (25) | all finite; peaks <= -9.3 dBFS on the test pluck; level vs dry -3.6 .. +15.0 dB | Resonator +15.0 dB on a pluck at its tuned note (resonant build-up; its steady-state gain law measures -0.03 dB). Others within -3.6 .. +4.1 dB |
| Latency | 3 samples at every rate / block size tested | Reported to the host |
| Polyphony stress | 64 note-ons: 15 voices active (voice limit), peak -0.62 dBFS | Bounded |
| Library | 645 presets: level window -38.1 .. -19.7 dBFS, highest peak -6.04 dBFS | See FACTORY_LIBRARY_QA.md |

## Method notes

- Filter tests drive `AnalogFilter` directly at 96 kHz (the oversampled rate the engine uses at 48 kHz / High 2x).
  Gains are steady-state RMS ratios of sine tones (0.05 FS, drive 0). Self-oscillation frequency is the
  interpolated FFT peak of the last 170 ms after an impulse. THD sums harmonics 2-10.
- Aliasing: render at 48 kHz, Blackman-Harris FFT (32768 points) from 0.2 s; energy not within +/-4 bins of a harmonic,
  relative to harmonic energy. Saw C7: INIT patch (filter at 20 kHz). Engines C6: filters off, quality High (2x); the
  "below 20 kHz" figure counts inharmonic energy under 20 kHz only. Sample / Multisample need a loaded sample
  (measured in TestSamples).
- These are engineering measurements, not listening tests. No blind listening test has been run yet.

### Filters (internal rate 96 kHz, cutoff 1 kHz unless noted)

| Measurement | Value | Unit |
|---|---:|---|
| MG Ladder LP24 gain@fc rel 100Hz | -11.859 | dB |
| MG Ladder LP24 attenuation 3 oct above fc | -73.116 | dB |
| MG Ladder LP12 attenuation 3 oct above fc | -36.553 | dB |
| MG Ladder HP attenuation 3 oct below fc | -72.257 | dB |
| MG Ladder self-oscillation level (res=1) | -16.950 | dBFS |
| MG Ladder self-oscillation frequency (fc=1kHz) | 993.225 | Hz |
| MG Ladder THD (drive 0.6, 0.5 FS, 94 Hz) | 4.249 | % |
| MG Ladder peak under random modulation (noise in) | 3.403 | dBFS |
| Diode Ladder LP24 gain@fc rel 100Hz | -10.385 | dB |
| Diode Ladder LP24 attenuation 3 oct above fc | -45.020 | dB |
| Diode Ladder LP12 attenuation 3 oct above fc | -25.354 | dB |
| Diode Ladder HP attenuation 3 oct below fc | -47.479 | dB |
| Diode Ladder self-oscillation level (res=1) | -20.629 | dBFS |
| Diode Ladder self-oscillation frequency (fc=1kHz) | 968.134 | Hz |
| Diode Ladder THD (drive 0.6, 0.5 FS, 94 Hz) | 4.204 | % |
| Diode Ladder peak under random modulation (noise in) | 7.590 | dBFS |
| Acid Diode LP24 gain@fc rel 100Hz | -11.078 | dB |
| Acid Diode LP24 attenuation 3 oct above fc | -44.005 | dB |
| Acid Diode LP12 attenuation 3 oct above fc | -20.744 | dB |
| Acid Diode HP attenuation 3 oct below fc | -28.470 | dB |
| Acid Diode self-oscillation level (res=1) | -138.863 | dBFS |
| Acid Diode THD (drive 0.6, 0.5 FS, 94 Hz) | 4.204 | % |
| Acid Diode peak under random modulation (noise in) | 7.833 | dBFS |
| MS Sallen-Key LP24 gain@fc rel 100Hz | -8.848 | dB |
| MS Sallen-Key LP24 attenuation 3 oct above fc | -54.832 | dB |
| MS Sallen-Key LP12 attenuation 3 oct above fc | -36.550 | dB |
| MS Sallen-Key HP attenuation 3 oct below fc | -36.142 | dB |
| MS Sallen-Key self-oscillation level (res=1) | -12.165 | dBFS |
| MS Sallen-Key self-oscillation frequency (fc=1kHz) | 999.768 | Hz |
| MS Sallen-Key THD (drive 0.6, 0.5 FS, 94 Hz) | 5.666 | % |
| MS Sallen-Key peak under random modulation (noise in) | 4.212 | dBFS |
| JP OTA LP24 gain@fc rel 100Hz | -11.858 | dB |
| JP OTA LP24 attenuation 3 oct above fc | -73.116 | dB |
| JP OTA LP12 attenuation 3 oct above fc | -36.553 | dB |
| JP OTA HP attenuation 3 oct below fc | -72.260 | dB |
| JP OTA self-oscillation level (res=1) | -16.817 | dBFS |
| JP OTA self-oscillation frequency (fc=1kHz) | 991.389 | Hz |
| JP OTA THD (drive 0.6, 0.5 FS, 94 Hz) | 4.737 | % |
| JP OTA peak under random modulation (noise in) | 3.087 | dBFS |
| JN OTA LP24 gain@fc rel 100Hz | -14.446 | dB |
| JN OTA LP24 attenuation 3 oct above fc | -75.782 | dB |
| JN OTA LP12 attenuation 3 oct above fc | -39.219 | dB |
| JN OTA HP attenuation 3 oct below fc | -70.027 | dB |
| JN OTA self-oscillation level (res=1) | -17.477 | dBFS |
| JN OTA self-oscillation frequency (fc=1kHz) | 994.251 | Hz |
| JN OTA THD (drive 0.6, 0.5 FS, 94 Hz) | 3.072 | % |
| JN OTA peak under random modulation (noise in) | 8.166 | dBFS |
| P5 Cascade LP24 gain@fc rel 100Hz | -11.971 | dB |
| P5 Cascade LP24 attenuation 3 oct above fc | -73.340 | dB |
| P5 Cascade LP12 attenuation 3 oct above fc | -36.666 | dB |
| P5 Cascade HP attenuation 3 oct below fc | -72.038 | dB |
| P5 Cascade self-oscillation level (res=1) | -16.838 | dBFS |
| P5 Cascade self-oscillation frequency (fc=1kHz) | 980.635 | Hz |
| P5 Cascade THD (drive 0.6, 0.5 FS, 94 Hz) | 7.131 | % |
| P5 Cascade peak under random modulation (noise in) | 4.239 | dBFS |
| SEM State-Var LP24 gain@fc rel 100Hz | -9.435 | dB |
| SEM State-Var LP24 attenuation 3 oct above fc | -73.078 | dB |
| SEM State-Var LP12 attenuation 3 oct above fc | -36.552 | dB |
| SEM State-Var HP attenuation 3 oct below fc | -36.143 | dB |
| SEM State-Var self-oscillation level (res=1) | -132.579 | dBFS |
| SEM State-Var THD (drive 0.6, 0.5 FS, 94 Hz) | 2.572 | % |
| SEM State-Var peak under random modulation (noise in) | 10.753 | dBFS |
| Analog SVF LP24 gain@fc rel 100Hz | -9.437 | dB |
| Analog SVF LP24 attenuation 3 oct above fc | -73.078 | dB |
| Analog SVF LP12 attenuation 3 oct above fc | -36.552 | dB |
| Analog SVF HP attenuation 3 oct below fc | -36.143 | dB |
| Analog SVF self-oscillation level (res=1) | -21.150 | dBFS |
| Analog SVF self-oscillation frequency (fc=1kHz) | 999.884 | Hz |
| Analog SVF THD (drive 0.6, 0.5 FS, 94 Hz) | 1.743 | % |
| Analog SVF peak under random modulation (noise in) | 13.357 | dBFS |

### Oscillators (48 kHz)

| Measurement | Value | Unit |
|---|---:|---|
| Analog saw A4 pitch error | 0.164 | cents |
| Analog saw C7 inharmonic/aliasing energy, quality Eco (1x) | -36.365 | dB |
| Analog saw C7 inharmonic/aliasing energy, quality High (2x) | -51.836 | dB |
| Analog saw C7 inharmonic/aliasing energy, quality Ultra (4x) | -53.760 | dB |
| Analog saw C7 inharmonic/aliasing energy, quality Extreme (8x) | -53.476 | dB |
| Analog saw C3 harmonic near 5 kHz vs ideal, quality Eco (1x) | -0.615 | dB |
| Analog saw C3 harmonic near 10 kHz vs ideal, quality Eco (1x) | -2.487 | dB |
| Analog saw C3 harmonic near 15 kHz vs ideal, quality Eco (1x) | -5.807 | dB |
| Analog saw C3 harmonic near 5 kHz vs ideal, quality High (2x) | -0.153 | dB |
| Analog saw C3 harmonic near 10 kHz vs ideal, quality High (2x) | -0.615 | dB |
| Analog saw C3 harmonic near 15 kHz vs ideal, quality High (2x) | -1.415 | dB |
| Analog saw C3 harmonic near 5 kHz vs ideal, quality Ultra (4x) | -0.038 | dB |
| Analog saw C3 harmonic near 10 kHz vs ideal, quality Ultra (4x) | -0.153 | dB |
| Analog saw C3 harmonic near 15 kHz vs ideal, quality Ultra (4x) | -0.352 | dB |
| Analog saw C3 harmonic near 5 kHz vs ideal, quality Extreme (8x) | -0.010 | dB |
| Analog saw C3 harmonic near 10 kHz vs ideal, quality Extreme (8x) | -0.038 | dB |
| Analog saw C3 harmonic near 15 kHz vs ideal, quality Extreme (8x) | -0.088 | dB |
| mode Wavetable peak (init patch) | -19.041 | dBFS |
| mode Analog peak (init patch) | -17.765 | dBFS |
| mode Sample peak (init patch) | -68.598 | dBFS |
| mode Multisample peak (init patch) | -68.598 | dBFS |
| mode Granular peak (init patch) | -20.008 | dBFS |
| mode Spectral peak (init patch) | -17.733 | dBFS |
| mode FM peak (init patch) | -27.472 | dBFS |
| mode Physical peak (init patch) | -17.234 | dBFS |
| mode Modal peak (init patch) | -16.274 | dBFS |
| mode Phase Dist peak (init patch) | -17.226 | dBFS |
| mode Additive peak (init patch) | -19.926 | dBFS |
| engine Wavetable A4 pitch error | 0.409 | cents |
| engine Wavetable C6 inharmonic/aliasing energy, quality High (2x) | -31.971 | dB |
| engine Wavetable C6 inharmonic/aliasing energy below 20 kHz, quality High (2x) | -86.315 | dB |
| engine Analog A4 pitch error | 0.341 | cents |
| engine Analog C6 inharmonic/aliasing energy, quality High (2x) | -36.586 | dB |
| engine Analog C6 inharmonic/aliasing energy below 20 kHz, quality High (2x) | -83.104 | dB |
| engine Granular A4 pitch error | 0.587 | cents |
| engine Granular C6 inharmonic/aliasing energy, quality High (2x) | -5.953 | dB |
| engine Granular C6 inharmonic/aliasing energy below 20 kHz, quality High (2x) | -5.984 | dB |
| engine Spectral A4 pitch error | 0.406 | cents |
| engine Spectral C6 inharmonic/aliasing energy, quality High (2x) | -31.968 | dB |
| engine Spectral C6 inharmonic/aliasing energy below 20 kHz, quality High (2x) | -86.907 | dB |
| engine FM A4 pitch error | 0.070 | cents |
| engine FM C6 inharmonic/aliasing energy, quality High (2x) | -80.014 | dB |
| engine FM C6 inharmonic/aliasing energy below 20 kHz, quality High (2x) | -80.270 | dB |
| engine Physical A4 pitch error | 0.093 | cents |
| engine Physical C6 inharmonic/aliasing energy, quality High (2x) | -75.865 | dB |
| engine Physical C6 inharmonic/aliasing energy below 20 kHz, quality High (2x) | -75.865 | dB |
| engine Modal A4 pitch error | -0.617 | cents |
| engine Modal C6 inharmonic/aliasing energy, quality High (2x) | -31.353 | dB |
| engine Modal C6 inharmonic/aliasing energy below 20 kHz, quality High (2x) | -31.353 | dB |
| engine Phase Dist A4 pitch error | 0.301 | cents |
| engine Phase Dist C6 inharmonic/aliasing energy, quality High (2x) | -37.200 | dB |
| engine Phase Dist C6 inharmonic/aliasing energy below 20 kHz, quality High (2x) | -40.834 | dB |
| engine Additive A4 pitch error | 0.410 | cents |
| engine Additive C6 inharmonic/aliasing energy, quality High (2x) | -32.997 | dB |
| engine Additive C6 inharmonic/aliasing energy below 20 kHz, quality High (2x) | -87.423 | dB |

### Effects (C4 pluck through each effect, 48 kHz)

| Measurement | Value | Unit |
|---|---:|---|
| distortion level change vs dry (loudest 400 ms) | 4.082 | dB |
| distortion peak | -17.138 | dBFS |
| distortion tail level 1.8-2.3 s | -240.000 | dBFS |
| tube level change vs dry (loudest 400 ms) | -0.245 | dB |
| tube peak | -19.369 | dBFS |
| tube tail level 1.8-2.3 s | -240.000 | dBFS |
| tape level change vs dry (loudest 400 ms) | 2.675 | dB |
| tape peak | -16.809 | dBFS |
| tape tail level 1.8-2.3 s | -240.000 | dBFS |
| eq level change vs dry (loudest 400 ms) | 0.000 | dB |
| eq peak | -18.113 | dBFS |
| eq tail level 1.8-2.3 s | -240.000 | dBFS |
| mbcomp level change vs dry (loudest 400 ms) | 1.237 | dB |
| mbcomp peak | -11.820 | dBFS |
| mbcomp tail level 1.8-2.3 s | -240.000 | dBFS |
| freqshift level change vs dry (loudest 400 ms) | -0.351 | dB |
| freqshift peak | -15.103 | dBFS |
| freqshift tail level 1.8-2.3 s | -240.000 | dBFS |
| ringmod level change vs dry (loudest 400 ms) | -3.521 | dB |
| ringmod peak | -19.472 | dBFS |
| ringmod tail level 1.8-2.3 s | -240.000 | dBFS |
| chorus level change vs dry (loudest 400 ms) | -1.820 | dB |
| chorus peak | -17.354 | dBFS |
| chorus tail level 1.8-2.3 s | -240.000 | dBFS |
| dimension level change vs dry (loudest 400 ms) | 0.658 | dB |
| dimension peak | -14.358 | dBFS |
| dimension tail level 1.8-2.3 s | -240.000 | dBFS |
| flanger level change vs dry (loudest 400 ms) | -0.659 | dB |
| flanger peak | -17.440 | dBFS |
| flanger tail level 1.8-2.3 s | -240.000 | dBFS |
| phaser level change vs dry (loudest 400 ms) | 0.074 | dB |
| phaser peak | -17.302 | dBFS |
| phaser tail level 1.8-2.3 s | -240.000 | dBFS |
| stereodelay level change vs dry (loudest 400 ms) | -0.792 | dB |
| stereodelay peak | -18.843 | dBFS |
| stereodelay tail level 1.8-2.3 s | -74.240 | dBFS |
| tapedelay level change vs dry (loudest 400 ms) | -0.733 | dB |
| tapedelay peak | -18.843 | dBFS |
| tapedelay tail level 1.8-2.3 s | -71.786 | dBFS |
| digitaldelay level change vs dry (loudest 400 ms) | -0.387 | dB |
| digitaldelay peak | -18.843 | dBFS |
| digitaldelay tail level 1.8-2.3 s | -100.705 | dBFS |
| reverb level change vs dry (loudest 400 ms) | -0.378 | dB |
| reverb peak | -18.843 | dBFS |
| reverb tail level 1.8-2.3 s | -75.132 | dBFS |
| convolution level change vs dry (loudest 400 ms) | -1.795 | dB |
| convolution peak | -18.843 | dBFS |
| convolution tail level 1.8-2.3 s | -240.000 | dBFS |
| stereo level change vs dry (loudest 400 ms) | 0.000 | dB |
| stereo peak | -18.113 | dBFS |
| stereo tail level 1.8-2.3 s | -240.000 | dBFS |
| limiter level change vs dry (loudest 400 ms) | -0.133 | dB |
| limiter peak | -18.491 | dBFS |
| limiter tail level 1.8-2.3 s | -240.000 | dBFS |
| bbdchorus level change vs dry (loudest 400 ms) | -3.579 | dB |
| bbdchorus peak | -17.614 | dBFS |
| bbdchorus tail level 1.8-2.3 s | -85.502 | dBFS |
| bbddelay level change vs dry (loudest 400 ms) | -0.044 | dB |
| bbddelay peak | -18.843 | dBFS |
| bbddelay tail level 1.8-2.3 s | -81.318 | dBFS |
| shimmer level change vs dry (loudest 400 ms) | -0.772 | dB |
| shimmer peak | -18.843 | dBFS |
| shimmer tail level 1.8-2.3 s | -57.958 | dBFS |
| rotary level change vs dry (loudest 400 ms) | -2.119 | dB |
| rotary peak | -14.858 | dBFS |
| rotary tail level 1.8-2.3 s | -240.000 | dBFS |
| resonator level change vs dry (loudest 400 ms) | 15.017 | dB |
| resonator peak | -9.303 | dBFS |
| resonator tail level 1.8-2.3 s | -125.525 | dBFS |
| transient level change vs dry (loudest 400 ms) | 0.000 | dB |
| transient peak | -18.113 | dBFS |
| transient tail level 1.8-2.3 s | -240.000 | dBFS |
| spectral level change vs dry (loudest 400 ms) | 0.226 | dB |
| spectral peak | -19.925 | dBFS |
| spectral tail level 1.8-2.3 s | -240.000 | dBFS |

### Processor realtime factor and peak (Ocean Drive Poly, 6-note chord)

| Measurement | Value | Unit |
|---|---:|---|
| 44100 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 6.024 | x realtime |
| 44100 Hz / 32 peak | -3.498 | dBFS |
| 44100 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 4.321 | x realtime |
| 44100 Hz / 64 peak | -3.498 | dBFS |
| 44100 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 4.912 | x realtime |
| 44100 Hz / 128 peak | -3.498 | dBFS |
| 44100 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 5.618 | x realtime |
| 44100 Hz / 256 peak | -3.498 | dBFS |
| 44100 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 6.404 | x realtime |
| 44100 Hz / 512 peak | -3.498 | dBFS |
| 44100 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 4.427 | x realtime |
| 44100 Hz / 1024 peak | -3.498 | dBFS |
| 48000 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 4.989 | x realtime |
| 48000 Hz / 32 peak | -3.251 | dBFS |
| 48000 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 5.490 | x realtime |
| 48000 Hz / 64 peak | -3.251 | dBFS |
| 48000 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 5.521 | x realtime |
| 48000 Hz / 128 peak | -3.251 | dBFS |
| 48000 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 5.390 | x realtime |
| 48000 Hz / 256 peak | -3.251 | dBFS |
| 48000 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 5.908 | x realtime |
| 48000 Hz / 512 peak | -3.251 | dBFS |
| 48000 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 5.554 | x realtime |
| 48000 Hz / 1024 peak | -3.251 | dBFS |
| 88200 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 2.665 | x realtime |
| 88200 Hz / 32 peak | -3.261 | dBFS |
| 88200 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 3.198 | x realtime |
| 88200 Hz / 64 peak | -3.261 | dBFS |
| 88200 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 3.693 | x realtime |
| 88200 Hz / 128 peak | -3.261 | dBFS |
| 88200 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 2.974 | x realtime |
| 88200 Hz / 256 peak | -3.261 | dBFS |
| 88200 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 2.946 | x realtime |
| 88200 Hz / 512 peak | -3.261 | dBFS |
| 88200 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 3.084 | x realtime |
| 88200 Hz / 1024 peak | -3.261 | dBFS |
| 96000 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 2.505 | x realtime |
| 96000 Hz / 32 peak | -2.915 | dBFS |
| 96000 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 2.556 | x realtime |
| 96000 Hz / 64 peak | -2.915 | dBFS |
| 96000 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 2.348 | x realtime |
| 96000 Hz / 128 peak | -2.915 | dBFS |
| 96000 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 2.539 | x realtime |
| 96000 Hz / 256 peak | -2.915 | dBFS |
| 96000 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 2.245 | x realtime |
| 96000 Hz / 512 peak | -2.915 | dBFS |
| 96000 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 2.511 | x realtime |
| 96000 Hz / 1024 peak | -2.915 | dBFS |
| 192000 Hz / 32 realtime factor (6 voices, quality High 2x, this machine) | 1.234 | x realtime |
| 192000 Hz / 32 peak | -2.731 | dBFS |
| 192000 Hz / 64 realtime factor (6 voices, quality High 2x, this machine) | 1.340 | x realtime |
| 192000 Hz / 64 peak | -2.731 | dBFS |
| 192000 Hz / 128 realtime factor (6 voices, quality High 2x, this machine) | 1.359 | x realtime |
| 192000 Hz / 128 peak | -2.731 | dBFS |
| 192000 Hz / 256 realtime factor (6 voices, quality High 2x, this machine) | 1.296 | x realtime |
| 192000 Hz / 256 peak | -2.731 | dBFS |
| 192000 Hz / 512 realtime factor (6 voices, quality High 2x, this machine) | 1.359 | x realtime |
| 192000 Hz / 512 peak | -2.731 | dBFS |
| 192000 Hz / 1024 realtime factor (6 voices, quality High 2x, this machine) | 1.219 | x realtime |
| 192000 Hz / 1024 peak | -2.731 | dBFS |

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
| peak with 64 held notes | -0.619 | dBFS |

### State

| Measurement | Value | Unit |
|---|---:|---|
| state size | 22546.000 | bytes |
### Automation

| Measurement | Value | Unit |
|---|---:|---|
| peak under random automation (6000 changes) | -0.087 | dBFS |
### Long run

| Measurement | Value | Unit |
|---|---:|---|
| audio rendered | 300.000 | s |
| peak | -1.019 | dBFS |
| realtime factor | 7.372 | x realtime |
