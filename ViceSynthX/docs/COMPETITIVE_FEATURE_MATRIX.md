# Competitive Feature Matrix

Sources: vendor pages fetched 2026-11-20 (see MARKET_COMPARISON.md for URLs and quotes). "n/s" = not stated on the fetched
page (unknown, not "absent"). VICE SYNTH X column: status from FEATURE_GAP_ANALYSIS.md (V = verified by test, I = implemented,
P = partial, M = missing). Competitor entries are vendor claims, **not independently verified**; no competitor was run.

| Feature | VICE SYNTH X | Serum 2 | Vital | Pigments | Omnisphere 3 | Falcon | Phase Plant | Diva | Synplant 2 |
|---|---|---|---|---|---|---|---|---|---|
| Version on page | 1.0.0 (dev) | 2 | n/s | n/s | 3 | n/s | n/s | 1.4.8 (MPE note) | 2 |
| Price shown | - | $249 | tiers | $199 | n/s | $299 | several | 179 EUR | $149 |
| Wavetable | V | yes | yes | yes | yes ("EDM wavetables") | yes | yes (editor quote) | no | n/s |
| Virtual analog | V | n/s | n/s | yes | n/s | n/s | n/s | yes (5 osc models) | n/s |
| Sample / multisample (SFZ) | V / V | yes / yes (sfz) | n/s | yes (Sample) | yes (deep-sampled) | yes | n/s | no | n/s |
| Granular | V | yes | n/s | yes | n/s | yes (IRCAM) | n/s | no | n/s |
| Spectral / harmonic resynthesis | V (harmonic) | yes | spectral warping | Harmonic engine | n/s | Spectral Stretch | n/s | no | n/s |
| Modal / physical | M | n/s | n/s | Modal | n/s ("Modal" appears as a library name) | Pluck | n/s | no | n/s |
| FM engine (multi-operator) | M (per-osc FM/PM only) | FM warps | n/s | n/s | n/s | FM Explorer | audio-rate FM | n/s | n/s |
| Sample-to-wavetable | P (wavetable from harmonic frames / editor) | yes (Frequency Estimation) | yes (pitch-splice, vocode) | n/s | n/s | n/s | n/s | n/s | n/s |
| Filter models | 9 (V measured) | n/s | n/s | 16 types / 50+ modes | 36 new types | n/s | n/s | 5 models (ZDF) | n/s |
| Modulation matrix | 32 slots, 4 env, 10 LFO, 8 macros (V) | n/s | drag & drop, remap | unlimited connections | Quadzone | deep | yes | 3 env, 2 LFO | DNA editor |
| MPE | M | n/s | yes | n/s | yes | n/s | yes | yes | n/s |
| Microtuning | M | n/s | n/s | n/s | n/s | n/s | n/s | n/s | n/s |
| Arpeggiator / sequencer | V / V | arp + clip sequencer | n/s | generative sequencer | n/s | event processors | n/s | arp | n/s |
| Effects | 18 (V) | flexible routing | n/s | 20 (3 per bus) | 35+ new | 120+ | Snapins | 2 slots | n/s |
| Layers | M | n/s | n/s | n/s | n/s | semi-modular | n/s | no | layer mode |
| Factory presets | 85 | 626+ | 75-400+ | 1700+ | "thousands" | "thousands" | n/s | 1200+ | "hundreds" |
| Factory wavetables | 27 | 288 | 25-150 | n/s | n/s | n/s | n/s | - | - |
| AI text-to-patch | V (offline + 5 LLM modes) | n/s | text-to-wavetable (web service) | n/s | n/s | n/s | n/s | n/s | n/s |
| Audio-to-patch | V (similarity metric, see AI report) | n/s | n/s | n/s | n/s | n/s | n/s | n/s | Genopatch (local AI) |
| Works offline | V | yes | yes (text-to-wavetable online) | n/s | n/s | n/s | n/s | yes | yes (Genopatch local) |
| Formats | VST3, Standalone (Windows x64 verified) | VST3/AU/AAX | n/s | n/s | n/s | n/s | n/s | n/s | VST2.4/VST3/AU |
| Linux | builds (not a product target) | n/s | yes | n/s | n/s | n/s | n/s | n/s | n/s |
| CPU / audio quality data | measured (single machine) | n/s | GPU-drawn UI claim | n/s | n/s | n/s | n/s | n/s | n/s |

No matched-condition CPU, audio-quality or AI comparison has been performed; this matrix compares stated features only.
