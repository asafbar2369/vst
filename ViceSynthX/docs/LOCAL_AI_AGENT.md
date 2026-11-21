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

**Not verified:** inference against a real Ollama or LM Studio model has not been run in this environment (no local
model server here). The owner's machine was not accessed, by instruction. How good the results are depends on the
model the user installs; small models may follow the schema poorly.
