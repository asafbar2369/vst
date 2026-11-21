# Issue #11 working checkpoint — not release acceptance

Integration base: 0b9649ca49ae5507aff2219be69e4ac55197083e (parents: Codex 0796fc0, Claude 50d0ac3).

AudioAcceptanceTests adds original synthetic dry/effect-isolated WAV evidence at 44.1/48/96 kHz, raw impulse and anti-phase predelay probes. CI writes logs/audio into validation artifacts. Raw files retain gain; copies are stereo-RMS matched across the entire 4-second fixture including tail (not LUFS). Gain and unnormalized metrics are logged. Listening remains pending. No external samples or ROMs.

Source hypothesis: reverb side input currently bypasses mono predelay; the anti-phase impulse measures energy before 200 ms. This does not prove the cause of owner's harshness. Current rack mix can double a coherent identity processor at 50%; classify effects before changing its law. Right diffuser allocation also needs rate/boundary investigation; power-of-two rounding means undersizing cannot be assumed at all rates.

Research consulted 2026-11-21:
- https://www.dsprelated.com/freebooks/pasp/Choice_Delay_Lengths.html — delay length/mode-density design; do not treat prime lengths alone as quality proof.
- https://www.dafx.de/paper-archive/2004/P_061.PDF — circuit-derived nonlinear ladder model. Current filters are not verified hardware emulations.

Next: finish combined CI, retain exact result SHA; collect baseline metrics/WAVs; fix confirmed predelay fault with regression; audit rack identity gain and reverb decay, aliasing and filter response before curating presets. Complete all #11 and #7 requirements including 500 curated presets, full six-op/32-algorithm FM and legal SYX, true local inference, UI workflows, installer/validators and owner listening. No claim that any of those gates is closed by this checkpoint.
