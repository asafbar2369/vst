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
| Six-operator FM, 32 algorithms, .syx import with checksum | Synth/FmVoice.*, GUI/FmEditor.* | MSFA "Modern" engine (Apache 2.0, vendored from Dexed 1ed5a4d) + own SysEx codec + editor | TestFm, TestGui FM page | pass (Linux) |
| Reverb programs (hall / plate / room / chamber / ambient / nonlinear / gated / reverse) | FX/ReverbEngine.* | 16-line FDN + multi-tap | TestReverb (RT60, density, comb periodicity) | pass (Linux) |
| Local AI setup wizard + render-analyse-refine agent | AI/AIController.cpp, AI/PatchOps.cpp, GUI/AISettingsPanel.cpp | worker-thread detection, tool loop | TestAIProviders (mock Ollama) | pass (mock); live UNVERIFIED |
| INIT, SHOW / HIDE panels, single SETTINGS, waveform arrow, instant arrow-key loading | GUI/HeaderHero.*, PluginEditor.*, GUI/OscSourceMenu.*, GUI/BrowserLibrary.* | | TestGui | pass (Linux) |
| Physical (waveguide string), Modal, Phase Dist, Additive modes | Synth/ExtraEngines.*, Synth/Oscillator.cpp, GUI/SynthPanels.cpp, GUI/WaveDisplay.cpp | four new oscillator modes, 10 parameters per oscillator | TestExtraEngines, TestGui | pass (Linux): tuning ±1 ct, T60 within 0.2 %, modal ratios within 0.012 %, PD THD −145 dB at Amount 0 |
| Vector synthesis, wave sequencing | Synth/Voice.cpp, GUI/VectorPage.* | equal-power X/Y mix of A / B / C / Sub or Noise; OSC A position step sequencer | TestExtraEngines, TestGui | pass (Linux): corner isolation −117 dB, centre −6.02 dB each |
| Microtuning (Scala .scl / .kbm) | Core/Tuning.*, Synth/SynthEngine.cpp, GUI/TabPages.cpp | lock-free 128-key pitch table | TestTuning | pass (Linux): 19-EDO ±0.007 ct |
| MIDI Learn | PluginProcessor.cpp, GUI/Widgets.cpp | CC → parameter, same-block response, host gestures | TestSessionBehaviour | pass (Linux) |
| MPE (lower zone) | Synth/SynthEngine.cpp, Synth/Voice.cpp | per-note bend / pressure / CC 74 | TestSessionBehaviour | pass (Linux) |
| BBD chorus / delay, shimmer, rotary, resonator, transient shaper, spectral | FX/ExtraEffects.cpp | 7 effects, completed rack order | TestExtraEffects, TestFxLaws | pass (Linux), measured |
| Four layers, splits / velocity layers | Synth/SoundLayer.*, PluginProcessor.cpp, GUI/LayersPage.* | A (editor) + B/C/D complete sounds; key / velocity range, transpose, level, pan; project recall | TestLayers, TestGui | pass (Linux): split / velocity isolation > 100 dB, level -6.00 dB |
| Audio-rate filter FM | Synth/Filters.*, Synth/Voice.cpp, GUI/SynthPanels.cpp | per-sample exponential cutoff modulation from OSC A/B/C, Sub, Noise | TestFilterFm | pass (Linux): sidebands -16.7 dB (1 oct), -9.2 dB (3 oct), -138.6 dB off |
| Arp / sequencer ratchets | Synth/ArpSeq.*, Core/PatchState.*, Presets/PresetFormat.cpp, GUI/TabPages.cpp | 1-4 repeats per step, sample-accurate across blocks | TestArpRatchet | pass (Linux): spacing error <= 1 sample |
| Preset morphing | PluginProcessor.cpp (applyMorph), GUI/LayersPage.* | Morph parameter, target, normalised interpolation, switches at 50 % | TestLayers | pass (Linux): -5.70 dB at 50 % (expected -5.70) |

## Codex integration on 2026-11-21

PR #10 is based on Claude 19b250e. Earlier results above apply only to their named commits.
The following new changes require exact-commit CI verification; they are not a flagship completion claim.

| Requirement | Source | Regression | Status before CI |
|---|---|---|---|
| Silver controls and SIZE | Theme.cpp, HeaderHero.*, PluginEditor.cpp | TestGui rotary render and scale checks | IMPLEMENTED UNTESTED |
| Preserve a valid resource after failed import | PatchState.cpp | TestCoreDSP failed/superseded loads | IMPLEMENTED UNTESTED |
| Owner-thread publication; stale-load suppression | PatchState.cpp | clear/reset during queued loads | IMPLEMENTED UNTESTED |
| LOAD SAMPLE; waveform and keyboard drops | SynthPanels.cpp, WaveDisplay.cpp, MacroPerformance.cpp, PluginProcessor.cpp | TestGui single and multi-file import | IMPLEMENTED UNTESTED |
| Single-sample chromatic MIDI 0-127 | Oscillator.cpp | all 128 notes, measured sine crossings | IMPLEMENTED UNTESTED |
| Full-range multi-file zones and recall | SampleBank.cpp, PatchState.cpp | two note-named fixtures, boundary and missing-file checks | IMPLEMENTED UNTESTED |
| Short forward/ping-pong loops at large pitch ratios | Oscillator.cpp | multi-boundary loop regression | IMPLEMENTED UNTESTED |
| Original stereo data; no silent truncation | SampleBank.cpp | stereo fixture; loader validation | IMPLEMENTED UNTESTED |
| Market comparison | MARKET_REVIEW_2026-11-21.md | official vendor documentation review | DOCUMENTED; no competitive runtime benchmark |

No new factory sample library was created. The expanded synthesis architecture, four independent layers,
MPE/microtuning/MIDI Learn, 8x oversampling, advanced editors, 2,000 curated presets and 500 original
wavetables remain mandatory and are not marked complete by this integration.

## Superseding owner decision: English-only AI

The owner explicitly disabled Hebrew AI in the VST on 2026-11-21.
Hebrew prompt interpretation, Hebrew replies and automatic RTL layout are removed from the active product.
The lexicon is removed from the build. Controller and provider entry points reject Hebrew requests before
network access, and response validation rejects Hebrew output without applying patch operations.
TestAIProviders now tests this English-only policy, English generation and zero provider calls for rejected input.
Earlier Hebrew QA rows above are historical evidence of the previous scope, not current acceptance criteria.
