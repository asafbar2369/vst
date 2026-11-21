# Known Limitations

Current as of 2026-11-20 (branch `claude/bold-cori-kvu779`). Ordered roughly by impact.

## Not verified
- **FL Studio 25**: never run. Generic VST3 conformance passed (pluginval strictness 10, Steinberg validator), but FL-specific
  behaviour (wrapper, project recall inside FL, MIDI routing, scaling) is UNVERIFIED.
- **Live AI providers**: mock protocol coverage exists. Claude's 19b250e records one OpenAI invalid-key (HTTP 401) error-path test; successful inference is still unverified. Model availability depends on the user's account.
- No listening tests, no matched-condition comparison with any competitor, no measurement of real hardware.
- No memory / thread sanitiser runs yet; real-time safety is by design and code review.

## Sound engine
- **CPU**: heavy unison patches are expensive (a 6-note chord with 54 unison oscillators, 2x oversampling runs at ~5x realtime
  on a 2.1 GHz cloud core; ~1.2x at 192 kHz). No SIMD unison processing, no multi-core rendering, no AVX2 paths yet.
- **Polyphonic headroom**: presets are calibrated per note (peaks <= -4 dBFS), but a 6-note pad chord peaks near 0 dBFS. There
  is no output safety limiter; in the standalone app loud chords can clip at the audio interface.
- **Diode Ladder** (and Acid Diode): the cutoff value is calibrated to small-signal brightness, so at maximum resonance the peak
  sits about 3x above the cutoff value (measured 2998 Hz at a 1 kHz setting). Acid Diode self-oscillates only with Character
  raised; SEM does not self-oscillate (by design).
- MS Sallen-Key high-pass measured at about 6 dB/octave (18 dB three octaves below cutoff).
- Eco (1x) quality aliases audibly on very high notes (-27 dB inharmonic energy on a C7 saw).
- Oscillator modes: wavetable, virtual analog, sample, multisample (SFZ), granular, spectral, six-operator FM (32 algorithms,
  .syx cartridges; curves are approximations, not hardware-identical - docs/FM_ENGINE.md). **Not implemented**: dedicated
  additive engine, phase distortion engine, physical / modal modelling, vector synthesis, wave
  sequencing, sample-attack + synthesised-sustain layering, four-layer architecture, keyboard splits and velocity layers,
  MPE, MIDI Learn, microtuning (Scala).
- Reverb: eight programs (hall, plate, room, chamber, ambient, nonlinear, gated, reverse; docs/REVERB_REPORT.md). Plate /
  room / chamber late tails still show more spectral persistence than the old (heavily chorused) reverb; no listening test yet.
- Effects: 18 effects as listed in the brief. **Not implemented**: shimmer reverb; BBD
  chorus / delay models; rotary speaker; resonator; transient shaper; dedicated bitcrusher module; dynamic EQ; spectral effects.
- Oversampling options are 1x / 2x / 4x (no 8x).

## Library and content
- 97 factory presets in 18 categories (incl. 12 FM presets on original FM voices) (target 2,000 in 24 categories), 27 factory wavetables (target 500), 9 synthesised
  multisample instruments and 10 synthesised single samples - all original, none recorded from real instruments.
- 14 near-duplicate candidate pairs are flagged for listening review (FACTORY_LIBRARY_QA.md).
- Per-preset velocity / mod-wheel / macro usefulness has not been reviewed.

## AI
- Audio-to-preset similarity is an internal feature metric; evaluation mean 0.548 on synthesizable references. FM, glassy and
  bell timbres match poorly (0.07-0.28). No validation against human perception.
- The optimiser does not search FM routings or effects. Matching takes ~4-7 s for 40 candidates.
- Local agent loop (render -> analyse -> refine) is tested against a mock Ollama server only; no real local model was run here
  (docs/LOCAL_AI_AGENT.md). Result quality depends on the model the user installs.
- The offline designer understands a fixed English vocabulary. Per the owner's 2026-11-21 scope change, Hebrew AI is disabled; prompts and replies are English-only.
- Ollama requests set a 32k context; models with smaller context limits may truncate the parameter reference. LM Studio's
  context length is set in LM Studio itself.
- Not implemented from the AI brief: AI wavetable generation beyond harmonic frames, AI arpeggio / MIDI sequence generation as
  separate tools (only through patch operations), semantic preset search, AI-assisted sample selection UI, preset morphing UI.

## Interface
- English-only UI and AI. Hebrew support was explicitly removed by the owner on 2026-11-21.
- Not implemented: 3D wavetable view, FFT harmonic / spectral editors beyond the current wavetable editor, multisample mapping
  editor, granular visualisation, piano-roll sequencer, MIDI import / export, spectrum analyser view.
- Screenshots were taken with Linux fonts (DejaVu / Liberation); Windows renders with its system fonts.

## Platform
- Windows x64 builds and validation run on GitHub-hosted runners only; the binaries are unsigned (no code-signing certificate).
- macOS / AU and AAX are not built.
- JUCE is used under its AGPLv3 / commercial licence terms; a commercial release requires a JUCE commercial licence and a
  review of the VST3 SDK licence (Steinberg's dual licence).
