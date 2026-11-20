# VICE SYNTH X — Production-grade AI completion

Complete and fully validate the VICE SYNTH X AI sound design system to production-grade standards.

## AI integration
- Implement and verify real Anthropic Claude API connectivity.
- Implement and verify OpenAI API connectivity.
- Support local LLMs through Ollama and LM Studio.
- Add automatic provider and model discovery where supported.
- Verify current API endpoints, model IDs and request schemas against actual documentation.
- Never simulate successful API responses in production.

## Sound design
- Allow AI to control every relevant synthesis parameter.
- Generate playable presets from natural-language prompts.
- Modify existing sounds through conversational instructions.
- Support English and Hebrew.
- Analyze imported reference audio and generate matching synth patches.
- Render generated sounds, measure their acoustic properties and refine the results automatically.
- Allow users to preview, save, edit, undo and redo AI-generated presets.

## Offline functionality
- Support a real locally running language model without paid API access.
- Retain the built-in rule-based designer as an emergency fallback.
- Clearly distinguish local LLM, cloud AI and rule-based modes.

## Security and stability
- Encrypt API credentials securely.
- Never embed private API keys in source code or binaries.
- Keep all AI network requests and analysis outside the real-time audio thread.
- Implement reliable cancellation, timeouts, retry limits and error handling.
- Prevent invalid AI responses from crashing or corrupting the synthesizer.
- Ensure AI requests never interrupt audio playback.

## Mandatory QA
- Test actual provider connectivity and complete prompt-to-preset workflows.
- Test every synthesis parameter exposed to AI.
- Test invalid responses, disconnected internet, incorrect credentials and unavailable models.
- Verify preset persistence and project recall.
- Compile Windows x64 VST3 and Standalone EXE.
- Run Pluginval and host compatibility tests, targeting FL Studio 25.

Do not declare the AI system complete until actual end-to-end tests pass. Clearly distinguish verified functionality from untested functionality.

Continue implementation, compilation, testing and bug fixing until all achievable acceptance criteria are satisfied. Do not access or control my personal computer.
