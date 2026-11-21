# Reverb redesign: investigation and measurements (2026-11-21)

The owner reported the reverb as harsh and metallic after listening on Windows. This report covers what was wrong with
the old algorithm, what replaced it, and what was measured. All numbers come from `Tests/TestReverb.cpp`
(`VSX_TEST_ONLY="Reverb measurements"`, 48 kHz, default settings: Size 60 %, Decay 3 s, Damping 40 %,
Diffusion 75 %, Modulation 30 %, Low Cut 120 Hz, pre-delay 0 for the impulse responses). No listening test has been done
yet. These are objective proxies, not a perceptual verdict.

## What was wrong with the old reverb
`Tests/LegacyReverb.h` keeps the previous algorithm for comparison. It was an 8-line FDN with:
- **A narrow spread of loop lengths:** 1116-1617 samples, ratio 1.45:1. At Size 0 these shrank to about 7-10 ms, which
  is the classic recipe for comb / flutter colouration. Measured comb periodicity: **0.205**, the highest of everything
  measured.
- **No early reflections and no diffusion inside the loop.**
- **One common sine LFO for all lines.**
- **A first-order wet low cut:** strong sub-bass content remained, with a peak near 30 Hz despite the 120 Hz low cut
  (see `measurements/reverb/reverb_spectrum.png`).
- **Unfiltered extreme highs:** a flat response to 20 kHz (-17 dB at 20 kHz re 1 kHz). This is digital "fizz".

## New design (`Source/FX/ReverbEngine.*`)
- **16-line FDN, eight programs:** Hall, Plate, Room, Chamber, Ambient and Gated use the network. Nonlinear and Reverse
  are shaped dense multi-tap responses.
- **Loop lengths:**
  - spread geometrically over a 3.2:1 to 4.4:1 range per program;
  - rounded to primes;
  - smoothed when Size changes, so there are no jumps.
- **Diffusion:** an all-pass inside every loop, plus four Dattorro-style input diffusers per channel.
- **Two-band absorption per line (Jot):**
  - the low band decays with Decay;
  - the high band decays with Decay x an HF ratio set by Damping and the program.
- **Mixing:** a 16 x 16 Walsh-Hadamard feedback matrix and orthogonal Walsh output taps for decorrelated stereo.
- **Modulation:** each line has its own smoothed-random modulation at 0.23-1.1 Hz.
- **Program-specific early reflections.**
- **Output filters:** a second-order low cut, and a second-order high cut that follows Damping.
- **Gated:** a hold-and-release gate keyed from the dry input. Hold time is set by Decay.
- **Loudness:** each program's gain is calibrated to the impulse-response energy of the old reverb at the defaults,
  so switching programs or versions does not jump in level.
- **Program selector:** a new **Program** parameter (`fx_reverb_program`). It is registered after Low Cut, so existing
  parameter indices before it are unchanged.

## Measurements

| Program | RT60 500 Hz (s) | RT60 4 kHz (s) | Mixing time (ms) | Comb periodicity | Late spectral persistence | CPU (x real time) |
|---|---|---|---|---|---|---|
| Legacy (before) | 2.80 | 1.26 | 58 | **0.205** | 0.161 | - |
| Hall | 2.83 | 1.69 | 94 | 0.056 | 0.181 | 55.6 |
| Plate | 2.87 | 1.83 | 40 | 0.123 | 0.356 | 48.8 |
| Room | 2.83 | 1.66 | 32 | 0.111 | 0.424 | 46.8 |
| Chamber | 2.87 | 1.72 | 50 | 0.128 | 0.309 | 46.4 |
| Ambient | 2.92 | 1.71 | 94 | 0.064 | 0.106 | 44.8 |
| Nonlinear | 0.41 | 0.45 | 34 | 0.034 | 0.266 | 191.9 |
| Gated | 0.56 | 0.61 | 30 | 0.115 | 0.000 | 45.2 |
| Reverse | 0.29 | 0.29 | 74 | 0.082 | -0.025 | 187.1 |

What the metrics mean:
- **RT60:** Schroeder T30 (x2) in octave bands.
- **Mixing time:** when the Abel-Huang normalised echo density first reaches 0.9.
- **Comb periodicity:** the highest normalised autocorrelation of the 20-400 ms response at lags of 1-50 ms. Flutter
  and comb repetition raise it; this is the metric that targets "metallic".
- **Late spectral persistence:** the correlation of the detrended log spectra of two consecutive late windows.
  Persistent narrow modes raise it.

Interpreting the results honestly:
- **Comb periodicity:** every new program is lower than the legacy reverb, by 1.6x (Chamber) to 6x (Nonlinear). The
  test asserts at least 20 % lower.
- **Late spectral persistence (mixed result):**
  - Hall and Ambient are close to or below the legacy value.
  - Plate, Room and Chamber are higher (0.31-0.42). Their loops are physically shorter, so their modal density is lower,
    and the legacy reverb's heavy modulation (about 22 samples of common sine sweep) smears its modes more.
  - The test only asserts < 0.6 for these programs. Raising modulation further would trade ringing for audible
    chorusing; this is a candidate for listening-based tuning.
- **High-frequency decay:**
  - The 4 kHz RT60 is 0.58-0.64 of the 500 Hz value at Damping 40 %.
  - The legacy reverb decayed faster at 4 kHz (0.45) but kept a flat response up to 20 kHz.
  - The new programs roll off above about 8 kHz instead. See `measurements/reverb/reverb_spectrum.png`, which is power
    density on a scale where white noise plots flat.
- **Decay accuracy:** Hall, Plate, Chamber and Ambient follow the Decay setting within ±30 % at 1.5, 3 and 6 s. This is
  tested.
- **Robustness:** 2,000 blocks of random program changes and automation stay finite and bounded. This is tested.

## Audio examples
`docs/audio/reverb/` (48 kHz, 16-bit) contains the same original synthetic phrase (four plucked tones and a noise
burst) through the legacy reverb and each program. The wet signal is matched to -26 dBFS RMS and the dry signal is
identical, so the files compare character, not level.

## Plots
In `docs/measurements/reverb/`:
- `reverb_edc.png`: energy decay.
- `reverb_spectrum.png`: spectrum.
- `reverb_ir_waveforms.png`: first 400 ms of each response.

Regenerate them with:

```
VSX_IR_DIR=<dir> VSX_TEST_ONLY="Reverb measurements" ViceSynthXTests
python3 tools/plot_reverb.py <dir> docs/measurements/reverb
```

## Factory presets
The level calibration of the factory library was re-measured with the new reverb, and 66 trims were updated.
The library test passes: 88 / 0, every preset within -16 ± 3 dB or peak-limited, peaks <= -3 dBFS.

"Korg Scream" was renamed to "Sallen Scream" because the old name used a manufacturer's trademark.
