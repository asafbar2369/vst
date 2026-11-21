# Issue #11 working checkpoint — not release acceptance

Integration base: 0b9649ca49ae5507aff2219be69e4ac55197083e (parents: Codex 0796fc0, Claude 50d0ac3).

AudioAcceptanceTests adds original synthetic dry/effect-isolated WAV evidence at 44.1/48/96 kHz, raw impulse and anti-phase predelay probes. CI writes logs/audio into validation artifacts. Raw files retain gain; copies are stereo-RMS matched across the entire 4-second fixture including tail (not LUFS). Gain and unnormalized metrics are logged. Listening remains pending. No external samples or ROMs.

Source hypothesis: reverb side input currently bypasses mono predelay; the anti-phase impulse measures energy before 200 ms. This does not prove the cause of owner's harshness. Current rack mix can double a coherent identity processor at 50%; classify effects before changing its law. Right diffuser allocation also needs rate/boundary investigation; power-of-two rounding means undersizing cannot be assumed at all rates.

Research consulted 2026-11-21:
- https://www.dsprelated.com/freebooks/pasp/Choice_Delay_Lengths.html — delay length/mode-density design; do not treat prime lengths alone as quality proof.
- https://www.dafx.de/paper-archive/2004/P_061.PDF — circuit-derived nonlinear ladder model. Current filters are not verified hardware emulations.

Next: finish combined CI, retain exact result SHA; collect baseline metrics/WAVs; fix confirmed predelay fault with regression; audit rack identity gain and reverb decay, aliasing and filter response before curating presets. Complete all #11 and #7 requirements including 500 curated presets, full six-op/32-algorithm FM and legal SYX, true local inference, UI workflows, installer/validators and owner listening. No claim that any of those gates is closed by this checkpoint.

## Follow-up implementation

Reverb now applies predelay to both channels before constructing mid/side diffusion inputs; right diffuser allocation accounts for its 1.07 length multiplier. No feedback topology, damping, tail gain or rack mixing-law changes are claimed here. Anti-phase impulse regression requires silence before predelay and a non-silent later tail. Baseline branch e5399bc retains original behavior for A/B. Integration includes the independent panel toggles and immediate preset navigation. All Windows runtime results still require inspection.

Concurrent Claude sample work f4deb4c was observed after original green baseline; it has NOT been integrated. Its parameter-layout and transaction-API changes require careful reconciliation with PR #10 sample work. Do not blindly overwrite either branch.

Additional primary sources consulted: https://www.xferrecords.com/products/serum-2 (hybrid engines/workflow); https://www.arturia.com/products/software-instruments/pigments/overview (six engine overview); https://u-he.com/downloads/manuals/plugins/diva/Diva-user-guide.pdf (circuit-model/CPU tradeoff); https://kilohearts.com/docs/phase_plant (modular generator/effect structure); https://soniccharge.com/synplant (audio-driven patch creation); https://asb2m10.github.io/dexed/ (DX7 SysEx and GPLv3); https://docs.ollama.com/capabilities/tool-calling and https://lmstudio.ai/docs/developer/openai-compat/tools (local tool-call protocols). These are documentation comparisons, not measured competitor sound rankings or a complete market survey.
