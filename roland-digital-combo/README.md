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
* **Play modes** – JD-800, D-50, LAYER, SPLIT.

No Roland ROM, PCM samples, firmware, factory patches or artwork are included. PCM partials and
JD-800 waveforms are generated algorithmically and do **not** sound like the original ROM.
Not affiliated with Roland.

Build (Windows, Visual Studio 2022, CMake ≥ 3.22):

```
cmake -S roland-digital-combo -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target DigitalCombo_VST3 DigitalComboTests
build\DigitalComboTests_artefacts\Release\DigitalComboTests.exe
```

CI: `.github/workflows/digital-combo-windows.yml` (windows-2022) builds, runs the tests, the
Steinberg VST3 validator and pluginval (strictness 10), and uploads the VST3 zip.
What is verified and what is not: see `docs/VERIFICATION.md`.
