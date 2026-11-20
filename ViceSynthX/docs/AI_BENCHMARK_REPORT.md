# AI Benchmark Report

Status per item: **TESTED AND VERIFIED** only where an automated test ran in this environment; live provider
connectivity is **UNVERIFIED** (no API keys or local model servers were available to this build environment).

## 1. What was verified

| Item | Evidence | Status |
|---|---|---|
| Structured patch operations: malformed responses rejected, invalid operations reject the whole transaction, values range-checked | `Tests/TestAI.cpp` | TESTED AND VERIFIED |
| Request formats: Anthropic Messages (`anthropic-version: 2026-10-21`, `output_config.format` json_schema, no sampling params), OpenAI Chat Completions (`response_format` json_schema / json_object), Ollama native (`/api/chat` with `format` schema, `stream: false`), LM Studio (OpenAI-compatible json_schema) | `Tests/TestAIProviders.cpp` (request bodies) + format sources in section 4 | TESTED (format) |
| Protocol handling: success parsing, model listing, auth errors (401), refusals, missing models (404), schema fallback (400 -> json_object), invalid JSON content, unreachable server | `Tests/TestAIProviders.cpp` against a **local mock HTTP server** | TESTED AGAINST MOCK - not live |
| Cancellation of an in-flight HTTP request | mock server delays 4 s; request cancelled after 0.3 s returns in < 2.5 s | TESTED AGAINST MOCK |
| Stale-result protection, A/B compare, undo / redo, one request at a time, missing-key offline fallback | controller tests with an in-memory host | TESTED AND VERIFIED |
| Offline designer, English and Hebrew prompts (new patch, edits, variations, clarification question) | `TestAIProviders` (Hebrew), earlier English prompt runs | TESTED AND VERIFIED (offline) |
| Closed-loop render / measure / refine for perceptual goals (darker, wider, ...) | controller test requires a measurement for "darker and wider" | TESTED AND VERIFIED |
| Live Claude / OpenAI / Ollama / LM Studio requests | not run | **UNVERIFIED** |
| FL Studio 25 behaviour of the AI workspace | not run | **UNVERIFIED** |

## 2. Audio-to-preset matching (measured)

Method: factory presets are rendered as reference audio (they are synthesizable by construction, so this is an optimistic
best case), analysed, converted to an initial patch and refined with 40 candidate renders. Development and evaluation sets
are disjoint; nothing was tuned on the evaluation set in this release. **Similarity is the internal feature metric
(spectral centroid / harmonics / envelope / brightness / width / modulation, 0..1). It has not been validated against
human listening and must not be read as "percent perceived accuracy".**

### Sound matching benchmark

| Measurement | Value | Unit |
|---|---:|---|
| development Ladder Saw Stack initial similarity | 0.141 | 0..1 |
| development Ladder Saw Stack final similarity | 0.477 | 0..1 |
| development Ladder Saw Stack renders / time | 41.000 | renders in 5.9 s |
| development Glass Harmonics initial similarity | 0.008 | 0..1 |
| development Glass Harmonics final similarity | 0.066 | 0..1 |
| development Glass Harmonics renders / time | 41.000 | renders in 7.0 s |
| development Kalimba Sun initial similarity | 0.027 | 0..1 |
| development Kalimba Sun final similarity | 0.567 | 0..1 |
| development Kalimba Sun renders / time | 41.000 | renders in 4.1 s |
| development mean initial similarity | 0.059 | 0..1 |
| development mean final similarity | 0.370 | 0..1 |
| evaluation Diode Growler initial similarity | 0.431 | 0..1 |
| evaluation Diode Growler final similarity | 0.746 | 0..1 |
| evaluation Diode Growler renders / time | 41.000 | renders in 5.9 s |
| evaluation PWM Velvet initial similarity | 0.635 | 0..1 |
| evaluation PWM Velvet final similarity | 0.736 | 0..1 |
| evaluation PWM Velvet renders / time | 41.000 | renders in 7.2 s |
| evaluation FM Tine Machine initial similarity | 0.045 | 0..1 |
| evaluation FM Tine Machine final similarity | 0.284 | 0..1 |
| evaluation FM Tine Machine renders / time | 41.000 | renders in 7.2 s |
| evaluation Ensemble Section initial similarity | 0.347 | 0..1 |
| evaluation Ensemble Section final similarity | 0.423 | 0..1 |
| evaluation Ensemble Section renders / time | 41.000 | renders in 6.8 s |
| evaluation Synthwave Lead initial similarity | 0.278 | 0..1 |
| evaluation Synthwave Lead final similarity | 0.585 | 0..1 |
| evaluation Synthwave Lead renders / time | 41.000 | renders in 6.2 s |
| evaluation Deep Space Drone initial similarity | 0.395 | 0..1 |
| evaluation Deep Space Drone final similarity | 0.513 | 0..1 |
| evaluation Deep Space Drone renders / time | 41.000 | renders in 7.2 s |
| evaluation mean initial similarity | 0.355 | 0..1 |
| evaluation mean final similarity | 0.548 | 0..1 |

Observations: refinement improved every case over the analysis-based guess (evaluation mean 0.355 -> 0.548). Analog-style
sounds reach 0.59-0.75; FM / glassy / bell timbres stay low (0.07-0.28) because the initial guess maps harmonic spectra to a
wavetable and the optimiser does not yet search FM routings. Each match took 4-7 s for 41 renders on the test machine.

## 3. Comparison with other AI sound-design tools

No competitor tool was run in this environment, so there are **no comparative results**. Documented facts from vendor pages
(retrieved 2026-11-20, see MARKET_COMPARISON.md): Sonic Charge Synplant 2 "Genopatch crafts synth patches from audio
recordings, using AI to find optimal synth settings based on your source sample" and "runs entirely on your local computer";
Native Instruments Massive XL advertises a "Preset Generator" that turns "any sample into a real, playable preset ... stem
separation included". A matched-condition comparison (same reference set, listening panel) remains to be done.

Not yet built: the original benchmark dataset described in the brief (analog / digital basses, FM pianos, pads, brass,
strings, choirs, bells, plucks, granular, percussion) with recorded or third-party references, repeated-run statistics,
API cost tracking and controlled listening evaluations.

## 4. Sources used to verify provider formats

| Provider | Source | Version / date |
|---|---|---|
| Anthropic | Claude API reference bundled with the development tools (curl examples, models catalogue, API-version migration notes) | API version 2026-10-21; model alias `claude-opus-5-5` listed as active |
| OpenAI | github.com/openai/openai-openapi `openapi.json` (`ResponseFormatJsonSchema`, `ListModelsResponse`, message `refusal`) | spec 2.3.0, commit a01e367 (2026-11-20) |
| Ollama | github.com/ollama/ollama `docs/api.md`, `docs/capabilities/structured-outputs.mdx`, `docs/api/openai-compatibility.mdx` | commit 5187f59 (2026-11-20) |
| LM Studio | github.com/lmstudio-ai/docs `1_developer/3_openai-compat/structured-output.md`, `models.md`, `index.mdx` | commit 8c7159b (2026-10-20) |

The official sites developers.openai.com, ollama.com, lmstudio.ai and platform.claude.com were not reachable from the build
environment; the GitHub-hosted specifications above were used instead.
