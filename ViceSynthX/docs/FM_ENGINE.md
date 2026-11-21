# Six-operator FM engine (2026-11-21, MSFA since 9f2cf20)

Every oscillator has an **FM** mode. In this mode it plays a six-operator FM voice in the style of the classic 1983
six-operator architecture, and you can load your own `.syx` cartridges.

## Engine and licensing
- **Sound engine: MSFA, the "Modern" engine of Dexed.** Since commit 9f2cf20 the operators, envelopes, pitch envelope,
  LFO and algorithms are rendered by MSFA (Music Synthesizer for Android; Google, Pascal Gauthier, Jean Pierre
  Cimalando). Its files are vendored **unmodified** from Dexed commit 1ed5a4d in `Source/ThirdParty/msfa`.
  - MSFA is licensed under the **Apache License 2.0**. The licence text and a notice listing the copyright holders ship
    in that folder (`LICENSE-APACHE-2.0.txt`, `NOTICE.md`). Apache 2.0 is compatible with this plug-in's distribution.
  - **Not used:** Dexed's own GPLv3 code, including the default "Mark I" engine, the OPL engine, Dexed's tuning files
    and its logger. `tuning.*`, `libMTSClient.h` and `../debugger/Logger.h` are original replacements (standard 12-TET
    tuning with the same table as Dexed, an MTS-ESP stub and a no-op trace macro).
  - Using Mark I would require distributing the whole plug-in under GPLv3. That is an owner decision and has not been
    made.
- **VICE SYNTH X's own code** (original): the voice data model (`fm::Patch`), VCED / packed / hex codecs, SysEx import
  and export, the algorithm chart used for the diagrams, the FM editor, the factory voices, FM Amount, and the
  integration: a fixed 48 kHz internal rate, cubic resampling to the voice rate, the synth's pitch passed as MSFA
  pitch bend (±48 semitones), and Dexed's per-voice output conversion.
- **No ROM content is bundled.** The factory FM voices (`fm::factoryVoices()` and the FM presets) are original designs.
- **User cartridges stay local.** They are read from the user's disk, and their data is never uploaded or sent to any AI
  provider.
- **Hardware equivalence is not claimed.** MSFA is a well-regarded emulation, but no hardware measurements were made
  here. Equivalence with **Dexed's Modern engine** is measured: see `docs/FM_DEXED_COMPARISON.md`.

## What is implemented
| Area | Details |
|---|---|
| Algorithms | All 32, from the published chart. Feedback is self-feedback, except algorithm 4 (OP4 to OP6) and algorithm 6 (OP5 to OP6). The test checks the carrier count of every algorithm, that every operator is either a carrier or a modulator, and the render order. |
| Operators | Ratio (coarse 0 = 0.5, 1-31, fine) or fixed frequency (1 / 10 / 100 / 1000 Hz x 10^(fine/100)); detune ±7; output level 0-99; velocity sensitivity 0-7; amp-mod sensitivity 0-3; mute and solo in the editor (stored as an operator-enable mask). |
| Envelopes | 4 rates / 4 levels per operator. Notes start from L4 and release to L4; keyboard rate scaling 0-7. |
| Keyboard level scaling | Break point, left / right depth, -LIN / -EXP / +EXP / +LIN curves. The total level is capped at maximum. |
| Pitch | Pitch envelope (level 50 = no shift, ±4 octaves); transpose ±24; LFO with 6 waves, speed, delay with fade-in, PMD / AMD / PMS, and key sync; oscillator key sync. |
| Engine integration | A per-voice MSFA renderer at a fixed 48 kHz internal rate, cubic-resampled to the voice rate. It responds to the synth's pitch (key, glide, bend, octave / semi, modulation), level and pan. Synth filters, envelopes and effects apply after it. The new automatable parameter **FM Amount** (`osc_x_fm_amount`, 0-2) scales the whole modulation index. |
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
- No hardware comparison measurements. The Dexed (Modern engine) comparison is in `docs/FM_DEXED_COMPARISON.md`.
- FM voice fields are not host-automatable. Only FM Amount and the oscillator's normal parameters are.
- Each FM oscillator renders one voice: unison, warp, granular and other mode-specific controls do not apply in FM mode.
- No performance data (aftertouch / breath / foot routing) is parsed from the format-9 / format-0 messages; those
  settings were outside the voice data in the original format anyway.
- The factory library has only 12 original FM voices. More are planned with the preset expansion.
