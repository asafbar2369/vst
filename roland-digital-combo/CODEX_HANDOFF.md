# Codex handoff: Digital Combo VST3 (14 Nov 2026)

## Scope

The repository also contains an unrelated RMX16 project. The synth plugin lives
in `roland-digital-combo/`. Work from the supplied source archive or a branch
of the user's authorized GitHub repository, never from the user's personal PC.

## Verified changes in this checkpoint

- `JD800Engine.*`: revised 24-Tone resource management.
- `D50Engine.*`: revised 16/8 voice allocation and Separate MIDI channels.
- `PluginProcessor.*` + `MidiNoteOwnership.h`: note ownership across transposition
  changes, collisions, and MIDI channels.
- `PluginEditor.cpp`: explicitly identifies built-in presets as synthetic.
- `tools/verify_pcm_roms.py`: local chip-dump metadata recognition only.

Linux commands that passed:

```
cd roland-digital-combo

g++ -std=c++17 -Wall -Wextra -Werror -pedantic -ISource \
  Tests/MidiNoteOwnershipTest.cpp -o /tmp/midi-test
/tmp/midi-test

g++ -std=c++17 -O2 -ISource \
  Tests/EngineResourceTest.cpp \
  Source/JD800Engine.cpp Source/JdWaves.cpp Source/JdEffects.cpp \
  Source/D50Engine.cpp Source/D50SysEx.cpp Source/JD800SysEx.cpp \
  -o /tmp/engine-resource-test
/tmp/engine-resource-test

PYTHONPATH=tools python -m unittest discover -s tools -p 'test_*.py' -v
```

## Not done / never claim complete without evidence

1. Build and debug full Windows x64 VST3 using the pinned JUCE version and run
   Steinberg validator, pluginval, native GUI render checks and host tests.
2. Validate the D-50 Separate routing and channel pressure/CC behavior against
   original documented MIDI data, including edge cases and simultaneous channels.
3. Decode *legally obtained* D-50 and JD-800 original PCM dumps, including
   multisample zone/loop maps; add fixtures that do not redistribute ROM data.
   The metadata checker is **NOT** a ROM decoder.
4. Verify timing and mapping of all envelopes, original filter types, effects,
   voice stealing and sample interpolation against hardware recordings.
5. Finish truly hardware-faithful editor layouts and automate meaningful UI
   tests (all controls linked to audio and saved parameters).
6. Add authorized original or user-created SYX preset bank import/export tests,
   preserve messages/slots, and verify patch roundtrip and DAW state recovery.
7. Test Windows 10/11 and FL Studio 25 on a genuinely accessible test system,
   **not the user's personal computer**.

Don't download or publish proprietary Roland ROM, firmware or factory banks
without evidence of permission. No ROM bytes are in this source checkpoint.

This is a staged **source checkpoint**, not a verified production VST3.

## Continuation (14 November 2026, later checkpoint)

Read `QA_CONTINUATION_2026-11-14.md` before additional edits. SysEx patch-memory
slots are now selectable from the GUI with a partial/full coverage indicator,
and malformed-file recovery was fixed. The modified JUCE code is **not verified
compiled**; prioritize building the GUI with JUCE 9.0.5 and running
`DigitalComboTests`, `DigitalComboHostTest`, VST3 Validator, pluginval, and then
Windows / FL Studio 25 tests. The portable CMake mode can be used without JUCE:
`-DDIGITALCOMBO_PORTABLE_TESTS_ONLY=ON`.

The user supplied no authorised ROM dumps or original factory SysEx bytes;
never claim they were imported, and don't commit such data to public repositories.
