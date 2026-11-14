# Digital Combo continuation checkpoint — 2026-11-14

**Status: SOURCE CHECKPOINT, NOT A FINISHED VST3 RELEASE.**
This continues the supplied `DigitalCombo_Source_Checkpoint_2026-11-14.zip` without
removing prior source or test artifacts.

## Changes in this continuation

1. **SysEx bank browser** (`PluginEditor`, `PluginProcessor`, `ParamArea`):
   - The patch browser now enumerates received D-50 and JD-800 SysEx patch-memory slots.
   - Selecting a slot copies its bytes into that engine's temporary edit area; the other
     engine and COMBO mixer are left alone.
   - Slot names are decoded using the instrument-specific format.
   - A slot is flagged `[partial SysEx]` until every byte of its D-50 448-byte or
     JD-800 384-byte patch-memory area has been received. Partial slots remain selectable
     by design, but are not represented as complete or factory-authentic.
   - DAW state restore clears old bank slots before applying the restored bank to avoid
     leaking patches from a prior project. New partial writes start from init bytes.
   - `ParamArea::isAddressRange` rejects wraparound, and invalid slot numbers are guarded.
2. **SysEx file recovery**: malformed/truncated F0 frames no longer silently consume the
   start byte of the following valid F0 frame. Null/oversized file import is rejected.
3. **PCM WAV/AIFF playback reliability** (`DspCommon.h`, `UserWaveLoader`):
   - Correct fractional interpolation across sustain-loop boundaries, clamp bad positions,
     and replace potentially unbounded repeated subtraction with constant-time modulo.
   - Cap decoded per-file size to 32 MiB and per-instrument set to 256 MiB, with explicit
     skipped-file messages, preventing unexpectedly huge allocations.
4. **Independent SysEx inventory** (`tools/inspect_syx.py`): reads user-supplied `.syx`
   files, validates Roland DT1 checksums, reports complete/partial patch coverage and
   available names, and does not include or retrieve any copyrighted presets.
5. **Buildable headless tests**: CMake option `DIGITALCOMBO_PORTABLE_TESTS_ONLY=ON` builds
   four portable executables without fetching JUCE. `ParamArea` coverage is tested against
   the actual header using a deliberately minimal, test-only JUCE interface shim (not a
   JUCE plug-in compilation). Additional JUCE-host regression tests were added to
   `Tests/TestMain.cpp`, **but were not executed here**.

## Tests actually run (isolated Linux environment, GCC 14.2)

- `cmake -S ... -B ... -G Ninja -DDIGITALCOMBO_PORTABLE_TESTS_ONLY=ON`: configured.
- `cmake --build ...`: compiled four portable test binaries.
- `ctest --test-dir ... --output-on-failure`: **4/4 passed**.
- Three standalone regression binaries plus the engine resource test were recompiled with
  `-fsanitize=address,undefined`, then executed: **4/4 passed**.
- `PYTHONPATH=tools python -m unittest discover -s tools -p 'test_*.py' -v`:
  **7/7 passed** (four SysEx inventory tests, three user-ROM identification tests).
- Logs: `docs/verification-logs/continuation-2026-11-14/`.

## Explicitly NOT verified / incomplete

- **The modified JUCE GUI/processor was NOT compiled**: network DNS blocked fetching
  JUCE 9.0.5, and this runtime has no preinstalled JUCE checkout.
- No new Windows VST3, standalone executable, installer, or FL Studio 25 host test.
- No real user's factory SysEx bank was uploaded for validation; SysEx inventory tests
  use synthetic, license-free test messages.
- Original Roland ROM PCM decoding and accurate multisample-zone mapping remain absent;
  the ROM verifier only identifies known chip dumps. No proprietary ROM/SysEx files are
  bundled or downloaded.
- Original-hardware DSP measurements/1:1 reference calibration remain incomplete.

## Reproduce portable tests

```
cmake -S roland-digital-combo -B build-portable -G Ninja -DDIGITALCOMBO_PORTABLE_TESTS_ONLY=ON
cmake --build build-portable --parallel
ctest --test-dir build-portable --output-on-failure
cd roland-digital-combo
PYTHONPATH=tools python -m unittest discover -s tools -p 'test_*.py' -v
```

## Inspect a user-provided SysEx file

```
python roland-digital-combo/tools/inspect_syx.py /path/to/your-bank.syx --json
```

This tool reports bank coverage, **not** waveform/ROM compatibility or original
Roland factory-patch authenticity. It deliberately rejects handshake DAT as an
unsupported command instead of falsely treating it as ordinary DT1.
