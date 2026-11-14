# Digital Combo source checkpoint — 14 November 2026

This source-only checkpoint updates the uploaded `vst-workspace-2026-11-14.zip`.
No Roland PCM/ROM/firmware, factory SysEx patches, or other proprietary samples are included.

## Changes compared with the uploaded source

- JD-800: voice allocation tracks a 24-Tone resource budget rather than a fixed
  16-note capacity; released tones still consume resources until reused.
- D-50: WHOLE/DUAL voice budget and tone-specific capacity handling improved;
  Separate MIDI-channel routing added at engine level.
- The plugin processor now tracks the originating MIDI channel and source key,
  keeps transposition note-on/note-off pairs consistent, and does not kill a pitch
  while another source note still owns it. Invalid transpositions outside the
  MIDI 0-127 range are skipped instead of clamped and colliding.
- Added `MidiNoteOwnership.h` to centralize cross-channel held-note ownership.
- GUI patch browser no longer falsely labels the built-in synthetic patches as
  original factory ROM sounds.
- Added a standalone user-ROM identity checker for known D-50 and JD-800 PCM chip
  dumps, with CRC32/SHA-1 and ZIP-folder scanning. **The checker cannot decode ROM,
  reconstruct multisample mappings, or make the plugin play hardware PCM.**
- Added standalone C++ and Python regression tests and wired C++ tests into CMake.

## Executed tests in isolated Linux runtime

- `g++ -std=c++17 -Wall -Wextra -Werror -pedantic -ISource Tests/MidiNoteOwnershipTest.cpp ...`
  PASS.
- `g++ -std=c++17 -O2 -ISource Tests/EngineResourceTest.cpp Source/JD800Engine.cpp Source/JdWaves.cpp Source/JdEffects.cpp Source/D50Engine.cpp Source/D50SysEx.cpp Source/JD800SysEx.cpp ...`
  PASS (JD 1/4-Tone limits, D-50 WHOLE 16 / DUAL 8).
- `PYTHONPATH=tools python -m unittest discover -s tools -p 'test_*.py' -v`
  3 of 3 PASS (ROM recognition checks and missing file reporting).

## Known unverified/missing

- No Windows VST3 built in this session; no FL Studio 25 integration test.
- JUCE-based GUI compile/render/host regression tests NOT re-run in this session.
- True hardware ROM decoding, original PCM multisample mapping, original factory
  presets and exact D-50/JD-800 DSP calibration remain **incomplete**.
- No redistribution rights were established for proprietary Roland ROM or patch files.

This archive is a **research source checkpoint, not a finished VST3 release**.
