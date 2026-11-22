# Changelog

All dates 2026-11-20 unless noted. Commits on `claude/bold-cori-kvu779`.

## 2026-11-22: four layers, splits and velocity layers
- Layer A (the editor's patch) plus layers B, C and D (complete sounds with their own engine and effects rack).
- Each layer has a key range, velocity range, transpose, level and pan; a note-off releases the note its key started
  even if the transpose changed meanwhile. PANIC and preset changes silence every layer.
- New SOUND DESIGN > LAYERS page (LOAD from the library, EDIT swaps a layer into the editor, CLEAR).
- Layers are saved in the project / host session (not in presets). Tests: TestLayers, TestGui.
- **Preset morphing:** a Morph parameter (automatable, project setting) blends the editor's patch towards a morph
  target (SET TARGET from the library or USE CURRENT). Continuous parameters interpolate in their normalised range;
  switches and choices change at 50 %. Wavetables, samples, FM banks, matrix routing and rack order are not morphed.
- **Audio-rate filter FM:** each analog filter has an FM depth (0-4 octaves) and an FM source (OSC A / B / C, Sub,
  Noise). The cutoff follows the source every sample (the matrix runs at a 16-sample control rate). With FM at 0 the
  output is byte-identical to before (54 filter / preset WAVs compared).
- **Ratchets:** every arpeggiator and step-sequencer step can repeat 1-4 times, evenly spaced within the step (ARP:
  RATCHET lane; SEQ: "Lane: Ratchet"). Releasing the keys cancels repeats still waiting. Saved in presets only when
  used, so existing presets are written unchanged.

## 2026-11-21: microtuning, MIDI Learn, MPE and seven effects
- Scala microtuning (.scl / .kbm); MIDI Learn on every knob; MPE (lower zone); BBD chorus, BBD delay, shimmer,
  rotary, resonator, transient shaper and spectral effects, each measured (docs/EFFECTS_AND_CONTROL.md).
- Old rack orders are completed instead of discarded.
- Linux full suite: 1,316,976 passed, 0 failed.

## 2026-11-21: new synthesis engines, vector / wave sequencing, aliasing, CPU
- **Four new oscillator modes** (docs/SYNTHESIS_ENGINES.md):
  - **Physical:** a waveguide string with pluck / hammer / bow excitation, pick position, stiffness and a T60 control.
  - **Modal:** resonator bank with measured material mode ratios.
  - **Phase Dist:** eight shapes and a DCW Amount.
  - **Additive:** 64 partials with partial decay and motion.
- **Vector synthesis:** an X/Y pad mixing A / B / C / Sub-or-Noise with equal power.
- **Wave sequencing:** OSC A position steps with a pattern, rate / sync and crossfade.
  - New SOUND DESIGN > VECTOR page.
- **Factory presets for the new engines:** Plucks, Bells (mallets), Vintage 1980s (phase distortion), Digital (additive).
- **Aliasing:** a four-point BLEP replaces the two-point polyBLEP for the VA and sub saw / square, and the sub triangle
  uses polyBLAMP. Saw C7: Eco −27.0 → −36.4 dB, High −38.7 → −51.8 dB, Ultra −45.4 → −53.8 dB. The guards are
  tightened.
- **CPU:**
  - The unison loop computes the analog setup once per sample; the reverb uses a parity table; the filter DC
    coefficient is precomputed.
  - −14 % instructions per block, bit-identical output, +4 % wall clock.

## 2026-11-21: audio and workflow correction (9f2cf20, 113cf97, 2d8729b, and this commit)
- **FM engine replaced by MSFA**, the Apache-2.0 "Modern" engine of Dexed, vendored unmodified (Source/ThirdParty/msfa,
  NOTICE.md). The earlier clean-room FM renderer is gone; SysEx, the voice model and the editor are unchanged.
  - Measured equal to Dexed's Modern engine: 57 cases, median residual −267 dB, largest difference 1 LSB
    (docs/FM_DEXED_COMPARISON.md). This is a regression test on every build.
  - `FmRenderer::noteOn` played INIT instead of the given voice for direct callers; fixed.
  - MSFA feedback history was uninitialised for new voices (valgrind); now zero-initialised without modifying MSFA.
    `Porta::init_sr` is now called as in Dexed.
- **Gain staging** (docs/GAIN_STAGING_REPORT.md):
  - Clipping root cause measured: presets were calibrated on single notes only, so chords hit the safety stage.
  - The library is now calibrated on single notes and full-velocity chords, and the chord limit is now **true peak**
    (4x, BS.1770-style meter).
  - Default Preset Volume −10.5 dB.
  - Insert effects use a linear mix law; other effects use equal power.
- **Session controls:**
  - MASTER VOL (post-FX, exact silence at minimum, never stored in presets);
  - GLOBAL FX BYPASS;
  - stereo meters with a CLIP latch; PANIC;
  - silent preset switching (voices, sustain, arp, tails and pending MIDI cleared; held keys do not retrigger);
  - all audition features removed.
- **Interface:**
  - STUDIO / SOUND DESIGN / FM header (LIBRARY and STAGE pages removed);
  - SAVE PRESET with overwrite confirmation, PRESET VOL and AUTO GAIN in the browser;
  - SAVE / RESET SETTINGS.
- **Factory library: 536 presets** (FactoryExpansion: curated voicings × treatments in 11 families, plus 40 original
  FM designs).
  - Fixed: five silent Bass presets (a multisample loaded as a single sample);
  - fixed: two identical preset pairs (non-injective pair selection);
  - fixed: bowed-string samples given staccato / pizzicato envelopes.
  - The library test now fails on identical sound states, and logs the 20 most acoustically similar pairs.

## 2026-11-21: PR #10 merge and issue #11 interface gates
- Merged PR #10 (English-only AI, transactional PatchState loads, multi-file note-named multisamples, keyboard / waveform
  drops, instant arrow-key preset loading). The Codex MSVC error C2872 (`OscMode` ambiguous with `Recipes::OscMode` in
  TestCoreDSP) is fixed with a namespace-scope constant.
- PR #10's direct `setSample` / `setMultiSample` calls routed through the transactional importer (`importToOscillator`);
  "files:" multisamples recalled and reported as missing per file.
- Load failure policy: user choices keep the previous sound (KeepPrevious); project / preset recall keeps the missing
  reference for relinking (KeepReference). Reset supersedes queued sample loads.
- Fix: a looping sample voice could stop when a wrap landed in the last sample of the file (high notes; found by PR #10's
  all-128-notes test).
- INIT button (clean patch, undoable); PRESETS / AI / MACROS / KEYS show-hide toggles in the header, centre view expands,
  layout stored with the plug-in state; the duplicate AI-panel settings gear removed (one SETTINGS button); a waveform /
  source dropdown arrow on every oscillator (Studio cards and Sound Design panels); search-field up / down and list
  Page Up / Page Down / Home / End load presets immediately.
- Linux full suite after the merge: 913,991 passed, 0 failed.

## 2026-11-21: six-operator FM engine and local AI agent
- New oscillator mode FM: clean-room six-operator engine, all 32 algorithms, feedback, ratio / fixed, 4-rate / 4-level
  envelopes, keyboard level / rate scaling, velocity, pitch EG, LFO; FM Amount parameter; .syx 32-voice and single-voice
  import with checksum validation, export; SOUND DESIGN > FM editor with algorithm diagrams, operator grid, mute / solo,
  cartridge list with instant arrow-key loading; 12 original factory FM voices and an FM preset category
  (docs/FM_ENGINE.md). No ROM data and no third-party FM code.
- Local AI agent: DETECT LOCAL AI wizard (Ollama / LM Studio discovery, model selection, setup guidance) and a
  render -> analyse -> refine tool loop for AI providers (Agent steps 0-4) (docs/LOCAL_AI_AGENT.md).
- Preset names / tags no longer reference trademarks or people (Acid Squelch Line, String Machine Night, Rain City Brass).

## 2026-11-21: reverb redesign
- New 16-line FDN reverb with eight programs (Hall, Plate, Room, Chamber, Ambient, Nonlinear, Gated, Reverse), prime
  geometric loop lengths, in-loop diffusion, two-band absorption, per-line random modulation, early reflections and
  second-order wet filters. New parameter Reverb Program. Measured against the old reverb (docs/REVERB_REPORT.md):
  comb periodicity 0.205 -> 0.034-0.128; matched-loudness WAV examples in docs/audio/reverb.
- Offline AI designer understands reverb program words (plate, room, chamber, gated, reverse ...).
- Factory library recalibrated for the new reverb; "Korg Scream" renamed "Sallen Scream" (trademark).

## Validation and QA
- `59eed18` Windows CI #38: full suite 909,808 / 0 failed; pluginval strictness 10 SUCCESS; Steinberg validator 47 / 0;
  installer + uninstaller verified.
- `3b5cf64` Fix: Windows stack overflow (1.09 MB engine on a 1 MB stack) - voice pool on the heap.
- `59eed18` Measured DSP regression suite (filters, oscillators, effects, library, processor matrix, state recall,
  automation, long run); SEM / SVF drive fix; as-played loudness calibration of all 85 presets.
- `9104fdb` CPU: shared filter coefficients, single-evaluated oscillator shapes (4.65x -> 5.22x realtime on the test chord).
- Sound-matching benchmark with development / evaluation split.

## AI workspace
- `2e3d0a8` Six AI modes (offline, Claude, OpenAI, Ollama, LM Studio, other OpenAI-compatible) with model discovery,
  connection tests, HTTPS policy, cancellable HTTP; stale-result protection; A/B compare; AI change highlights; reference
  audio drag-and-drop with consent for sharing analysis; Hebrew prompts (lexicon for the offline designer, RTL text);
  save with rename; measured results shown separately; AI settings panel; provider / controller tests.
- `ba7ab25` Merged PR #5 (AI hardening: per-job reference snapshots, single in-flight request, cancelled results never
  applied, atomic rejection of invalid patch operations, safer reference analysis; core test target; Windows workflow).

## Interface
- `f5ce892` Cinematic editor at 1648 x 928 (Library / Studio / Sound Design / Stage), preset browser, hero, studio cards,
  AI panel, macro strip, performance keyboard, overlays, editor tests and screenshots.
- `62aec7b` Windows workflow: pluginval, Steinberg validator, Inno Setup installer, install / uninstall check.

## Earlier
- `85e281b`, `8f09a56` Engine, filters, effects, presets, AI layer, analysis and processor (initial implementation).
