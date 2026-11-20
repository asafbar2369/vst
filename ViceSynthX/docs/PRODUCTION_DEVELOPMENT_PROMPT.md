# VICE SYNTH X — Complete Professional Synthesizer Development & Research

Act as a senior DSP engineer, synthesizer architect, C++/JUCE developer, AI integration engineer, GUI designer and QA engineer. Continue developing the existing project without removing functionality or starting over.

## 1. Mandatory competitor research
Read and analyze product documentation and manuals before implementing new features.

- Serum 2: https://www.xferrecords.com/products/serum-2
- https://xferrecords.com/web-manual/serum-2/welcome
- https://xferrecords.com/web-manual/serum-2/exploring-sound-design-in-serum
- Pigments: https://www.arturia.com/products/software-instruments/pigments/overview
- Omnisphere 3: https://www.spectrasonics.net/products/omnisphere/overview.php
- https://www.spectrasonics.net/products/omnisphere/library.php
- Falcon: https://www.uvi.net/falcon
- https://manual.uvi.net/falcon/en/
- Vital: https://vital.audio/
- Synplant 2: https://soniccharge.com/synplant

Create a detailed feature-comparison matrix, missing features, priorities, risks and competitive advantages. Do not copy proprietary code, samples or graphical assets. Verify URLs and documentation before reliance.

## 2. Advanced synthesis engines
Implement, complete and verify:
- Wavetable, virtual analog, granular, spectral resynthesis, additive, six-operator FM with flexible algorithms, phase modulation and phase distortion, physical modeling, modal, sample and multisample synthesis.
- Multilayer synthesis with keyboard and velocity splits; advanced oscillator/filter routing.
- Selectable 1x/2x/4x/8x oversampling; full MPE, microtuning, MIDI Learn.

References:
- https://docs.juce.com/
- https://docs.juce.com/develop/classjuce_1_1AudioProcessor.html
- https://github.com/steinbergmedia/vst3sdk
- https://midi.org/mpe-midi-polyphonic-expression
- https://huygens-fokker.org/microtonality/scales.html

Research peer-reviewed DSP papers on band-limited oscillators, anti-aliasing, zero-delay feedback filters, nonlinear circuit modeling, FFT resynthesis, physical modeling and granular synthesis.

## 3. Professional sound design
- Advanced 2D/3D wavetable editor, audio-to-wavetable conversion, FFT harmonic and spectral editing, wavetable morphing.
- Visual drag-and-drop modulation, advanced modulation matrix, multiple envelopes, editable LFO curves.
- Multitrack arpeggiator, piano-roll sequencer, MIDI pattern import/export.
- Preset morphing, intelligent randomization, A/B comparison, undo/redo.

Reference: https://support.xferrecords.com/article/59-converting-samples-to-wavetables

## 4. Real AI integration
Implement real functionality, not simulated responses.
- Anthropic: https://platform.claude.com/docs/en/api/overview
- https://platform.claude.com/docs/en/api/messages/create
- https://platform.claude.com/docs/en/build-with-claude/structured-outputs
- OpenAI: https://developers.openai.com/api/docs/guides/structured-outputs
- Ollama: https://github.com/ollama/ollama
- https://ollama.com/blog/structured-outputs
- LM Studio: https://lmstudio.ai/docs/app
- https://lmstudio.ai/docs/developer/openai-compat/structured-output

Verify current model IDs, authentication, API versions, endpoints and schemas.
- Text-to-preset, audio-to-preset matching, conversational editing, AI wavetable generation, AI modulation and effects design, preset morphing, semantic preset search, MIDI generation.
- English and Hebrew; real local LLM; built-in rule-based fallback.
- Rendering and iterative sound matching, automatic acoustic measurement and refinement.
- Valid AI patch operations must modify actual parameters and generate playable audio. The AI must understand the current patch, parameter ranges, routing, architecture and effects. Validate every response; never expose credentials or interrupt real-time audio with network requests.

See AI_COMPLETION_PROMPT.md for specific acceptance criteria.

## 5. Audio-to-preset reconstruction
Research Synplant 2 (https://soniccharge.com/synplant); implement independent sound matching. Analyze FFT/STFT, harmonic amplitudes, spectral envelope, fundamental pitch, ADSR, transients, stereo width, noise, pitch variation and modulation movement. Render candidates and iteratively optimize. Report measured similarity and limitations; do not claim perfect reconstruction without evidence.

## 6. Factory library
Target 2,000 professional original presets, 500 original wavetables, high-quality original or properly licensed multisamples, analog and digital 1980s/1990s sounds, modern basses, leads, pads, keys, plucks, cinematic textures, percussion and experimental sounds.

Study Roland D-50, JD-800, JV-1080, Jupiter-8; Yamaha DX7, SY77, SY99; Korg M1 and Wavestation; Prophet-5 and Juno-106; Fairlight CMI and Synclavier. Prioritize musical quality over quantity. Avoid unauthorized ROMs, samples and proprietary assets; maintain a licensing manifest.

## 7. Professional GUI
Preserve VICE SYNTH X visual identity. Fully functional interactive controls, resizable HiDPI/4K interface, real-time oscilloscope and spectrum analyzer, 3D wavetable visualization, envelope and LFO editors, advanced preset browser, AI chat, searchable modulation destinations, tooltips, MIDI keyboard, CPU meter and visible parameter values. No decorative controls pretending to function.

## 8. DSP quality and optimization
Band-limited oscillators and anti-aliasing, high-quality interpolation, stable nonlinear filters, accurate modulation, sample-accurate MIDI, click-free transitions, SIMD/AVX2 with compatibility fallback, real-time thread safety, denormal/NaN/infinity protection, stable maximum polyphony, accurate latency and uninterrupted audio during AI.
Test at 44.1/48/88.2/96/192 kHz and varied buffer sizes with demanding polyphonic patches. Measure aliasing, frequency response, distortion, CPU, allocation, latency and peaks.

## 9. Mandatory compilation and validation
- Pluginval: https://github.com/Tracktion/pluginval
- https://github.com/Tracktion/pluginval/releases
- VST3 SDK: https://github.com/steinbergmedia/vst3sdk
- https://steinbergmedia.github.io/vst3_dev_portal
- FL Studio: https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm
- GitHub Actions: https://docs.github.com/en/actions/tutorials/store-and-share-data
- Inno Setup: https://jrsoftware.org/inno-setup/documentation/contents.htm

Windows x64 Release VST3 and Standalone EXE, dedicated CI, automated DSP/GUI/AI integration tests, Pluginval strictness 10, Steinberg validator, FL Studio 25 actual host testing where available, project recall, automation, memory leaks, race conditions, long-duration stability and installation/uninstallation. Never mislabel mock API results as live connectivity. Never claim FL Studio compatibility without running the actual host.

## 10. Final deliverables
VICE SYNTH X.vst3, VICE SYNTH X.exe, VICE SYNTH X Setup.exe, original factory presets and wavetables, source code, user manual, technical documentation, research bibliography with URLs, benchmark report, QA logs and SHA-256 checksums.

## 11. Development rules
First complete, compile and validate existing implementation before expanding. Work in isolated branches. Implement real features, not placeholders. Record research, decisions, tests and unresolved issues. Continue implementation, compilation, testing, optimization and fixes until achievable criteria are met. Report missing credentials, commercial software or test environments honestly. Do not access, control or automate my personal computer.

The objective is a competitive original production-grade AI-powered synthesizer with professional audio quality, advanced hybrid synthesis, intuitive sound design and genuine AI-assisted sound creation.
