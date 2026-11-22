# Feature Gap Analysis

Statuses: **VERIFIED** (implemented and tested), **IMPLEMENTED** (code exists, runtime behaviour not independently verified),
**PARTIAL**, **MISSING**, **BLOCKED** (needs a dependency, credential or environment), **RESEARCH REQUIRED**.
Competitor presence uses vendor statements from COMPETITIVE_FEATURE_MATRIX.md (not independently verified).
A declared parameter or GUI control is not counted as a working feature unless a test exercises it.

## Synthesis

| Feature | Status | Evidence | Competitors stating it | Relative position |
|---|---|---|---|---|
| Wavetable oscillator (mipmapped, morph, warps) | VERIFIED | renders, pitch, aliasing tests | Serum 2, Vital, Pigments, Falcon, Phase Plant | comparable core; far fewer tables (27 vs 288 stated for Serum 2) |
| Virtual analog (polyBLEP saw / pulse / PWM, tri, sine) | VERIFIED | pitch + aliasing measurements | Diva, Pigments | functional; no circuit-level oscillator models |
| Sample + multisample (SFZ import, loops) | VERIFIED (render) / IMPLEMENTED (SFZ edge cases) | mode render tests | Serum 2, Pigments, Falcon | no mapping editor UI |
| Granular | VERIFIED (render) | mode render test | Serum 2, Pigments, Falcon | no visualisation |
| Spectral (harmonic resynthesis) | VERIFIED (render) | mode render test | Serum 2, Falcon | harmonic sounds only |
| Sub / noise, FM / PM / RM / AM / sync, PWM, wavefold | IMPLEMENTED | used by presets, finite-output tests | most | per-oscillator only |
| 16-voice unison per oscillator | VERIFIED (render, CPU measured) | benchmark | most | CPU heavy (no SIMD) |
| Dedicated additive engine | MISSING | - | Pigments (Harmonic) | below |
| 6-operator FM with algorithms | IMPLEMENTED (Linux tests) | Dexed Modern comparison, 57 cases | Falcon (FM Explorer), Phase Plant | below |
| Phase distortion engine | IMPLEMENTED (Linux tests) | TestExtraEngines | Serum 2 (PD in oscillator) | below |
| Physical / modal modelling | IMPLEMENTED (Linux tests) | TestExtraEngines (tuning, T60, materials) | Pigments (Modal), Falcon (Pluck), reFX Rippler | below |
| Vector synthesis / wave sequencing | IMPLEMENTED (Linux tests) | TestExtraEngines | n/s | - |
| Four-layer architecture, splits, velocity layers | IMPLEMENTED (Linux tests) | TestLayers | Rapid (8 layers), Synplant (layer mode) | below |
| MPE (lower zone) | IMPLEMENTED (Linux tests) | TestSessionBehaviour | Vital, Omnisphere 3, Phase Plant, Diva | below |
| MIDI Learn | IMPLEMENTED (Linux tests) | TestSessionBehaviour | n/s | below (common feature) |
| Microtuning (Scala) | IMPLEMENTED (Linux tests) | TestTuning | Repro (microtuning section) | below |
| Oversampling 1x / 2x / 4x | VERIFIED | aliasing per quality, latency | n/s | 8x MISSING |

## Filters, modulation, effects

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| 9 filter models, 6 modes, ZDF / TPT | VERIFIED (measured) | AUDIO_QUALITY_REPORT.md | Diva states 5 models (ZDF), Omnisphere 3 "36 new types", Pigments 16 types |
| Component tolerance / drift | IMPLEMENTED | code | not measured against hardware |
| 4 envelopes, 10 LFOs, 8 engine macros, 32 matrix slots, drag-and-drop | VERIFIED (automation fuzz, editor tests) | tests + pluginval | Pigments states "unlimited connections" |
| 18 effects, serial / parallel routing, reorder | VERIFIED (finite / bounded) | effect tests | Falcon states 120+, Pigments 20 |
| Shimmer / plate / reverse / gated / nonlinear reverb, BBD, rotary, resonator, transient shaper, dynamic EQ, spectral FX | MISSING | - | |
| Arpeggiator, polyphonic step sequencer | VERIFIED (presets render as played) | library tests | piano-roll editor MISSING |

## AI

| Feature | Status | Evidence |
|---|---|---|
| Text-to-preset (offline rule-based, English + Hebrew) | VERIFIED | tests |
| Cloud / local LLM providers (Claude, OpenAI, Ollama, LM Studio, compatible) | IMPLEMENTED, protocol TESTED AGAINST MOCK; live **BLOCKED** (no keys / servers in build environment) | TestAIProviders |
| Validated, atomic patch transactions; undo / redo; A/B; change highlights; stale-result protection; cancellation | VERIFIED | TestAI, TestAIProviders |
| Closed loop render / measure / refine for perceptual goals | VERIFIED (runs and reports measurements) | controller test |
| Audio-to-preset matching | VERIFIED (runs), quality MEASURED (eval mean similarity 0.548, internal metric) | matching benchmark |
| AI wavetable / harmonic design | PARTIAL (harmonic-frame wavetable operation) | PatchOps |
| AI modulation / effects design, variations, arps / sequences via operations | IMPLEMENTED (operations exist); quality not benchmarked | PatchOps |
| Semantic preset search, AI sample selection UI, AI morphing UI | MISSING | - |
| AI benchmark dataset with listening evaluation | MISSING | - |

## Product / validation

| Item | Status |
|---|---|
| Windows x64 VST3 + Standalone build | VERIFIED (CI) |
| pluginval strictness 10, Steinberg validator | VERIFIED (CI) |
| Installer / uninstaller | VERIFIED (CI) |
| FL Studio 25 | BLOCKED (no authorised FL Studio environment) |
| Code signing | MISSING |
| 2,000 presets / 500 wavetables / licensed multisample library | MISSING (85 / 27 / 9 synthesised instruments) |
| User manual | PARTIAL (in-app quick help; README) |
