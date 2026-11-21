# Gain staging, clipping and preset loudness (2026-11-21)

## What caused the clipping
- **The output stage was hiding the problem.** It is transparent below −1 dBFS and bends smoothly to a ceiling just under
  0 dBFS above it. That curve was doing the work.
- **The factory library was calibrated on single notes only.** The target was loudest-400 ms RMS −16 dBFS with peaks
  ≤ −4 dBFS. Nobody measured chords.
- **Measured on the real engine:** with a full-velocity six-note chord, **74 of 97 presets went above −1 dBFS** (median
  +4.0 dBFS, worst +10.3 dBFS). Every one of those notes was shaped by the safety curve. That is the "unpleasant clipping"
  you hear on analog patches, which are dense, in phase at the attack, and often use unison.
- **INIT at the old default level (−3 dB):** a single note peaked at −8.6 dBFS and a full-velocity chord at **+4.6 dBFS**.

## What changed (no limiter added, no concealment)
1. **Calibration covers chords.**
   - Every factory preset is rendered as a single note and as a full-velocity six-note chord.
   - Preset Volume is set so that the single-note loudest-400 ms RMS is −20 dBFS ± 3 dB, the single-note peak is
     ≤ −6 dBFS, and the chord **true peak** is ≤ −1.5 dBTP.
   - True peak is measured with 4x windowed-sinc oversampling in the manner of ITU-R BS.1770-4 Annex 2
     (`Source/Analysis/TruePeak.h`). On test sines it under-reads by at most 0.3 dB up to 20 kHz and never overshoots
     (`Tests/TestCoreDSP.cpp`). Inter-sample peaks are what a D/A converter, sample-rate conversion or lossy encoding
     reconstructs, so the headroom rule now holds after playback, not only at sample instants.
   - The library test fails if any chord true peak exceeds −1 dBTP or any chord sample reaches the safety stage.
   - Sounds with high crest factors are allowed below the window when a peak limit decides, and the test reports this.
2. **Default Preset Volume is −10.5 dB.** INIT and new patches start with headroom: a single note at −16.1 dBFS and a
   full-velocity chord at −2.9 dBFS.
3. **Effect gain laws (measured in `Tests/TestFxLaws.cpp`).**
   - The old rack law kept dry and wet both at unity at 50 % mix. For insert effects, whose wet signal is the processed dry
     signal (correlation 0.95–1.0), that summed to +5.7 to +8.1 dB.
   - Insert effects now use a linear crossfade. Modulation, delay and reverb effects use an equal-power crossfade.
   - The new reverb's wet level is about equal to the dry level.
4. **Filter drive and headroom** (`docs/measurements/filters`).
   - The drive law changed from linear 0–30 dB to 24·d^1.5 dB.
   - Larger internal headroom: THD at drive 0.3 went from 2.5–28 % to 0.3–7.8 %.
5. **Per-voice amplitude.** The envelope × velocity × 0.7 law was not changed. Polyphonic headroom is provided by the
   calibration and the default level above, so the sound of the effect chain is unchanged.
6. **The output safety stage stays.** It is a hard-protection floor only. The new **CLIP** indicator in the header latches
   whenever the stage engages, so it can no longer hide anything.

## Library integrity (2026-11-21, 536 presets)
- **Silent voicing:** five Bass presets used the "Warm Bass" source as a single sample. It exists only as a
  multisample, so those presets rendered nothing (−58 to −61 dBFS at +12 dB trim). They now load the multisample.
- **Identical presets:** the expansion paired voicing `i % V` with treatment `(i + i / V) % T`. That covers every pair
  only when gcd(V + 1, T) = 1. For Bass (9 × 6) it does not, so two pairs of presets were the same sound under different
  names. Selection is now `(v, (v + q) % T)` for candidate `q·V + v`, which is injective for any V and T.
- **Musical mismatch:** bowed-string samples (slow to speak) were given staccato and pizzicato envelopes, which end
  before the sound develops. Voicings and treatments now carry flags, and such pairs are skipped.
- **Test strengthened:** the library test now fails on any two presets with an identical sound state (all parameters,
  sources, modulation, FM voices and patterns, ignoring name and Preset Volume). The near-duplicate candidate
  report (parameter distance and acoustic similarity) is kept as a measurement.

## New controls
- **Preset Volume** (browser panel; stored in presets, ID `master_volume`).
- **AUTO GAIN** (browser panel). It renders the current sound offline on a worker thread, silently, and applies the same
  rule as the library, including the true-peak chord limit. It is undoable.
- **MASTER VOL** (header; `master_out`; −70 dB is exact silence).
  - A 30 ms linear ramp.
  - Applied after all effects.
  - Automatable and recalled with the project.
  - Never stored in or changed by presets.
- **Stereo output meters and CLIP** (header).
- **GLOBAL FX BYPASS** (header; `fx_bypass`). It uses the per-effect click-free ramps and is never stored in presets.

## Preset switching and PANIC
- On any preset change (and on PANIC), the audio thread clears the following at the start of the next block:
  - all voices;
  - the sustain state;
  - the arpeggiator and sequencer;
  - every effect's state (reverb tails, delay feedback buffers);
  - pending injected MIDI.
- A 64-sample fade from the last output sample removes the click.
- Held keys do not retrigger. Only new notes sound.
- **Tested** in `Tests/TestSessionBehaviour.cpp`:
  - a held full-velocity chord with sustain pedal, arpeggiator, 10 s reverb and 0.9 delay feedback gives exact digital
    silence over the 200 blocks after the switch;
  - 120 rapid switches with random notes;
  - PANIC;
  - MASTER VOL level, silence, persistence across presets and project recall.
- All audition features were removed: preset double-click, the hero play button, the AI preview buttons, AI
  auto-preview and the scheduled audition MIDI. AI analysis renders silently and offline.

## Evidence files
- `Tests/TestClipAB.cpp` renders eight presets (same DSP, same MIDI) at the Preset Volume they shipped with and at the new
  calibration, through the output stage. It counts the samples that reached the safety stage. With `VSX_AUDIO_DIR` set,
  it writes `clip_<preset>_{before,after}.wav`.
- Listening tests were not possible in this environment. They are pending with the owner.
