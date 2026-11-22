# New effects, microtuning, MIDI Learn and MPE (2026-11-21)

All are original code, built from published techniques. Measurements come from `Tests/TestExtraEffects.cpp`,
`Tests/TestTuning.cpp` and `Tests/TestSessionBehaviour.cpp`.

## Effects (FX RACK, 25 effects)
| Effect | Technique | Measured |
|---|---|---|
| BBD Chorus | dual bucket-brigade-style chorus, modes I / II / I+II (0.513 / 0.863 / 9.75 Hz LFO), inverted modulation L/R, BBD input / output filtering, compander-style saturation, hiss | rate 0.512 / 0.853 Hz, swing 1.83 ms (1.85 ms set) |
| BBD Delay | delay whose bandwidth follows bucket-brigade physics: stages / (4 x time); 4th-order low-pass, saturation, hiss and wow in the loop | −3 dB bandwidth 7.7 kHz at 100 ms, 2.5 kHz at 300 ms (4096 stages) |
| Shimmer Reverb | algorithmic reverb with a pitch-shifted feedback path (+12 / +7 / +5 / +19 / +24 / −12 st); loop gain scaled by 1/√T60 | octave band +44.9 dB vs Shimmer 0; tail decays 27.5 dB in 6 s; wet gain +0.35 dB |
| Rotary Speaker | horn / drum after an 800 Hz Linkwitz-Riley crossover; Doppler from rotor radius / speed of sound; amplitude modulation; 2 mics; slow 0.8 / 0.67 Hz, fast 6.8 / 5.7 Hz; the drum accelerates 5x slower | horn 0.86 Hz slow, 6.75 Hz fast |
| Resonator | tuned feedback combs (unison, octave, fifth, major, minor, sus4), T60 decay, tone; loop delay compensates the filter phase delay | 3.9 cents from the note, 34 dB above a tritone |
| Transient Shaper | differential envelopes; attack / sustain shaping +12 / −24 dB | attack +11.9 / −14.6 dB, sustain −5.2 dB; transparent at 0 |
| Spectral | STFT 2048 / hop 512; blur, freeze (phase-vocoder instantaneous frequency), tilt | identity −138 dB; frozen 440 Hz held at −25 dB after the input stops |

- **Rack order:** presets and projects saved before these effects list 18 effects. Their order is kept, and the new
  effects are inserted at their default places. (Before this change, any order of a different length was discarded.)
- **Limits:**
  - "BBD" means BBD-style: the clock, filtering and hiss behaviour are modelled, not a specific chip circuit.
  - The spectral effect delays its wet signal by 2048 samples. This is not reported as plug-in latency, because it
    is an effect inside the mix.

## Microtuning (Settings > ENGINE > Tuning)
- **Supported files:** Scala `.scl` scales (cents and ratios) and `.kbm` keyboard mappings (retuned range, middle key,
  reference key and frequency, formal-octave degree, unmapped "x" keys).
- **Engines:** every oscillator mode follows the tuning, including Physical, Modal and FM. Unmapped keys stay silent.
  Glide starts from the tuned pitch.
- **Storage:** saved with the project, never in presets.
- **Measured:** 19-EDO through the synth within ±0.007 cents. A 12-EDO `.scl` reproduces 12-TET exactly.

## MIDI Learn
- **Use:** right-click any knob and choose MIDI Learn, then move a controller.
- **Timing:** the value is heard in the same audio block. The host and UI are updated with change gestures from the
  message thread.
- **Bindings:** one CC per parameter. CC 64 and 120–127 cannot be learned. Bindings are saved with the project.

## MPE (Settings > ENGINE)
- **Zone:** lower zone. Channel 1 is the master channel. Channels 2–16 give each note its own pitch bend (range
  1–96 semitones, default 48), pressure (the Aftertouch source) and CC 74 slide (the Timbre source).
- **Notes:** the same key on two channels sounds twice, and a note-off releases only its own channel.
- **Storage:** a project setting.
- **Measured:** a +2-semitone bend on channel 2 gives C4 and D4 at equal level. After the channel-2 note-off, D4 is at
  −142.6 dB.
- **Limits:** upper-zone MPE and MPE configuration messages (MCM) are not handled. The zone is fixed to the lower
  zone.

## Arpeggiator and sequencer ratchets
Each arpeggiator step (RATCHET lane) and sequencer step ("Lane: Ratchet") repeats 1-4 times, evenly spaced within
the step; each repeat has the step's gate divided by the ratchet count. Repeats that fall in later audio blocks are
queued (pre-allocated, no audio-thread allocation) and cancelled when the keys are released, the mode changes or PANIC.
Measured (TestArpRatchet, 48 kHz, 120 BPM, 1/16 = 6,000 samples): ratchet 3 repeats within 1 sample of even spacing;
key released after 256 samples during a ratchet 4 step: no repeat afterwards and every note-on has a note-off;
sequencer ratchet 4: four notes, last at 0.75 step. Presets without ratchets are written exactly as before.
