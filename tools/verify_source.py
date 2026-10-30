#!/usr/bin/env python3
"""Fast source-level regression checks; this is not a DSP or host test."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
processor = (root / "Source/PluginProcessor.cpp").read_text(encoding="utf-8")
editor = (root / "Source/PluginEditor.cpp").read_text(encoding="utf-8")
header = (root / "Source/PluginProcessor.h").read_text(encoding="utf-8")
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")

checks = {
    "nine original program labels": all(name in processor for name in (
        "AMBIENCE", "ROOM A1", "HALL C1", "PLATE A1", "HALL B3",
        "CHORUS", "ECHO", "NONLIN 2", "REVERSE 1")),
    "Echo A max 1200 ms": "juce::jlimit (1.0f, 1200.0f, pd)" in processor,
    "Echo B max 400 ms": "juce::jlimit (1.0f, 400.0f, echoBPd)" in processor,
    "Hall B3 DSP max 200 ms": "program == 0 || program == 2 || program == 4 ? 200.0f" in processor,
    "Hall B3 GUI max 200 ms": "if (p == 0 || p == 2 || p == 4) return 200.0f" in editor,
    "20 ms delay-control smoothing": "1.0f / (0.020f * (float) sr)" in processor,
    "sample-rate-dependent coefficient caches invalidated on reset": "cacheProgram = -1;" in processor and "cacheDecay = cacheEchoBDecay = -1.0f;" in processor,
    "smoothed Echo B delay is used": "echoR.readMs (dB, sr)" in processor and "echoBPd" in processor,
    "Chorus five voices span four 50 ms gaps": "pd * 4.0f * offsets[i]" in processor,
    "resizable editor": "setResizable (true, true)" in editor,
    "overlay coordinates scaled with faceplate": "g.addTransform (juce::AffineTransform::scale (sx, sy))" in editor,
    "Store/Recall handled by processor": "processor.storeUserPreset()" in editor and "processor.recallUserPreset()" in editor,
    "stored preset included in plugin state": "state.addChild (storedUserPreset.createCopy()" in processor and "restored.removeChild (preset" in processor,
    "Recall disabled when no slot stored": "recallButton.setEnabled (processor.hasStoredUserPreset())" in editor,
    "invalid host buffers are cleared to silence": "buffer.clear();" in processor and "buffer.getNumChannels() < 2" in processor,
    "state restore tolerates forward-compatible extra nodes": "tolerate additional state nodes" in processor and "restored.getNumChildren() !=" not in processor,
    "state restore rejects non-numeric values and duplicate IDs": "value.isDouble() || value.isInt() || value.isInt64()" in processor and "matches != 1" in processor,
    "VST3 format configured": "FORMATS VST3 Standalone" in cmake,
}

for label, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}  {label}")

if not all(checks.values()):
    raise SystemExit(1)
print(f"\n{len(checks)} source-level checks passed. No compilation or audio-runtime test is implied.")
