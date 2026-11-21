# Changelog

All dates 2026-11-20 unless noted. Commits on `claude/bold-cori-kvu779`.

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
