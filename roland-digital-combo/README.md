# Roland Digital Combo (unofficial D-50 / JD-800 style VST3)

Independent JUCE VST3 instrument for Windows x64 with two original synthesis engines:

* **D-50 engine** – LA-style structure (2 tones x 2 partials, 7 structures incl. ring modulator,
  TVF/TVA 5-time envelopes, P-ENV, 3 LFOs, EQ, chorus, reverb). Its parameters live in a
  448-byte temporary area laid out exactly as in Roland's *D-50 MIDI Implementation v2.00*, so
  D-50 DT1/RQ1 System Exclusive messages and `.syx` files address the same bytes.
* **JD-800 engine** – driven by a 384-byte patch laid out exactly as the JD-800 Patch Temporary Area
  (*Owner's Manual II*, MIDI Implementation, model 3DH): 4 tones with 2 LFOs, pitch/TVF/TVA envelopes,
  bias, velocity curves, key ranges, solo/legato, portamento, EQ, and the Group A (distortion, phaser,
  spectrum, enhancer) / Group B (chorus, delay, reverb) effect chain with all documented orders.
  JD-800 DT1/RQ1 SysEx and `.syx` files address the same bytes. The 108 internal waveforms are
  algorithmic substitutes chosen from the manual's waveform list descriptions, **unless you supply your
  own dumps of the instrument's three wave-ROM chips** (see "Your own JD-800 wave ROM" below).
* **Play modes** – JD-800, D-50, LAYER, SPLIT, plus per-engine level, pan, transpose and key zones.
* **Editor** – a redesigned, resizable interface (75-200 %, free drag with fixed aspect ratio, size saved
  with the project): header with JD-800 / D-50 / LAYER / SPLIT buttons, patch browser, INIT, undo / redo,
  MIDI activity and output meter; pages SOUND, ENVELOPES, MODULATION, EFFECTS and PERFORMANCE for each
  engine, built from the documented parameter tables (every control writes exactly its documented byte);
  MIXER & FILES, SETTINGS and ADVANCED (the D-50 and JD-800 front panels, detailed pages and full byte
  editors). Pitch-bend and mod wheels on screen follow incoming MIDI. All drawing is original.
* **Mod wheel and pitch bend** – CC1 and 14-bit pitch bend work in all four modes and with polyphony
  (D-50 bend 0-24 semitones, JD-800 up 0-12 / down 0-48, per patch); both are host parameters too.
* **Patch files** – `.dcpatch` (both synths + mode + mixer) in `Documents\Digital Combo\Patches`,
  listed in the header's patch browser together with the factory presets.
* **Your own waves (optional)** – choose a folder of WAV/AIFF files named by wave number
  (`001 ….wav` = D-50 PCM 1, `012.wav` = JD-800 waveform 12). Root key and sustain loop are read
  from the file's sampler data. Nothing is bundled or copied; only the folder path is saved.

* **Your own JD-800 wave ROM (optional)** – MIXER & FILES page -> "JD-800 wave ROM": choose the folder
  or `.zip` that holds your own dumps of the three 1 MiB wave-ROM chips. The plug-in recognises them by
  CRC-32 (or by structure), descrambles and decodes them in memory and plays the real multisamples with
  their key ranges and loops. Only the location is saved with the project; nothing is copied, stored in
  the plug-in or sent anywhere. Details, measurements and open points: `docs/ROM-IMPORT.md`.
* **SysEx files** – DT1 and bulk-dump DAT packets in `.syx` files are applied (checksums verified, damaged
  or foreign messages skipped and counted, partial patches reported and not offered).

No Roland ROM, PCM samples, firmware, factory patches or artwork are included. Without your own ROM
dumps, PCM partials and JD-800 waveforms are generated algorithmically and do **not** sound like the
original ROM. The D-50's PCM ROM cannot be read (not implemented). Not affiliated with Roland.

Build (Windows, Visual Studio 2022, CMake ≥ 3.22):

```
cmake -S roland-digital-combo -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target DigitalCombo_VST3 DigitalComboTests DigitalComboHostTest
build\DigitalComboTests_artefacts\Release\DigitalComboTests.exe
build\DigitalComboHostTest_artefacts\Release\DigitalComboHostTest.exe "build\DigitalCombo_artefacts\Release\VST3\Roland Digital Combo.vst3" out
"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /DBundleDir="<full path to Roland Digital Combo.vst3>" roland-digital-combo\installer\DigitalCombo.iss
```

The C/C++ runtime is linked statically (`/MT`), so the plug-in does not need the Visual C++
Redistributable. The editor uses JUCE's software renderer by default (COMBO page > Editor drawing
can switch to Direct2D).

CI: `.github/workflows/digital-combo-windows.yml` (windows-2022) builds, checks the DLL imports,
runs the unit tests and the host test (which also captures the editor from the real screen with
both renderers), the Steinberg VST3 validator and pluginval (strictness 10), builds the installer
and tests it (silent install, SHA256 of the installed binary, validator on the installed copy,
silent uninstall), and uploads the VST3 zip and the installer.
What is verified and what is not: see `docs/VERIFICATION.md`.

### Source continuation (2026-11-14)

See [`QA_CONTINUATION_2026-11-14.md`](QA_CONTINUATION_2026-11-14.md).
The GUI's patch browser now lists imported SysEx patch-memory slots for both
engines and identifies partial data; the WAV/AIFF sample reader has additional
loop/safety fixes. The separate `tools/inspect_syx.py` audits user `.syx` files
without importing audio ROMs. `tools/verify_pcm_roms.py` verifies ROM identities
only; the JD-800 wave ROM is decoded by the plug-in itself (`Source/JdRomImport.*`, `docs/ROM-IMPORT.md`),
the D-50 PCM ROM is not decoded at all.

For Linux/CI without a JUCE checkout, run the five C++ portable tests (including the JD-800 ROM importer on
a synthetic ROM; set `DIGITALCOMBO_JD800_ROM_DIR` to a folder with your own dumps to run it on them too) using:

```
cmake -S . -B build-portable -G Ninja -DDIGITALCOMBO_PORTABLE_TESTS_ONLY=ON
cmake --build build-portable
ctest --test-dir build-portable --output-on-failure
```

These are **not** VST3 / host compatibility tests. A JUCE build, GUI render test,
pluginval/validator and actual Windows FL Studio 25 test remain necessary.
