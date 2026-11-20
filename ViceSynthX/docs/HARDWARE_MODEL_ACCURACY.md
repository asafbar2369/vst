# Hardware Model Accuracy

Classification used (from the brief):
1. **Measured hardware emulation**: fitted to measurements of the original unit.
2. **Circuit-informed simulation**: topology and nonlinearity derived from the circuit / published analyses, not fitted to a unit.
3. **Algorithmic approximation**: a generic structure tuned by ear / design targets.
4. **Musical inspiration**: only the character is referenced.

**No VICE SYNTH X model is a measured hardware emulation.** No original hardware, service-manual measurements or legally
available hardware recordings were used. Product names below are trademarks of their owners and are used only to describe
the inspiration; nothing is claimed to be authentic, exact, identical or component-accurate.

## Filters

| VICE model | Inspiration | Implementation | Class | Measured behaviour (AUDIO_QUALITY_REPORT.md) |
|---|---|---|---|---|
| MG Ladder | Moog-style transistor ladder | 4 nonlinear TPT one-pole stages (tanh), global feedback, zero-delay solution, bass compensation | 2 | LP24 -73 dB at 3 oct; self-oscillation 993 Hz @ 1 kHz; THD 20.7 % (drive 0.6) |
| Diode Ladder | Diode ladder (e.g. EMS / Roland-style) | coupled 4-stage RC network solved implicitly per sample (secant conductances); cutoff calibrated to geometric mean of w180 and w3dB | 2 | resonance peak 2998 Hz @ 1 kHz setting (by calibration); LP24 -45 dB |
| Acid Diode | TB-303-style diode ladder with unequal first capacitor | same solver, C1 ratio 0.55, k_osc 21.5 (numerically derived) | 2 | no self-oscillation at Character 0; LP24 -44 dB |
| MS Sallen-Key | Korg MS-20 (Korg35) | two-pole Sallen-Key with clipping feedback (Pirkle-style TPT) | 2 | self-oscillation 1000 Hz; HP ~6 dB/oct measured; THD 29.9 % |
| JP OTA | Roland Jupiter-style IR3109 OTA cascade | 4 OTA stages (tanh of input difference), 12/24 dB taps | 3 | self-oscillation 991 Hz |
| JN OTA | Roland Juno-style 80017A cascade + HPF | 4 OTA stages, softer feedback saturation | 3 | self-oscillation 994 Hz; THD 13.7 % |
| P5 Cascade | Sequential Prophet-5 rev.3 (CEM3320)-style | 4 OTA stages, cubic feedback saturation, asymmetry | 3 | self-oscillation 968 Hz (3.2 % low) |
| SEM State-Var | Oberheim SEM | TPT SVF, saturating integrator, input saturation, damping limited (no self-oscillation) | 3 | THD 17.2 % |
| Analog SVF | generic analog state-variable | TPT SVF, negative damping allowed, saturating integrator | 3 | self-oscillation 1000 Hz; THD 13.2 % |

Per-voice component tolerance (stage-cutoff spread, feedback gain and offsets) and slow oscillator drift are modelled as
random variation; they are **not** derived from measured unit-to-unit spread.

## Effects and vintage digital character

| Effect | Inspiration | Class |
|---|---|---|
| Dimension chorus | Roland Dimension-D style multi-mode chorus | 4 |
| Tape delay / tape saturation | tape echo units | 3 |
| Algorithmic reverb | FDN (Hadamard) reverb | 3 |
| Convolution reverb | user / generated impulse responses | exact convolution of the loaded IR; generated IRs are synthetic |
| Frequency shifter | single-sideband (Hilbert, Niemitalo coefficients) | 3 |
| Others (distortion, tube, EQ, multiband compressor, ring modulator, flanger, phaser, stereo / digital delay, imager, limiter) | generic studio processors | 3 |

AMS RMX16, classic digital reverbs, Roland D-50 / JD-800 / JV-1080, Yamaha DX7 / SY77 / SY99, Korg M1 / Wavestation,
Fairlight and Synclavier were **not** modelled. Presets in the "Vintage 1980s / 1990s" categories are musical inspiration only
(class 4), built from original synthesis.

## What would be needed for class 1 or 2 claims
- Access to the original units (or legally licensed recordings), with matched-condition measurements: frequency response
  at several cutoff / resonance settings, self-oscillation tuning and amplitude, THD / IMD versus level, modulation response.
- Published schematics / service manuals for the exact revision, and a parameter fit with error statistics.
These are listed as P3 research items in DEVELOPMENT_PRIORITIES.md.
