# Development Priorities

Ranking considers musical usefulness, competitive need, complexity, risk, CPU / memory cost, testability and maintenance.
Order of work follows the brief: compile -> fix core defects -> validate AI -> interface -> quality / CPU -> engines -> library -> release.

## P0 - blockers, crashes, corrupted state (all currently closed)
| Item | Status |
|---|---|
| Missing compilation units on the pushed branch | fixed (source audit in CI) |
| Windows crash from a 1 MB object on the stack | fixed |
| Uninstaller verification | fixed |
| Cancelled / stale AI results applied over user edits | fixed (tested) |

## P1 - essential quality and professional function
1. **FL Studio 25 verification** in an authorised environment (owner-run test plan: load, automation, project recall, scaling, MIDI, standalone). Needs the user's FL Studio machine or an authorised test VM.
2. **Live AI provider verification** with the user's own keys / local servers (Claude, OpenAI, Ollama, LM Studio); record latency, failures, cost.
3. **CPU**: SIMD processing of unison voices (4-8 lanes), cheaper OTA / ladder nonlinearities, per-tick constant hoisting; target >= 2x on the benchmark chord. Then multi-core voice rendering only if measured as safe.
4. **Output safety**: optional soft limiter / clip guard (default on in the standalone app) for dense chords.
5. **MPE and MIDI Learn** (expected by professional users; low DSP risk).
6. **Sanitiser runs** (ASan / UBSan / TSan builds in CI) and an allocation-in-audio-thread test.
7. Factory library review: listening pass, velocity / mod-wheel / macro checks, resolve the 14 near-duplicate candidates.

## P2 - competitive features and workflow
- Sample-to-wavetable conversion (pitch-synchronous slicing + harmonic analysis) and wavetable import / export UI.
- Layer architecture (up to 4 layers with splits / velocity ranges) - large; needs CPU headroom from P1.
- Effects: plate / shimmer / reverse reverbs, BBD chorus / delay, rotary, transient shaper, dynamic EQ, resonator.
- Microtuning (Scala .scl / .kbm).
- Spectrum analyser view, multisample mapping editor, piano-roll sequencer, MIDI import / export.
- Diode-ladder option: resonance-tracking cutoff calibration.
- AI: semantic preset search, preset morphing UI, better matching for FM / bell timbres (FM routing search), benchmark dataset + listening panel.
- Library growth with QA gates (no trivial variations), towards the 2,000-preset target over multiple releases.

## P3 - differentiation / research
- 6-operator FM engine, modal / physical modelling, vector synthesis, wave sequencing.
- Hardware-measured filter fits (requires access to hardware or licensed measurements).
- Vintage digital character tools (PCM-style attacks, low-resolution playback modes, DAC colouration) as original algorithms.
- 8x oversampling, AVX2-specific paths.
- Code signing, macOS / AU builds, AAX.
