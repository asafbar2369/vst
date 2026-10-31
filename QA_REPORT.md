# RMX16 Research Build — QA report

Date: 2026-10-30

## Verification status

This package is a **source project**, not a compiled VST3 binary. A full compile and host-validation pass has **not** been completed in this environment. JUCE is fetched by CMake from GitHub at configure time; the current build cache contains only an incomplete FetchContent setup and no completed JUCE source/build tree. No claim of successful compilation, plugin scanning, audio playback, or CPU profiling is made.

## Checks performed by source/configuration inspection

- Confirmed the project declares JUCE 9.0.4 and C++17.
- Confirmed the CMake project is VST3-only; Standalone is not built.
- Confirmed the supplied front-panel image is embedded as JUCE binary data and used as the editor background.
- Confirmed editor is resizable and has a fixed 1057:198 aspect ratio constraint.
- Confirmed AudioProcessorValueTreeState is used for host parameters and state serialization methods are present.
- Confirmed the audio processor accepts mono/stereo input layouts subject to its layout check and renders stereo output.
- Confirmed the source contains nine named program paths.
- Confirmed source includes parameter smoothing and a 120-second reported tail length.

These are static source inspections, not runtime tests.

## Not yet verified / required on Windows

1. Configure and compile with Visual Studio 2022, Windows SDK, and CMake 3.22+.
2. Confirm JUCE 9.0.4 downloads and the VST3 target builds without compiler warnings/errors.
3. Run pluginval (strictness level 5 or higher) against the resulting VST3.
4. Scan and load in FL Studio 25; test 44.1, 48, 88.2, 96, and 192 kHz sessions; mono and stereo tracks; block sizes 32–2048.
5. Test automation, host save/reopen, preset/state recall, bypass, transport stop, sample-rate changes, and repeated editor open/close.
6. Test GUI resize at minimum, default, and maximum dimensions; verify aspect ratio and hit-zone alignment on Windows DPI scaling (100%, 125%, 150%, 200%).
7. Run long-duration stability and CPU/memory tests.
8. Compare every algorithm against authenticated hardware or licensed reference captures/IRs. Until this is done, DSP is an independent approximation, not an authenticated 1:1 emulation.

## Known modeling limitations

- No recovered original EPROM firmware or original coefficient/tap tables are included.
- The reverb algorithms are independently implemented approximations; Plate A1 has a dedicated all-pass diffusion/tank path, while the room/hall programs use the FDN. They have not been measured against a physical RMX16 or a licensed reference implementation.
- The documented 40 kHz internal clock is not yet recreated by a dedicated internal resampling stage; processing currently follows the host sample rate.
- Converter and arithmetic limitations are approximated numerically, not modeled as a complete circuit/firmware simulation.
- Some program parameter limits are inferred where public documentation is incomplete; see `RESEARCH_NOTES.md` and README.
- The user-supplied faceplate image is used as a static skin with interactive overlays. The background artwork itself is not vector-redrawn.

## Reproducible Windows build

From a Developer PowerShell for VS 2022 in this directory:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target RMX16_Reconstruction_VST3
```

If successful, locate `RMX16 Research Build.vst3` under `build` and copy it to:

`C:\Program Files\Common Files\VST3\`

Then rescan plugins in FL Studio 25. Do not treat the plugin as production-ready until the Windows build, pluginval, and host tests above pass.

## Stage update — 2026-10-30 (robustness pass)

### Changes made
- Added finite-value sanitization at the host audio boundary. NaN/Inf samples are converted to silence before they enter the delay network, preventing persistent contamination of delay buffers.
- Bounded non-finite/out-of-range dB conversion inputs before gain calculation.
- Removed an unused per-sample `freeze` local; freeze behavior remains represented in the precomputed feedback coefficients.
- Kept the output conversion clamp at the modeled 16-bit output boundary rather than allowing invalid values to propagate.

### Checks actually run
- Re-inspected the source around `processBlock`, the FDN feedback path, program switching, and parameter cache updates.
- Ran CMake configure. It failed before compilation because the environment cannot resolve `github.com` while FetchContent tries to obtain JUCE 9.0.4. This is an environment/network dependency failure, not a compiler diagnostic.
- A build was therefore not run; no VST3 binary or pluginval result is available.

### Next engineering stage
- Obtain JUCE 9.0.4 source in the Windows build environment (or provide a local JUCE source path), compile with MSVC, and address real compiler diagnostics.
- Add automated DSP tests for impulse response, silence stability, NaN/Inf rejection, decay envelope, stereo decorrelation, and state round-trip.
- Implement a measured 40 kHz internal-rate stage only after selecting and validating a proper asynchronous/polyphase resampler; do not fake the clock by changing delay lengths alone.
- Replace the estimated FDN/tap tables with independently derived per-program networks as primary-source documentation and authenticated reference measurements permit.


## Stage update — 2026-10-30 (program-limit consistency)

### Issue found and corrected
- A source-code/UI mismatch allowed Hall B3 (program index 4) to reach 300 ms even though the Issue 3 user manual specifies a 200 ms maximum.
- Corrected the DSP clamp and GUI pot range so Ambience, Hall C1 and Hall B3 are all capped at 200 ms; Room A1 and Plate A1 remain 300 ms, Chorus 50 ms, Echo A 1200 ms, Reverse 1 100 ms, and Nonlin 2 remains a conservative 300 ms implementation assumption.
- Removed a duplicate unreachable Reverse 1 condition in the GUI range function and corrected the research table.

### Evidence
- The AMS/Neve-hosted RMX16 manual states Hall B3 pre-delay up to 200 ms: https://media.uaudio.com/support/manuals/dd/AMS%20RMX16%20Manual.pdf
- The Issue 3 manual transcription independently gives the same 200 ms value: https://www.barryrudolph.com/recall/manuals/ams_neve_rmx_16.pdf
- An older 1984 review reports 300 ms, which conflicts with the later Issue 3 manual; this build follows the later manual.

### Verification performed
- Source-level check confirms the DSP and GUI now use the same program-specific maximum for Hall B3.
- Full compilation and plugin runtime tests remain blocked until JUCE is available in the build environment.


## Stage update — 2026-10-30 (AMS 500 Series Issue 2 manual review)

### Source reviewed
- Official AMS/Neve PDF: https://www.ams-neve.com/wp-content/uploads/2021/08/rmx-16-500-series-user-manual-issue-2.pdf
- The PDF identifies itself as the RMX16 500 Series User Manual, Issue 22 in the extracted title page (the hosted filename says Issue 2). It is 23 pages.

### Findings incorporated into research notes
- Explicit separation of original RMX16 hardware specifications from the 500 Series 24-bit/48 kHz converters and 32-bit DSP. The manufacturer states the 500 Series uses the same algorithms, but this does not establish that the original hardware had the newer signal path.
- Confirmed original program order and nine additional 500 Series programs.
- Confirmed Hall B3 maximum pre-delay of 200 ms.
- Confirmed Echo A/B maximum delays of 1.2 s / 400 ms.
- Added detailed behavior notes for Chorus, Echo, Nonlin 2, Reverse 1 and Ambience freeze behavior, including the documented DC-offset/overflow warning.
- Separated 500 Series-only features (Mix control, 100 user presets, OLED display and modern converter performance) from the original hardware target.

### Verification status
- Research notes updated from direct PDF text extraction.
- Existing DSP source was inspected for the Echo A/B parameter limits and program indexing; no DSP behavior change was made in this documentation-only pass.
- This pass does not establish algorithm-level equivalence or successful plugin compilation. Windows/MSVC build, pluginval, FL Studio 25 runtime checks and hardware/reference audio comparison remain outstanding.

## Stage update — 2026-10-30 (parameter-transition and resize pass)

### Changes made
- Added a 20 ms one-pole smoothing ramp for the main pre-delay and Echo B delay controls. The target values are clamped per program before smoothing; the first process sample initializes directly to the requested value to avoid an unintended startup sweep. This reduces abrupt delay-time jumps when a host automates these controls or the user nudges them.
- Corrected GUI overlay scaling: the photographed faceplate was resized by the editor, but the painted LED bezel and rotary pointer overlays previously remained in the original pixel coordinate space. The overlay paint now uses the same proportional transform as the faceplate.
- Removed an unused keypad local variable.
- Added `tools/verify_source.py`, a repeatable source-level regression check for program labels, delay limits, delay smoothing, resize behavior and VST3 target configuration.

### Checks actually run
- `python tools/verify_source.py`: 10/10 source-level checks passed.
- Reviewed the delay smoothing initialization and confirmed Echo B reads the smoothed Echo B delay value.
- No C++ compilation, pluginval run, DAW load test, or audio measurement was performed. JUCE source is not present locally and the environment cannot resolve GitHub to fetch JUCE 9.0.4.

### Remaining blockers before a distributable VST3
- Obtain JUCE 9.0.4 (or validate and pin another supported JUCE release) and compile with a Windows x64 MSVC toolchain.
- Resolve all compiler/linker diagnostics and run pluginval.
- Load in FL Studio 25 and test editor resize, parameter automation, preset/state restore, mono/stereo routing, and long-tail stability.
- Validate DSP against an original RMX16 or a licensed/reference capture set. The current FDN and per-program coefficient/tap patterns remain independent engineering approximations, not a bit-accurate or verified algorithm port.
- Add and validate a proper 40 kHz internal resampling stage before claiming original clock behavior.

## Stage update — 2026-10-30 (persistent Store/Recall slot)

### Changes made
- Moved the single front-panel Store/Recall slot from the editor instance into the processor. Closing/reopening the plugin editor no longer loses the stored slot.
- Added the stored snapshot as a dedicated `RMX16_USER_PRESET` child in the plugin's serialized ValueTree state. On state restore it is separated from the APVTS parameter tree before the parameters are restored.
- Recall is disabled until a user snapshot exists. This is one front-panel user slot, not a claim of the 500 Series' 100-preset library.

### Checks actually run
- Expanded `tools/verify_source.py`: 13/13 source-level checks passed, including the Store/Recall state serialization path.
- This is a source inspection/regression check only. Persistence still requires compiled host save/close/reopen/load tests.

## Stage update — 2026-10-30 (Chorus voice-spacing correction)

- Re-read the Chorus control description in the 500 Series Issue 2 manual: the Pre-delay value is the maximum spacing between adjacent voices, with Nudge in 1 ms increments. For five voices there are four inter-voice gaps.
- Corrected the modeled voice offsets from `0, 0.25, 0.50, 0.75, 1.00 × 5 × Pre-delay` to `0, 0.25, 0.50, 0.75, 1.00 × 4 × Pre-delay`. At a 50 ms setting, the modeled voice delays are now 0/50/100/150/200 ms before the small randomized modulation component.
- Source-level regression checks now include this five-voice spacing relationship; 14/14 checks pass. This is a manual-guided reconstruction, not a measured match to the original firmware.


## Additional source audit — 2026-10-30

- Fixed a lifecycle bug: JUCE may call `prepareToPlay()` again after a host sample-rate change. The engine reset now invalidates all cached filter/feedback coefficients, forcing them to be recomputed using the new sample rate. Without this, the DSP could retain coefficients calculated for the previous rate.
- Added a source regression check for cache invalidation. This is a code inspection/source test only; it is not a substitute for compiling and exercising sample-rate changes in a DAW.


## Stage update — 2026-10-31 (first real compile and pluginval)

- Built with GCC/Ninja on Linux against JUCE 9.0.4 (tag verified to exist). Compiler errors found and fixed: missing `C` language in `project()`, nonexistent `ValueTree::setType` (Store/Recall), plus `-Wshadow`, `-Wfloat-equal` and hidden-overload warnings.
- `pluginval` initially segfaulted in the Editor test: `setSize()` in the constructor called `resized()` before the keypad buttons existed. Fixed; `pluginval` strictness 5 and 10 now pass.
- Fixed: transparent buttons no longer get a LookAndFeel_V4 outline; keypad entry can no longer produce values like `1.2.3`; resize limits now match README (846x158 to 1586x297); the display clamps pre-delay to the program maximum; early-reflection tap tables are returned by reference; GitHub workflow now copies the `.vst3` bundle folder before uploading.
- Not done: Windows/MSVC build, FL Studio tests, audio comparison. 20/20 source checks in `tools/verify_source.py`.


## Stage update — 2026-10-31 (constructor safety and CI coverage)

- Fixed a confirmed editor-construction crash: the initial `setSize()` could invoke `resized()` before keypad components were created. The first size change now occurs after all child controls have been added.
- Aligned the resize limits with the documented 846×158 to 1586×297 range.
- Changed invalid audio-buffer handling to clear output rather than returning potentially stale samples.
- Hardened state restoration: every required parameter must appear exactly once with a finite numeric value; additional future state nodes are tolerated.
- Restored pull-request CI, added source regression checks and Windows `pluginval` strictness-level-5 validation, and package the complete VST3 bundle as a ZIP artifact.
- These changes are pending the new GitHub Actions run. FL Studio 25 runtime and audio-reference tests remain outstanding.
