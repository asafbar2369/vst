# Physical, Modal, Phase Distortion and Additive engines (2026-11-21)

These are four new oscillator modes, added after the existing seven (Wavetable, Analog, Sample, Multisample, Granular,
Spectral, FM). Existing modes, presets and projects are unchanged.
- **Modes are stored by name.** The new modes are appended, so old presets and projects load as before.
- **New parameters are added by string ID** (ParameterID version 2). Hosts identify parameters by ID, so automation
  in existing projects is unaffected.
- **No third-party code.** All four are original implementations of published textbook techniques.
- **Code:** `Source/Synth/ExtraEngines.*`.
- **Tests:** `Tests/TestExtraEngines.cpp` (259 checks) and the editor test in `Tests/TestGui.cpp`.

## Controls
| Control | Physical | Modal | Phase Dist | Additive |
|---|---|---|---|---|
| Position (the oscillator's Pos knob) | pick position | strike position | – | – |
| Excite | Pluck / Hammer / Bow | Pluck (impulse) / Hammer (mallet) / Bow | – | – |
| Decay | 60 dB decay time of the fundamental | 60 dB decay time of the first mode | – | – |
| Brightness | pluck noise colour, hammer contact time | mallet hardness, mode tilt | – | – |
| Damping | high partials decay faster | higher modes decay faster | – | – |
| Inharmonicity | string stiffness (stretched partials) | stretch (+) / compress (−) of the material's ratios | – | – |
| Material | – | String, Stiff String, Free Bar, Tuned Bar, Glass, Bell, Membrane, Tine | – | – |
| PD Shape / PD Amount | – | – | 8 shapes, DCW 0–1 | – |
| Tilt, Odd/Even, Stretch, Formant, Partials | – | – | – | spectrum (shared with Spectral) |
| Partial Decay / Partial Motion | – | – | – | higher partials die faster / slow per-partial drift |

Every control is modulatable and automatable. Envelope 3 driving PD Amount gives the classic DCW sweeps.

## Physical: digital waveguide string
The model is the single-loop waveguide string of Karplus–Strong, with the Jaffe–Smith extensions:
- **Loop:** one delay line, then a first-order Thiran allpass for the fractional delay, then four first-order allpasses
  for stiffness (dispersion), then a one-pole loss filter and the loop gain.
- **Tuning:** the integer delay and the Thiran coefficient are solved from the exact phase delays, at the fundamental,
  of the loss filter, the dispersion chain and the Thiran stage (iterated). The fundamental is therefore in tune at
  every Damping and Stiffness setting.
- **Decay:** the loop gain is set from the requested T60 and the loss filter's gain at the fundamental. It is never
  above 1 (the loss filter is 1 at DC), so the loop cannot grow.
- **Excitation:** Pluck is a noise burst whose colour comes from Brightness. Hammer is a raised-cosine contact.
  Both pass through a pick-position comb: plucking at x/L cancels every (L/x)-th harmonic. Bow is band-limited noise
  fed into the loop while the key is held. It is **not** a friction (bow-string) model.

**Measured** (48 and 96 kHz, MIDI 36–96, stiffness 0 and 0.5):
- Pitch error within ±1 cent; the test limit is 3 cents.
- Requested T60 0.5 / 1 / 3 s measured 0.500 / 1.000 / 2.999 s, with Damping 0.
- Stable at every excitation, Damping and Stiffness extreme, MIDI 0 to 127. Tested as "no growth": the level at the
  end never exceeds the level after the attack.

## Modal: resonator bank
- **Structure:** up to 32 two-pole resonators. Each mode's T60 comes from Decay, scaled by Damping for the higher
  modes. Amplitudes combine a natural fall-off, the Brightness tilt and strike-position weighting. The mode sum is
  normalised so that in-phase modes do not stack up at the strike.
- **No aliasing at the source:** modes above 0.45 × the sample rate are not rendered.
- **Material ratios**, from Fletcher & Rossing, *The Physics of Musical Instruments* (2nd ed.):
  - **Free bar** (free-free beam): 1, 2.756, 5.404, 8.933, …
  - **Tine** (clamped-free beam): 1, 6.267, 17.547, 34.386
  - **Membrane** (circular membrane, Bessel zeros): 1, 1.594, 2.136, 2.296, …
  - **Tuned bar**, as tuned in marimba / vibraphone bars: 1, 3.99, 9.9 (three modes only)
  - **Bell**, a well-tuned church bell relative to the prime: hum 0.5, prime 1, tierce 1.2, quint 1.5, nominal 2, …
  - **Glass**: thin-ring bending modes n(n² − 1)/√(n² + 1), normalised to n = 2 (Rayleigh). This is a model of a ring,
    not a measured wine glass.
  - **Stiff String**: k·√(1 + Bk²) with B = 4·10⁻⁴.

**Measured:** for the free bar, membrane and tine, the first three spectral peaks are within 0.012% of the expected
ratios. Every material and excitation is stable from MIDI 0 to 127.

## Phase Distortion
- **Principle:** a cosine is read through a warped phase, the technique of mid-1980s phase-distortion synthesizers.
- **Amount (DCW)** moves the phase map from linear, a pure sine, to the full shape. There are eight shapes:
  - Saw, Square, Pulse, Double Sine, Saw-Pulse;
  - three resonance shapes: a sine at 1–16 × the fundamental under a saw, triangle or trapezoid window.
- **Continuous at the wrap:** every shape, at every Amount, is continuous across the cycle wrap (no hard-sync step).
  The resonance shapes crossfade from the sine for this reason.
- **DC:** a 10 Hz DC blocker removes the shapes' DC offset.
- **Unison** works as in the Analog mode.
- **Measured:**
  - Amount 0 gives a pure sine for every shape, with THD of −145 dB (numerical floor).
  - At Amount 1 the strongest of the 2nd and 3rd harmonics is −0.0 to −19.4 dB relative to the fundamental,
    depending on the shape.
- **Not claimed:** waveform identity with any specific instrument. These are the published principle and shape
  families, not a circuit or firmware emulation.

## Additive
- **Partials:** up to 64 sine partials (table lookup) with an analytic spectrum.
  - Base amplitude 1/k; Tilt as in the Spectral mode; Odd/Even scales the even partials; Stretch is k^(1 + 0.12·s).
  - Formant moves the spectral envelope independently of pitch.
  - Partials at or above 0.45 × the sample rate are not rendered.
- **Partial Decay:** partial k decays at a rate proportional to k − 1.
- **Partial Motion:** a slow, bounded random walk of each partial's level.
- **Level:** normalised to equal RMS whatever the spectrum.
- **Measured:**
  - Odd/Even −1: the strongest even partial is −155 dB relative to the fundamental.
  - Partial Decay 0.6: over 1.4 s the fundamental stays at 0.0 dB and the 12th partial falls 120.5 dB.
  - Level spread across Tilt −1 / 0 / +1: 0.05 dB.

## Levels
Each engine is matched, on INIT, to the analog saw by loudest-400 ms RMS (the measure of the library calibration):

| Engine | Level vs saw |
|---|---|
| Phase Dist | +1.5 dB |
| Modal | −1.3 dB |
| Additive | −3.1 dB |
| Physical | −5.9 dB |

A plucked string has a high peak-to-RMS ratio, so its peaks reach the saw's level first. Factory presets are then
calibrated individually.

## Limits
- Physical, Modal and Additive render one voice per oscillator: unison, warp and fold do not apply to them, as with
  FM. Phase Dist supports unison.
- **Bow** is a sustained noise excitation, not a bow-string friction model.
- The **Tuned Bar** material has only three modes, and **Glass** is a thin-ring model.
- **Pitch changes:** pitch bend and glide retune the string every control tick. Large, fast pitch jumps can produce a
  small discontinuity when the integer delay changes.
- No listening tests were possible in this environment.
