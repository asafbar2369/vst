# QA Report

Every line states how it was established. Labels: **TESTED AND VERIFIED** (an automated test or validator ran and passed
in the stated environment), **MEASURED** (numbers recorded, see the linked report), **IMPLEMENTED** (code exists, behaviour
not independently verified), **UNVERIFIED** (not tested), **NOT IMPLEMENTED**.

Date: 2026-11-20. Branch `claude/bold-cori-kvu779`.

## 1. Test environments

| Environment | Where | What ran |
|---|---|---|
| Linux x86_64 (Ubuntu 24.04, GCC 13.3, Xvfb) | isolated cloud development container | full test runner incl. measurements, editor screenshots, callgrind profiling |
| Windows x64 (`windows-latest` GitHub-hosted runner, MSVC, x64) | GitHub Actions run #38, commit `59eed18` | Release build of VST3 + Standalone + test runner, full test runner, pluginval, Steinberg validator, installer build and silent install / uninstall |

No other computer was accessed. FL Studio was not available in either environment.

## 2. Results

| Check | Environment | Result | Status |
|---|---|---|---|
| Release build: VST3 + Standalone EXE + test runner (MSVC x64) | Windows CI #38 | built | TESTED AND VERIFIED |
| Release build: VST3 + Standalone (GCC, Linux) | container | built | TESTED AND VERIFIED |
| Automated test runner (engine, filters, effects, presets, AI validation, providers vs mock server, controller workflow, processor matrix, state recall, automation, 5-min long run, editor) | Windows CI #38 | **909,808 checks passed, 0 failed** | TESTED AND VERIFIED |
| Same runner + sound-matching benchmark | Linux container (commit 9104fdb + matching test) | 909,805 + 18 checks passed, 0 failed | TESTED AND VERIFIED |
| Independent core DSP / AI test target (`VSX_CORE_ONLY`) | Windows CI #38 | passed | TESTED AND VERIFIED |
| pluginval v1.0.4, strictness level 10 (26 test groups incl. state restoration, automation, parameter thread safety, fuzzing, editor while processing) | Windows CI #38 | SUCCESS | TESTED AND VERIFIED |
| Steinberg VST3 validator (built from vst3sdk v3.8.1_build_84) | Windows CI #38 | 47 tests passed, 0 failed (general, 32-bit, 64-bit) | TESTED AND VERIFIED |
| Inno Setup installer build | Windows CI #38 | built `VICE-SYNTH-X-Setup-1.0.0-x64.exe` | TESTED AND VERIFIED |
| Silent install (VST3 to Common Files\VST3, EXE to Program Files) and uninstall (both removed) | Windows CI #38 | installer exit 0, uninstaller exit 0, files removed | TESTED AND VERIFIED |
| Editor renders all four views at 1648 x 928 and scales 50-200 % with fixed aspect ratio; every knob bound to a registry parameter; clean teardown | Linux and Windows | pass | TESTED AND VERIFIED |
| Sample rates 44.1 / 48 / 88.2 / 96 / 192 kHz x buffers 32-1024 | Linux and Windows | finite, non-silent, bounded | TESTED AND VERIFIED |
| Audio measurements (filters, oscillators, effects) | Linux | see AUDIO_QUALITY_REPORT.md | MEASURED |
| CPU realtime factors | Linux (single machine) | see CPU_BENCHMARK_REPORT.md | MEASURED |
| Factory library levels, peaks, duplicates | Linux | see FACTORY_LIBRARY_QA.md | MEASURED |
| Live Claude / OpenAI / Ollama / LM Studio connectivity | - | not run (no keys / servers in the build environment) | UNVERIFIED |
| FL Studio 25 compatibility | - | not run (no FL Studio in an authorised environment) | UNVERIFIED |
| Other hosts (Ableton, Cubase, Reaper, Bitwig) | - | not run | UNVERIFIED |
| Memory-leak / race detectors (ASan, TSan, Dr. Memory) | - | not run | UNVERIFIED |
| Listening tests | - | not run | UNVERIFIED |

pluginval and the Steinberg validator load the plug-in in a generic VST3 host environment; passing them is strong evidence
of VST3 conformance but is **not** a substitute for testing inside FL Studio 25.

## 3. Defects found by this QA cycle and fixed

| Defect | Found by | Fix |
|---|---|---|
| Windows: tests crashed (stack overflow) - a 1.09 MB synth engine was created on a 1 MB thread stack | Windows CI | voice pool moved to the heap (engine 17.8 KB) |
| Uninstaller test: relative Inno log path and self-relaunching uninstaller | Windows CI | absolute log path, wait for removal |
| SEM / Analog SVF: Drive had no timbral effect (0 % THD) | filter measurements | input saturation stage |
| Arp / sequencer presets up to +9 dBFS (calibration had rendered them with the arp off) | library measurements | as-played calibration of all 85 presets |
| Editor: header and hero artwork painted over their controls; settings controls laid out in the wrong column | editor screenshots | paint order and coordinate fixes |
| Ladder filter silent after an optimisation (shared coefficients not computed for the ladder) | regression suite before commit | coefficients computed for the ladder too |

## 4. Logs

- Linux: `docs/qa/linux-test-run-9104fdb.log`, `docs/qa/linux-matching-benchmark-9104fdb.log`
- Windows: GitHub Actions run 44830515083, artifact `VICE-SYNTH-X-validation-logs` (tests.log, pluginval logs,
  vst3-validator.log, install / uninstall logs, editor screenshots) and `VICE-SYNTH-X-Windows-x64` (VST3 zip, EXE,
  Setup.exe, SHA256SUMS.txt). Artifacts expire after 14 / 30 days.

See KNOWN_LIMITATIONS.md for everything that is open.
