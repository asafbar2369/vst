# Six-operator FM engine (2026-11-21)

Every oscillator has a new **FM** mode. In this mode it plays a six-operator FM voice in the style of the classic
1983 six-operator architecture, and you can load your own `.syx` cartridges.

## Clean-room statement and licensing
- **Written from scratch.** The engine follows only the publicly documented architecture and the published MIDI /
  SysEx data format of that instrument family:
  - 6 operators and 32 algorithms;
  - feedback, ratio / fixed frequency, 4-rate / 4-level envelopes;
  - keyboard level and rate scaling, velocity sensitivity, pitch envelope and LFO;
  - the VCED single-voice layout (155 bytes) and the packed 32-voice bulk layout (128 bytes per voice).
- **Nothing copied from other implementations.** No code, tables or data were taken from Dexed / MSFA (GPLv3) or any
  other FM implementation, so there is no GPL obligation from this engine.
- **No ROM content is bundled.** The 12 factory FM voices (`fm::factoryVoices()`) are original designs made for this
  project.
- **User cartridges stay local.** They are read from the user's disk, and their data is never uploaded or sent to any AI
  provider.
- **Not hardware-identical.** The level curve (0.75 dB per step), the envelope rate-to-time mapping, the modulation index
  (a full-level modulator gives ±2 cycles of phase deviation), the feedback scaling and the LFO / pitch-envelope ranges
  are this project's own approximations. Exact equivalence with the hardware is not claimed, and there are no hardware
  measurements.

## What is implemented
| Area | Details |
|---|---|
| Algorithms | All 32, from the published chart. Feedback is self-feedback, except algorithm 4 (OP4 to OP6) and algorithm 6 (OP5 to OP6). The test checks the carrier count of every algorithm, that every operator is either a carrier or a modulator, and the render order. |
| Operators | Ratio (coarse 0 = 0.5, 1-31, fine) or fixed frequency (1 / 10 / 100 / 1000 Hz x 10^(fine/100)); detune ±7; output level 0-99; velocity sensitivity 0-7; amp-mod sensitivity 0-3; mute and solo in the editor (stored as an operator-enable mask). |
| Envelopes | 4 rates / 4 levels per operator. Notes start from L4 and release to L4; keyboard rate scaling 0-7. |
| Keyboard level scaling | Break point, left / right depth, -LIN / -EXP / +EXP / +LIN curves. The total level is capped at maximum. |
| Pitch | Pitch envelope (level 50 = no shift, ±4 octaves); transpose ±24; LFO with 6 waves, speed, delay with fade-in, PMD / AMD / PMS, and key sync; oscillator key sync. |
| Engine integration | A per-voice renderer at the engine's oversampled rate. It responds to the synth's pitch (key, glide, bend, octave / semi, modulation), level and pan. Synth filters, envelopes and effects apply after it. The new automatable parameter **FM Amount** (`osc_x_fm_amount`, 0-2) scales the whole modulation index. |
| SysEx | 32-voice bank (format 9, 4096 bytes) and single voice (format 0, 155 bytes), with checksum validation. A mismatch is shown as a warning, and the voices are still offered. A file may hold several messages. Raw 4096-byte bank data is accepted with a warning. Truncated, empty, foreign and garbage files are rejected with an explanation. All fields are clamped to legal ranges. The editor can also save a voice and a bank (.syx). |
| Editor | SOUND DESIGN > FM: oscillator selector, USE FM, voice name, LOAD CARTRIDGE, FACTORY VOICES, INIT VOICE, SAVE VOICE / BANK. The algorithm diagram shows carriers, modulation paths, the feedback loop and muted operators. Also: algorithm stepping, FM Amount knob, key sync / LFO sync, a 6-row operator grid of numeric fields (drag or wheel), pitch EG / LFO fields and LFO wave. The cartridge list loads with a click or the arrow keys (no Enter). AS NEW PATCH makes the voice a clean patch (OSC 1, filters off, amp release following the voice); otherwise the voice replaces only the selected oscillator. Loading a voice is one undo step; edits are published to the audio thread lock-free. |
| Presets / recall | The voice travels as a hex string (VCED + enable mask) in presets, undo, project state and A/B. Recall is tested. |

## Tests (`Tests/TestFm.cpp`, `Tests/TestGui.cpp`)
- **Algorithm topology:** all 32 algorithms.
- **Round trips:** 200 random voices through VCED, packed and hex; out-of-range data is clamped.
- **SysEx:**
  - bank and single-voice round trips;
  - a corrupted byte is detected by the checksum;
  - multiple messages, raw data, truncated / empty / garbage / foreign data;
  - loading from a file and from a missing file.
- **Renderer:**
  - pitch within 0.5 Hz (A4, ratio 2, ratio 0.5, transpose);
  - fixed 100 Hz at three different keys;
  - sustain while held and release after note-off;
  - velocity sensitivity, muted carrier, modulation brightening, FM Amount 0 giving a sine, feedback harmonics.
- **Stability:** every algorithm with random voices and feedback 7 at MIDI 0 / 36 / 60 / 96 / 127 (finite, bounded), and
  every factory voice.
- **Processor:** FM mode renders, survives a voice change while a note sounds, and recalls from project state and
  presets.
- **GUI:**
  - cartridge load (checksum OK);
  - selecting a voice and pressing the down arrow load immediately;
  - undo;
  - solo / mute masks;
  - a broken file and a deleted file are reported.

## Known limitations
- No hardware comparison measurements. The curves are approximations, as stated above.
- FM voice fields are not host-automatable. Only FM Amount and the oscillator's normal parameters are.
- Each FM oscillator renders one voice: unison, warp, granular and other mode-specific controls do not apply in FM mode.
- No performance data (aftertouch / breath / foot routing) is parsed from the format-9 / format-0 messages; those
  settings were outside the voice data in the original format anyway.
- The factory library has only 12 original FM voices. More are planned with the preset expansion.
