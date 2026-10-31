# RMX16 Research Build — JUCE VST3 source project

This is an independent, research-based software reconstruction inspired by the original AMS RMX16 digital reverberation system. It is **not** an official AMS/Neve product and is not a bit-for-bit copy of the proprietary firmware.

The project uses the user-supplied front-panel photograph as a visual reference/skin, with live parameter text and mouse-interactive controls overlaid. The nine original programs are represented by separate DSP paths:

1. Ambience
2. Room A1
3. Hall C1
4. Plate A1
5. Hall B3
6. Chorus
7. Echo
8. Nonlin 2
9. Reverse 1

## Current implementation status

- JUCE VST3-only project scaffold.
- Windows x64 CMake build configuration, pinned to JUCE 9.0.4 (the latest stable release listed by the official JUCE repository at the time this package was prepared).
- Mono or stereo input accepted; stereo output produced. The reverb engine sums input to mono, following the original hardware's mono-in/stereo-out topology.
- Custom faceplate using the supplied image itself as the GUI skin, with live display text and mouse hit-zones/knobs layered over the photographed controls.
- Resizable GUI with a locked 1057:198 aspect ratio. Drag the lower-right corner in FL Studio; supported size range is approximately 846×158 to 1586×297 logical pixels.
- Keypad program selection, parameter-selection keys, pot control, nudge, temporary Store/Recall, and input/output level controls.
- Independent approximate DSP implementations for all nine named programs; Plate A1 now has a dedicated four-stage all-pass diffuser and cross-coupled damped tank, separate from the general FDN.
- Approximate converter coloration: gain-ranging quantizer inspired by the documented 12-bit ADC + 2-bit range data, 16-bit signal word, 28-bit arithmetic rounding, and 18 kHz low-pass.
- Program-dependent availability of the high/low decay-filter controls; decay Nudge is 0.1 s and filter Nudge is one setting step.
- Reverse 1 trigger detector uses an envelope follower to avoid retriggering on every raw waveform zero crossing.
- Predelay now delays the wet excitation entering the reverb network as well as the early-reflection pattern; Chorus and Echo retain their program-specific delay behavior.
- Hardware parameter limits corrected: Hall B3 200 ms (Issue 3 manual); Echo A/B long-range maxima 1.2 s / 400 ms; high-frequency decay setting -9 to 0.

**This is an early independent DSP research build, not a finished authentic emulation.** The actual AMS algorithm coefficients, internal instruction flow, complete timing/tap tables and original EPROM code are not public in the user manual. Most room/hall algorithms use a newly written 8-line feedback-delay network with estimated early-reflection patterns. Plate A1 uses a separate all-pass diffuser and cross-coupled damped tank. Neither is the original's proprietary implementation. Reverse 1 is currently a rising-envelope approximation rather than a true time-reversed convolution/buffer algorithm. The project has not been sonically validated against an original RMX16 or the licensed UAD version.

The official Issue 1 manual does not state a numeric maximum predelay for every program. This build uses the published limits where specified (Ambience 200 ms, Room A1 300 ms, Plate A1 300 ms, Hall B3 200 ms, Chorus 50 ms, Echo long range A/B 1200/400 ms). Hall C1 200 ms is corroborated in later RMX16 500-series documentation. Nonlin 2's 300 ms and Reverse 1's 100 ms are conservative implementation assumptions and must not be treated as verified factory limits. Echo's alternative short range (650/150 ms) is not yet exposed as a switch.

## Requirements (Windows)

- Windows 10/11 x64.
- Visual Studio 2022 with **Desktop development with C++** workload and Windows SDK.
- CMake 3.22 or newer.
- Internet access on first configure/build so CMake can fetch JUCE 9.0.4 from the official JUCE GitHub repository.
- FL Studio 25 or another VST3 host for use/testing.

## Build

Open PowerShell in this folder and run:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target RMX16_Reconstruction_VST3
```

The project sets `COPY_PLUGIN_AFTER_BUILD FALSE`; after a successful build, find `RMX16 Research Build.vst3` under `build` and copy it manually to:

```text
C:\Program Files\Common Files\VST3\
```

Then open FL Studio 25, run a plugin database rescan, and load **RMX16 Research Build** as an effect.

## How to use the faceplate

- The photographed left knob is Input Level; the lower-right knob is Output Level.
- The small upper knob is the data pot.
- Press PREDELAY, DECAY TIME, LOW FILTER or HIGH FILTER to choose what the data pot changes.
- The 1–9 keypad selects one of the nine factory programs.
- PROGRAM cycles to the next program.
- NUDGE DOWN/UP changes the currently selected value by the hardware-style step.
- STORE/RECALL is a single user slot owned by the processor; it survives closing the editor and is saved with the plug-in state.
- Wet/Dry is exposed as a host parameter and defaults to 100% wet, matching send/return use. The original faceplate has no dedicated wet/dry control.

## Important modeling limitations

1. The engine currently processes at the host sample rate. It does not yet resample internally to the original's 40 kHz clock. This is one of the next steps for more faithful aliasing/time-quantization behavior.
2. The gain-ranging ADC, 16-bit word path and 28-bit arithmetic are approximated numerically; the real converter/processor circuitry is not recreated circuit-by-circuit.
3. The Room/Hall/Plate/Nonlin/Reverse response patterns are informed by the manual, but their actual proprietary coefficients are unavailable.
4. Echo A and B have separate pre-delay, feedback/decay and filter parameters exposed through the A/B keypad banks. The two delay paths are independent estimates, not recovered original EPROM coefficients; the exact original range-switching behavior is not yet reproduced.
5. The photo-skin is a static user-provided reference image. Some printed control legends and exact physical details are inherited from that source image; the interactive controls are overlays.

## Files

- `Source/PluginProcessor.*` — parameters, host integration and DSP.
- `Source/PluginEditor.*` — image-based rack faceplate and mouse controls.
- `Resources/user_reference_faceplate.png` — crop of the image supplied in this conversation.
- `RESEARCH_NOTES.md` — hardware facts, manual-derived program behavior, source list and reconstruction decisions.
- `QA_REPORT.md` — current test/verification matrix, known limitations and build status.
- `build-windows.ps1` — optional build helper.

## Latest development update (2026-10-30)

A robustness pass added host-boundary NaN/Inf rejection and bounded dB-to-gain conversion so invalid upstream samples cannot permanently contaminate the reverb delay buffers. The source was re-inspected. CMake configuration was attempted, but JUCE 9.0.4 could not be fetched because this environment cannot resolve `github.com`; compilation and runtime QA remain blocked. See `QA_REPORT.md` for the exact status and next steps.

## Latest source pass (2026-10-30)

- Main and Echo B delay-time changes now use a 20 ms smoothing ramp to reduce discontinuities during automation and manual adjustment.
- GUI LED/needle overlays now scale with the photographed faceplate when the editor is resized.
- Run `python tools/verify_source.py` for fast source-level regression checks. These checks do not replace compilation, pluginval, DAW tests or audio comparison.

**Build status:** source project only. A VST3 binary has not yet been built or verified. JUCE 9.0.4 must be available to CMake, and the final Windows x64 build must be tested in FL Studio 25.
- The front-panel Store/Recall buttons now use a processor-owned snapshot, serialized with the plug-in state so the single stored slot survives editor closure and DAW project save/restore. This still requires runtime verification in a host.
- Chorus voice spacing now models five voices with four adjacent gaps; at the maximum 50 ms spacing control, nominal offsets are 0/50/100/150/200 ms before the small random modulation component.

### Windows build helper

The PowerShell build helper accepts an optional local JUCE source directory, avoiding a network fetch when JUCE has already been downloaded:

```powershell
.\build-windows.ps1 -JucePath "C:\SDKs\JUCE"
```

Without `-JucePath`, CMake FetchContent downloads the pinned JUCE 9.0.4 tag. The helper checks that a `.vst3` bundle was produced, but that alone is not a substitute for pluginval or FL Studio testing.


### Additional robustness fix (2026-10-30)

The DSP now invalidates sample-rate-dependent coefficient caches during reset. This ensures a subsequent `prepareToPlay()` at a different host sample rate recalculates the filter and feedback coefficients rather than retaining values from the previous rate. This fix has passed the source-level regression check; runtime verification remains pending.


## Build verification (Linux, GCC, JUCE 9.0.4)

The project configures and builds the VST3 on Linux, and `pluginval` passes at strictness levels 5 and 10 (run under Xvfb, in-process). Fixes found by that run: `LANGUAGES C CXX` (JUCE 9 requires C), removal of nonexistent `ValueTree::setType`, and an editor crash caused by `setSize()` running before child components existed. Windows/MSVC, FL Studio and audio-quality testing are still outstanding.

## Folder layout

```
CMakeLists.txt, README.md, build-windows.ps1, .gitignore
Source/      PluginProcessor.*, PluginEditor.*
Resources/   user_reference_faceplate.png
tools/       verify_source.py
.github/workflows/build-vst3.yml
```
