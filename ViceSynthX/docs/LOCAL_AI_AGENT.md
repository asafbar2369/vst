# Local AI agent (Ollama / LM Studio), no cloud key (2026-11-21)

## Setup wizard
Go to SETTINGS > AI SOUND DESIGNER and press **DETECT LOCAL AI**. On the worker thread, VICE SYNTH X contacts only two
local servers at their configured addresses (by default `http://localhost:11434` for Ollama and
`http://localhost:1234/v1` for LM Studio) and lists the models each one reports.
- **A server with a model answers:** that server and its first model are selected, unless the current model is
  already one of them. The report shows each server's status and model list.
- **A server runs without a model, or nothing answers:** the report shows the exact setup steps. For example:
  - install Ollama and run `ollama pull qwen2.5:7b-instruct`;
  - or start LM Studio's local server.

  The clearly labelled **offline rule-based designer** stays active meanwhile. It is a fixed English vocabulary, not a
  language model, and the provider list and the AI panel badge say so.
- **Diagnostics:** TEST CONNECTION and REFRESH MODELS. Errors such as connection refused, HTTP status and timeouts are
  shown in plain text.

## Tool-assisted sound design (render, analyse, refine)
With an AI provider selected, each request runs this loop:
1. The model answers with **structured patch operations**. A JSON schema is enforced where the server supports it.
   The operations are validated against the parameter registry, and invalid ones are rejected.
2. **Tool step:** the host renders the proposed patch offline (C4, velocity 100, 1.5 s + 1 s tail) and analyses the
   audio:
   - peak / RMS / crest;
   - pitch and confidence;
   - spectral centroid at the start and end, roll-off and flatness;
   - attack / decay / sustain / release;
   - tremolo / vibrato;
   - stereo width and correlation;
   - warnings for clipping, silence and negative correlation.

   The measurements are sent back to the model as `TOOL RESULT render_and_analyse`, together with the original
   request and the conversation so far.
3. The model either answers `answer_only` (satisfied) or returns corrective operations, which are applied to the patch
   under review. A review can never restart from an init patch.
4. Steps 2-3 repeat up to **Agent steps** times (Settings, 0-4, default 2).
5. The final patch is one undoable step. The chat shows every agent step and the final measurements.

**Safety:**
- Nothing the model writes is executed; only validated parameter operations are applied.
- Rendering and analysis run on the AI worker thread, never in the audio callback.
- Reference audio is never uploaded. The analysis numbers go to *cloud* providers only with explicit consent, and are
  always available to local ones.
- The AI is English-only (owner decision 2026-11-21).

## Verification
`Tests/TestAIProviders.cpp`, using a mock Ollama server:
- The review request contains `render_and_analyse`, the spectral centroid, the original request and the model's first
  answer. The loop stops when the model is satisfied (2 calls). The first answer is applied.
- A 3-step run: the review's `new_patch` is applied as a modification, the corrective value wins, and the whole run is
  one undo step.
- The wizard detects a running server, selects the model `qwen-test:7b` and reports LM Studio as not found. With nothing
  running, the provider stays offline and the report contains the setup steps.

## Live test with real local models (2026-11-25)
A real model was served by **llama-cpp-python's OpenAI-compatible server on LM Studio's address**
(`http://127.0.0.1:1234/v1`), on this cloud container's CPU (2 threads), with a 4,096-token context. This exercises
VICE SYNTH X's LM Studio path (model listing, connection test, chat completions, fallback when `json_schema` is not
supported, validation, offline render); it is not the LM Studio application, and the Ollama API was not exercised.
Harness: `VSX_LIVE_LMSTUDIO=<url> VSX_TEST_ONLY="Live AI providers"` (Tests/TestLiveProviders.cpp). Requests:
"Create a warm early-1990s digital pad", "Create a deep analog bass".

What the live test found and fixed:
- The system prompt was about **14,350 tokens** and was refused by 4,096- and 8,192-token contexts. Local providers now
  get a **compact prompt** (2,832 tokens; parameters as `id range` lines, `*` marks modulation destinations, one format
  example). Cloud providers keep the full prompt.
- Servers without `json_schema` support fell back to JSON mode with the full schema text (1,075 tokens); with the
  compact prompt the fallback adds one line.
- Oscillator **C** could not be addressed by its letter (`removeCharacters ("OSC ")` deleted the C).
- Ollama requested a 32,768-token context; now 16,384 (half the memory).
- A **repair round**: when a model's operations are rejected, the validator's exact errors go back to it once. A
  rejected transaction is still never applied in part.

| Model (Q4_K_M) | Run | Pad | Bass |
|---|---|---|---|
| Qwen2.5 1.5B Instruct | before / after the compact prompt | refused (context) / rejected (invented `env1_delay` source) | refused / rejected (unknown wavetable) |
| Qwen2.5 7B Instruct | 1 (no repair) | rejected (`fx_reverb` as a source) | rejected (`env1_attack` as a source) |
| Qwen2.5 7B Instruct | 2 (repair) | rejected after repair (`osc_a_phase_rand` is not a mod destination) | **playable**: 10 operations, peak -21.7 dBFS, 156 s |
| Qwen2.5 7B Instruct | 3 (repair, `*` markers) | **playable**: 9 operations, peak -17.0 dBFS, 192 s | rejected after repair (invented `osc_a_cutoff`, misspelt `fx_dimention_on`) |

**Result:** with the 7B model the request fits a 4,096 context and 2 of 6 requests gave a playable patch; answers vary
between runs. A 1.5B model does not follow the operation format. Times are for 2 CPU threads; a desktop GPU is much
faster. The validator rejected every invalid operation (no invalid patch was applied). Sound quality of the AI patches
was not judged by listening. The owner's machine was not accessed.
