# VICE SYNTH X: market review and integration checkpoint

Review date: 2026-11-21 (Asia/Jerusalem). Repository baseline: 19b250e75724fba7309d958ef9b09dd38d755a76.
Integration branch: chatgpt/vice-synth-x-codex-continuation; PR #10.
This compares documented capability, not independently measured competitive sound quality.
Vendor pages were accessed during this review. Vendor release labels are reported as shown; no competitor binary was executed.

| Reference | Documented capability | VICE SYNTH X evidence / gap |
|---|---|---|
| [Serum 2](https://www.xferrecords.com/products/serum-2) | Wavetable, sample, multisample, granular and spectral oscillators; sample looping and slicing | Six oscillator modes exist. Import consistency, editing depth and resource lifetime require more work; similar mode names do not establish quality parity. |
| [Pigments](https://www.arturia.com/store/software-instruments/pigments) | Parallel engines including harmonic/additive; visual modulation, sequencing, MPE and microtuning | Modulation and sequencing exist. Dedicated additive, MPE and microtuning are absent in the reviewed baseline. |
| [Falcon](https://www.uvi.net/falcon) | Vendor page currently lists 28 oscillators, 120+ effects, 1,700 presets and 500+ wavetables; flexible layering | VICE baseline has 18 effects, 85 presets and 27 wavetables. Four independent layers and additional synthesis engines remain mandatory, incomplete requirements. |
| [Omnisphere 3](https://www.spectrasonics.net/products/omnisphere/overview.php) | Four sound sources per patch, eight patches per multi, Quadzone movement, MPE and extensive curated content | VICE lacks the required independent layer architecture and MPE. A new recorded sample library is explicitly out of VICE scope; importing the owner's files is the relevant comparison. |
| [Vital](https://vital.audio/) | Spectral warping, visual modulation, wavetable editing, MPE, tuning-file import and SIMD unison | VICE's visual editing and CPU optimization need measured development. Existing CPU results are from one cloud machine, not a matched Vital benchmark. |
| [Phase Plant](https://kilohearts.com/products/phase_plant) | Modular generators, audio-rate cross/self modulation, three effects lanes and MPE | VICE has fixed routing and cross-modulation; open-ended routing and expression parity are not established. |
| [Synplant 2](https://soniccharge.com/synplant) | Local Genopatch infers synthesizer settings from reference audio | VICE has a local feature-matching search and cloud/local provider interfaces. Internal evaluation similarity 0.548 is not perceptual accuracy or a Synplant comparison. Successful live provider generation remains unverified. |

## Engineering conclusions

1. The baseline is an implemented instrument with existing Windows validation evidence, not a completed flagship.
2. Competitive acceptance must cover workflows: import -> playable chromatic keyboard -> edit -> save -> reopen -> missing-file recovery.
3. The next architecture gates remain six-operator FM, additive/modal/physical synthesis, vector/wave sequencing, four independent layers, MPE, microtuning, MIDI Learn and 8x quality.
4. The original target of 2,000 curated presets and 500 original wavetables remains unchanged. Counts alone cannot establish distinctness or musical quality; listening review and macro/velocity testing are required. No new factory sample pack.
5. Preserve the original Windows release gates and exact-commit logs. A passing older commit does not validate a new integration.

## Benchmark protocol

Use legally available competitor builds on the same authorized isolated machine, same sample rate/buffer, matched polyphony, oscillator count and effect workload. Compare level-matched audio, tuning error, alias energy, filter response, note-stealing artifacts, CPU percentiles, memory, latency and recall. Record product versions and exact VICE commit. Run blind listening separately from numerical tests.

No matched-condition competitor runtime, listening test, or FL Studio test was performed in this review.

## Credential finding

Claude's latest commit and QA report record an OpenAI HTTP 401 invalid_api_key result. This verifies the error path only. The earlier chat's suggestion that a secret was for AI testing is plausible but does not identify the exact requested secret. No secret value was requested, read or disclosed by this continuation.
