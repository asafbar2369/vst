# Requirements Traceability

Requirement -> source -> implementation -> test -> actual result (2026-11-20). "CI #38" = GitHub Actions Windows run on
commit 59eed18; "Linux" = development container run on 9104fdb.

| Requirement | Source file(s) | Implementation | Test | Result |
|---|---|---|---|---|
| Windows x64 VST3 + Standalone | CMakeLists.txt, .github/workflows/vice-synth-x-windows.yml | JUCE 9.0.6 plug-in target | CI build | built (CI #38) |
| Installer / uninstaller | installer/ViceSynthX.iss | Inno Setup x64 | CI silent install / uninstall | pass (CI #38) |
| pluginval strictness 10 | workflow | pluginval v1.0.4 | CI | SUCCESS (CI #38) |
| Steinberg VST3 validator | workflow | vst3sdk v3.8.1_build_84 validator | CI | 47 / 0 (CI #38) |
| 6 oscillator modes produce valid audio | Source/Synth/Oscillator.cpp | OscVoice | TestCoreDSP, TestMeasurements | pass (Linux, CI) |
| Band-limiting / oversampling | Oscillator.cpp, SynthEngine.cpp | polyBLEP, 1x/2x/4x | aliasing measurement | -27 / -39 / -46 dB (Linux) |
| 9 analog-inspired filters, stable | Source/Synth/Filters.cpp | TPT models | filter measurements | slopes, self-osc, THD, stability pass (Linux, CI) |
| 18 effects, finite / bounded | Source/FX/Effects.cpp, EffectsRack.cpp | | effect measurements | pass |
| Arp / sequencer | Source/Synth/ArpSeq.cpp | | presets rendered as played | pass |
| Factory presets valid, consistent loudness | Source/Presets/FactoryPresets.cpp | as-played trims | library measurements | 85 / 85, peaks <= -4 dBFS |
| Preset serialisation | Source/Presets/PresetFormat.cpp | versioned JSON | TestCoreDSP round trip | pass |
| Project recall | Source/PluginProcessor.cpp | get/setStateInformation | state test, pluginval state restoration | pass |
| Automation | PluginProcessor.cpp, Core/Params.cpp | APVTS | random automation test, pluginval | pass |
| Sample rates 44.1-192 kHz, buffers 32-1024 | PluginProcessor.cpp | | processor matrix | pass |
| Voice stealing | SynthEngine.cpp | 32 voices + fade slots | 64-note test | pass |
| Long-run stability | engine | | 5-minute random run | pass |
| Real-time safety (no network / AI / file I/O on the audio thread) | AI/AIController.cpp (worker thread), PluginProcessor.cpp | worker thread + async updater | code review; pluginval thread-safety tests | review + pluginval pass; no sanitiser run |
| AI structured operations validated, atomic | AI/PatchOps.cpp | schema + registry validation | TestAI | pass |
| AI providers (6 modes), discovery, HTTPS policy, cancellation | AI/AIProvider.cpp | raw HTTP | TestAIProviders (mock server) | pass (mock); live UNVERIFIED |
| Stale / cancelled results never applied; one request at a time | AI/AIController.cpp | basePatch check, cancel flag | controller tests | pass |
| Hebrew prompts, RTL | AI/HebrewLexicon.cpp, GUI/AIDesignerPanel.cpp | lexicon, JUCE bidi text | Hebrew tests | pass (offline); RTL visual not screenshot-tested |
| Consent before sharing reference analysis; audio never uploaded | AIController.cpp, AISettingsPanel.cpp | shareReferenceAnalysis | code review | IMPLEMENTED (no automated test yet) |
| Secure key storage | AI/SecureStorage.cpp | DPAPI on Windows | - | IMPLEMENTED (not tested in CI) |
| Audio-to-preset matching | Analysis/SoundMatcher.cpp | analysis + render-optimise | matching benchmark | eval mean similarity 0.548 (internal metric) |
| Cinematic GUI, all controls bound to parameters | GUI/*, PluginEditor.cpp | 1648 x 928 artboard | TestGui (knob bindings, views, scaling) | pass (Linux, CI) |
| FL Studio 25 compatibility | - | - | - | UNVERIFIED |
| MPE, MIDI Learn, microtuning, layers, additive / FM-6op / physical engines | - | - | - | NOT IMPLEMENTED |
