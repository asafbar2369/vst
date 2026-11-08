# Digital Combo (unofficial D-50 / JD-800 style VST3)

Independent JUCE VST3 instrument for Windows x64 with two original synthesis engines:

* **D-50 engine** – LA-style structure (2 tones x 2 partials, 7 structures incl. ring modulator,
  TVF/TVA 5-time envelopes, P-ENV, 3 LFOs, EQ, chorus, reverb). Its parameters live in a
  448-byte temporary area laid out exactly as in Roland's *D-50 MIDI Implementation v2.00*, so
  D-50 DT1/RQ1 System Exclusive messages and `.syx` files address the same bytes.
* **JD-800 style engine** – 4 layered tones (A–D), each with waveform, LPF/BPF/HPF, filter and amp
  envelopes, plus chorus, delay and reverb.
* **Play modes** – JD-800, D-50, LAYER, SPLIT.

No Roland ROM, PCM samples, firmware, factory patches or artwork are included. PCM partials and
JD waveforms are generated algorithmically and do **not** sound like the original ROM.
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
