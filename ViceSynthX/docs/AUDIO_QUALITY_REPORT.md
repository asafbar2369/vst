# Audio Quality Report

Status: **MEASURED** on the build described below. Values come from the automated suite
`Tests/TestMeasurements.cpp` (log: `docs/qa/linux-test-run-9104fdb.log`). They describe
VICE SYNTH X itself; no competitor or hardware was measured, so **no comparative claim is made**.

- Test environment: Linux x86_64 container (Ubuntu 24.04), Intel Xeon @ 2.10 GHz (4 vCPU, shared cloud VM), GCC 13.3, JUCE 9.0.6, Release + LTO, single-threaded audio rendering
- Date: 2026-11-20
- Reproduce: `VSX_TEST_ONLY="DSP measurements" VSX_MEASURE_SECTION=filters ./ViceSynthXTests`
  (sections: filters, oscillators, effects, library, processor, state, longrun, matching)

## Key findings

| Area | Result | Assessment |
|---|---|---|
| Oscillator pitch (A4, analog saw) | +0.16 cents | Accurate |
| Aliasing / inharmonic energy, C7 saw | -27.0 dB (1x), -38.8 dB (2x), -45.5 dB (4x) | Improves with oversampling as designed; 1x (Eco) is audibly aliased on very high notes |
| Filter slopes | LP24 attenuates 44-76 dB three octaves above cutoff; LP12 >15 dB; HP >15 dB | All nine models pass the slope guards |
| Self-oscillation (res = 1, cutoff 1 kHz) | MG 993 Hz, MS 1000 Hz, JP 991 Hz, JN 994 Hz, P5 968 Hz, Analog SVF 1000 Hz | Tracks cutoff within 3.2 % |
| Diode Ladder self-oscillation | 2998 Hz at cutoff 1 kHz | By design (cutoff calibrated to small-signal brightness, see HARDWARE_MODEL_ACCURACY.md); listed as a limitation |
| Acid Diode, SEM | no sustained self-oscillation at res = 1 (Acid: only with Character raised) | By design; documented |
| Drive / THD (94 Hz, 0.5 FS, drive 0.6) | 13.7-29.9 % depending on model | SEM / Analog SVF were 0.0 % before the fix in this release (Drive had no timbral effect) |
| Stability | No non-finite output; bounded peaks under random cutoff / resonance / drive modulation at 16-sample rate | Pass |
| Effects | All 18 effects finite and bounded on a test pluck | Pass (see table: convolution and level-changing effects) |

## Method notes

- Filter tests drive `AnalogFilter` directly at 96 kHz (the oversampled rate the engine uses at 48 kHz / High 2x).
  Gains are steady-state RMS ratios of sine tones (0.05 FS, drive 0). Self-oscillation frequency is the
  interpolated FFT peak of the last 170 ms after an impulse. THD sums harmonics 2-10.
- Aliasing: 1 s render at 48 kHz, Blackman-Harris FFT (32768 points); energy not within +/-4 bins of a harmonic of
  2093 Hz, relative to harmonic energy. Includes the filter (cutoff 20 kHz) and the anti-imaging filters.
- These are engineering measurements, not listening tests. No blind listening test has been run yet.

### Filters (internal rate 96 kHz, cutoff 1 kHz unless noted)

| Measurement | Value | Unit |
|---|---:|---|
| MG Ladder LP24 gain@fc rel 100Hz | -11.860 | dB |
| MG Ladder LP24 attenuation 3 oct above fc | -73.117 | dB |
| MG Ladder LP12 attenuation 3 oct above fc | -36.554 | dB |
| MG Ladder HP attenuation 3 oct below fc | -72.250 | dB |
| MG Ladder self-oscillation level (res=1) | -20.045 | dBFS |
| MG Ladder self-oscillation frequency (fc=1kHz) | 993.225 | Hz |
| MG Ladder THD (drive 0.6, 0.5 FS, 94 Hz) | 20.746 | % |
| MG Ladder peak under random modulation (noise in) | 0.316 | dBFS |
| Diode Ladder LP24 gain@fc rel 100Hz | -10.384 | dB |
| Diode Ladder LP24 attenuation 3 oct above fc | -45.020 | dB |
| Diode Ladder LP12 attenuation 3 oct above fc | -25.354 | dB |
| Diode Ladder HP attenuation 3 oct below fc | -47.478 | dB |
| Diode Ladder self-oscillation level (res=1) | -24.434 | dBFS |
| Diode Ladder self-oscillation frequency (fc=1kHz) | 2997.991 | Hz |
| Diode Ladder THD (drive 0.6, 0.5 FS, 94 Hz) | 20.712 | % |
| Diode Ladder peak under random modulation (noise in) | 4.572 | dBFS |
| Acid Diode LP24 gain@fc rel 100Hz | -11.077 | dB |
| Acid Diode LP24 attenuation 3 oct above fc | -44.004 | dB |
| Acid Diode LP12 attenuation 3 oct above fc | -20.744 | dB |
| Acid Diode HP attenuation 3 oct below fc | -28.469 | dB |
| Acid Diode self-oscillation level (res=1) | -130.097 | dBFS |
| Acid Diode THD (drive 0.6, 0.5 FS, 94 Hz) | 20.711 | % |
| Acid Diode peak under random modulation (noise in) | 5.351 | dBFS |
| MS Sallen-Key LP24 gain@fc rel 100Hz | -8.846 | dB |
| MS Sallen-Key LP24 attenuation 3 oct above fc | -54.828 | dB |
| MS Sallen-Key LP12 attenuation 3 oct above fc | -36.546 | dB |
| MS Sallen-Key HP attenuation 3 oct below fc | -18.075 | dB |
| MS Sallen-Key self-oscillation level (res=1) | -18.186 | dBFS |
| MS Sallen-Key self-oscillation frequency (fc=1kHz) | 999.768 | Hz |
| MS Sallen-Key THD (drive 0.6, 0.5 FS, 94 Hz) | 29.945 | % |
| MS Sallen-Key peak under random modulation (noise in) | -1.701 | dBFS |
| JP OTA LP24 gain@fc rel 100Hz | -11.859 | dB |
| JP OTA LP24 attenuation 3 oct above fc | -73.117 | dB |
| JP OTA LP12 attenuation 3 oct above fc | -36.554 | dB |
| JP OTA HP attenuation 3 oct below fc | -72.258 | dB |
| JP OTA self-oscillation level (res=1) | -19.346 | dBFS |
| JP OTA self-oscillation frequency (fc=1kHz) | 991.448 | Hz |
| JP OTA THD (drive 0.6, 0.5 FS, 94 Hz) | 20.685 | % |
| JP OTA peak under random modulation (noise in) | 0.288 | dBFS |
| JN OTA LP24 gain@fc rel 100Hz | -14.446 | dB |
| JN OTA LP24 attenuation 3 oct above fc | -75.782 | dB |
| JN OTA LP12 attenuation 3 oct above fc | -39.220 | dB |
| JN OTA HP attenuation 3 oct below fc | -70.026 | dB |
| JN OTA self-oscillation level (res=1) | -19.305 | dBFS |
| JN OTA self-oscillation frequency (fc=1kHz) | 994.107 | Hz |
| JN OTA THD (drive 0.6, 0.5 FS, 94 Hz) | 13.677 | % |
| JN OTA peak under random modulation (noise in) | 6.515 | dBFS |
| P5 Cascade LP24 gain@fc rel 100Hz | -12.258 | dB |
| P5 Cascade LP24 attenuation 3 oct above fc | -73.897 | dB |
| P5 Cascade LP12 attenuation 3 oct above fc | -36.948 | dB |
| P5 Cascade HP attenuation 3 oct below fc | -71.481 | dB |
| P5 Cascade self-oscillation level (res=1) | -21.627 | dBFS |
| P5 Cascade self-oscillation frequency (fc=1kHz) | 968.241 | Hz |
| P5 Cascade THD (drive 0.6, 0.5 FS, 94 Hz) | 26.314 | % |
| P5 Cascade peak under random modulation (noise in) | 1.755 | dBFS |
| SEM State-Var LP24 gain@fc rel 100Hz | -9.435 | dB |
| SEM State-Var LP24 attenuation 3 oct above fc | -73.078 | dB |
| SEM State-Var LP12 attenuation 3 oct above fc | -36.552 | dB |
| SEM State-Var HP attenuation 3 oct below fc | -36.143 | dB |
| SEM State-Var self-oscillation level (res=1) | -132.579 | dBFS |
| SEM State-Var THD (drive 0.6, 0.5 FS, 94 Hz) | 17.215 | % |
| SEM State-Var peak under random modulation (noise in) | 8.849 | dBFS |
| Analog SVF LP24 gain@fc rel 100Hz | -9.437 | dB |
| Analog SVF LP24 attenuation 3 oct above fc | -73.078 | dB |
| Analog SVF LP12 attenuation 3 oct above fc | -36.552 | dB |
| Analog SVF HP attenuation 3 oct below fc | -36.143 | dB |
| Analog SVF self-oscillation level (res=1) | -18.138 | dBFS |
| Analog SVF self-oscillation frequency (fc=1kHz) | 999.591 | Hz |
| Analog SVF THD (drive 0.6, 0.5 FS, 94 Hz) | 13.202 | % |
| Analog SVF peak under random modulation (noise in) | 11.194 | dBFS |

### Oscillators (48 kHz)

| Measurement | Value | Unit |
|---|---:|---|
| Analog saw A4 pitch error | 0.158 | cents |
| Analog saw C7 inharmonic/aliasing energy, quality Eco (1x) | -27.036 | dB |
| Analog saw C7 inharmonic/aliasing energy, quality High (2x) | -38.734 | dB |
| Analog saw C7 inharmonic/aliasing energy, quality Ultra (4x) | -45.352 | dB |
| mode Wavetable peak (init patch) | -11.045 | dBFS |
| mode Analog peak (init patch) | -10.144 | dBFS |
| mode Sample peak (init patch) | -77.233 | dBFS |
| mode Multisample peak (init patch) | -72.752 | dBFS |
| mode Granular peak (init patch) | -11.866 | dBFS |
| mode Spectral peak (init patch) | -9.909 | dBFS |

### Effects (C4 pluck through each effect, 48 kHz)

| Measurement | Value | Unit |
|---|---:|---|
| distortion level change vs dry (loudest 400 ms) | 3.875 | dB |
| distortion peak | -9.342 | dBFS |
| distortion tail level 1.8-2.3 s | -240.000 | dBFS |
| tube level change vs dry (loudest 400 ms) | -0.433 | dB |
| tube peak | -11.551 | dBFS |
| tube tail level 1.8-2.3 s | -240.000 | dBFS |
| tape level change vs dry (loudest 400 ms) | 2.626 | dB |
| tape peak | -8.689 | dBFS |
| tape tail level 1.8-2.3 s | -240.000 | dBFS |
| eq level change vs dry (loudest 400 ms) | -0.064 | dB |
| eq peak | -10.458 | dBFS |
| eq tail level 1.8-2.3 s | -240.000 | dBFS |
| mbcomp level change vs dry (loudest 400 ms) | 0.824 | dB |
| mbcomp peak | -3.501 | dBFS |
| mbcomp tail level 1.8-2.3 s | -240.000 | dBFS |
| freqshift level change vs dry (loudest 400 ms) | -0.351 | dB |
| freqshift peak | -6.555 | dBFS |
| freqshift tail level 1.8-2.3 s | -240.000 | dBFS |
| ringmod level change vs dry (loudest 400 ms) | -3.479 | dB |
| ringmod peak | -11.669 | dBFS |
| ringmod tail level 1.8-2.3 s | -240.000 | dBFS |
| chorus level change vs dry (loudest 400 ms) | 0.870 | dB |
| chorus peak | -6.096 | dBFS |
| chorus tail level 1.8-2.3 s | -240.000 | dBFS |
| dimension level change vs dry (loudest 400 ms) | 0.664 | dB |
| dimension peak | -6.073 | dBFS |
| dimension tail level 1.8-2.3 s | -240.000 | dBFS |
| flanger level change vs dry (loudest 400 ms) | 2.104 | dB |
| flanger peak | -6.791 | dBFS |
| flanger tail level 1.8-2.3 s | -240.000 | dBFS |
| phaser level change vs dry (loudest 400 ms) | 2.869 | dB |
| phaser peak | -6.191 | dBFS |
| phaser tail level 1.8-2.3 s | -240.000 | dBFS |
| stereodelay level change vs dry (loudest 400 ms) | 0.163 | dB |
| stereodelay peak | -10.537 | dBFS |
| stereodelay tail level 1.8-2.3 s | -63.463 | dBFS |
| tapedelay level change vs dry (loudest 400 ms) | 0.201 | dB |
| tapedelay peak | -10.544 | dBFS |
| tapedelay tail level 1.8-2.3 s | -61.106 | dBFS |
| digitaldelay level change vs dry (loudest 400 ms) | 0.690 | dB |
| digitaldelay peak | -10.502 | dBFS |
| digitaldelay tail level 1.8-2.3 s | -89.791 | dBFS |
| reverb level change vs dry (loudest 400 ms) | 2.172 | dB |
| reverb peak | -9.086 | dBFS |
| reverb tail level 1.8-2.3 s | -61.960 | dBFS |
| convolution level change vs dry (loudest 400 ms) | -1.134 | dB |
| convolution peak | -10.536 | dBFS |
| convolution tail level 1.8-2.3 s | -240.000 | dBFS |
| stereo level change vs dry (loudest 400 ms) | -0.049 | dB |
| stereo peak | -10.480 | dBFS |
| stereo tail level 1.8-2.3 s | -240.000 | dBFS |
| limiter level change vs dry (loudest 400 ms) | -0.176 | dB |
| limiter peak | -10.794 | dBFS |
| limiter tail level 1.8-2.3 s | -240.000 | dBFS |
