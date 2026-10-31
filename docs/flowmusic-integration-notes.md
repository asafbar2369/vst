# FlowMusic RMX16 prototype — integration notes

This folder records the supplied FlowMusic parameter/UI/DSP prototype as a design reference for the JUCE VST3 project. It is not compiled into the plug-in.

## Decision: preserve the existing JUCE engine

The current project already has nine separate program paths, a hardware-style faceplate, host automation/state serialization, and additional Echo A/B controls. The supplied code is a different runtime/API (FlowMusic DSP runtime + React hooks), so it cannot be pasted into the JUCE target. Its parameter model also conflicts with the current hardware-oriented controls. This reference is therefore kept separate until each behavior is ported and tested in C++.

## Parameter mapping and conflicts

| Supplied control | Existing JUCE parameter | Integration note |
|---|---|---|
| Program | `program` | Same nine programs, but keep the current canonical order: Ambience, Room A1, Hall C1, Plate A1, Hall B3, Chorus, Echo, Nonlin 2, Reverse 1. |
| Decay (0.1–9.9 s, default 2.2) | `decay` | Existing range is 0.0–9.9 s and default 2.4 s. Decide whether to retain hardware-style zero or adopt the supplied 0.1/2.2 values before changing automation compatibility. |
| PreDelay (0–200 ms) | `predelay` | A fixed 200 ms maximum would remove existing program-specific ranges, including Room/Plate 300 ms and Echo A 1200 ms. Keep the existing per-program limits; expose 200 ms as a generic UI profile only if explicitly desired. |
| LowFilter (-12 to +6 dB) | `low` | Not equivalent: the existing parameter is a hardware-style low-frequency decay-time setting (-9…+9), not an input/output low-shelf gain. Do not silently change its meaning or existing automation ID. |
| HighFilter (-12 to +6 dB) | `high` | Not equivalent: existing control is a high-frequency decay setting (-9…0), not a high-shelf gain. A true tone shelf would need a separate parameter and DSP stage. |
| InputLevel (-24 to +6 dB) | `input` | Existing range extends to +12 dB. A change to +6 dB is a product/UI decision, not a direct port. |
| OutputLevel (-24 to +6 dB) | `output` | Existing range extends to +12 dB. |
| Mix (0–100%) | `mix` | Same concept; existing host parameter already defaults to 100% wet. |

## Source review: fixes required before any DSP port

The supplied AssemblyScript-like processor text is malformed and is not a compilable source file. Notable issues:

- `inPeakL`, `inPeakR`, `apIdx2`, and `apIdx3` are used without valid declarations/definitions.
- Several declarations are syntactically broken (including `let in= 0.0;0.0;`, the Chorus right-delay declaration, and a stray `ap= 0;`).
- The code's program-index routing swaps Echo and Chorus: index 5 is Chorus and index 6 is Echo in the supplied program list and in the existing JUCE project, but the prototype labels index 5 as Echo and index 6 as Chorus.
- The prototype's single mono pre-delay and shared delay buffers do not preserve the current project's independently modeled Echo A/B paths and program-dependent timing.
- The FlowMusic `useParam`, `useProcessorBuffer`, and `@flowmusic/*` interfaces are not JUCE APIs. The React editor must be reimplemented with JUCE components/attachments rather than included verbatim.
- The prototype's 250/280 Hz and 3.8/4 kHz shelves are new tone controls. They must not be wired to the existing decay-time Low/High controls.

## Safe implementation sequence

1. Keep the exact supplied eight-control schema in `flowmusic-parameters.json` as the reference contract.
2. Preserve current JUCE parameter IDs and existing presets/automation.
3. If true LowFilter/HighFilter tone shelves are wanted, add new, separately identified APVTS parameters and implement smoothed stereo shelf filters in the JUCE audio path.
4. Port only DSP algorithms that are compatible with the project's nine-program index order; add regression tests for every program route.
5. Rebuild and run source checks, pluginval, then FL Studio 25 load/audio tests. Do not claim a verified VST3 until those checks pass.

This note intentionally does not claim that the FlowMusic prototype has been integrated into the audio engine.