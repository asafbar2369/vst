# Digital Combo (unofficial D-50 / JD-800 style VST3)

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
  replaced by algorithmic substitutes chosen from the manual's waveform list descriptions.
* **Play modes** – JD-800, D-50, LAYER, SPLIT, plus per-engine level, pan, transpose and key zones.
* **Editor** – a D-50 front panel (2-line LCD, block and parameter buttons, data entry slider,
  INC/DEC, joystick in BALANCE or VALUE mode), a JD-800 front panel (54 sliders in the original
  slider sections, LAYER and ACTIVE tone buttons), detailed pages, full byte editors, a COMBO page
  (mixer, zone map, patch files, your own wave folders). All drawing is original.
* **Patch files** – `.dcpatch` (both synths + mode + mixer) in `Documents\Digital Combo\Patches`,
  listed in the header's patch browser together with the factory presets.
* **Your own waves (optional)** – choose a folder of WAV/AIFF files named by wave number
  (`001 ….wav` = D-50 PCM 1, `012.wav` = JD-800 waveform 12). Root key and sustain loop are read
  from the file's sampler data. Nothing is bundled or copied; only the folder path is saved.

No Roland ROM, PCM samples, firmware, factory patches or artwork are included. PCM partials and
JD-800 waveforms are generated algorithmically and do **not** sound like the original ROM.
Not affiliated with Roland.

Build (Windows, Visual Studio 2022, CMake ≥ 3.22):

```
cmake -S roland-digital-combo -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target DigitalCombo_VST3 DigitalComboTests DigitalComboHostTest
build\DigitalComboTests_artefacts\Release\DigitalComboTests.exe
build\DigitalComboHostTest_artefacts\Release\DigitalComboHostTest.exe "build\DigitalCombo_artefacts\Release\VST3\Digital Combo.vst3" out
"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /DBundleDir="<full path to Digital Combo.vst3>" roland-digital-combo\installer\DigitalCombo.iss
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
