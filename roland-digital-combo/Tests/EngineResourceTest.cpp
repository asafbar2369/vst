// Standalone engine-level regression test: no JUCE or proprietary ROM data required.
#include "JD800Engine.h"
#include "D50Engine.h"
#include <cassert>
#include <iostream>

int main()
{
    dc::JD800Engine jd;
    jd.prepare(48000.0, 128);
    dc::JdPatch patch {};
    patch.common.layerMask = 1;
    patch.tone[0].waveform = 0;
    jd.setPatch(patch);
    for (int i = 0; i < 24; ++i) jd.noteOn(36 + i, 0.8f);
    assert(jd.activeVoiceCount() == 24);
    assert(jd.activeToneCount(0) == 24);
    jd.noteOn(72, 0.8f);
    assert(jd.activeVoiceCount() == 24 && jd.activeToneCount(0) == 24);

    jd.allSoundOff();
    patch.common.layerMask = 15;
    for (auto& t : patch.tone) t.waveform = 0;
    jd.setPatch(patch);
    for (int i = 0; i < 15; ++i) jd.noteOn(36 + i, 0.8f);
    assert(jd.activeVoiceCount() == 6);
    for (int t = 0; t < 4; ++t) assert(jd.activeToneCount(t) == 6);

    dc::D50Engine d50;
    d50.prepare(48000.0, 128);
    dc::D50Params d50Patch {};
    d50Patch.patch.keyMode = 0;
    d50.setParams(d50Patch);
    for (int i = 0; i < 20; ++i) d50.noteOn(40 + i, 0.8f);
    assert(d50.activeVoiceCount() == 16);
    d50.allSoundOff();
    d50Patch.patch.keyMode = 1;
    d50.setParams(d50Patch);
    for (int i = 0; i < 20; ++i) d50.noteOn(40 + i, 0.8f);
    assert(d50.activeVoiceCount() == 8);
    assert(d50.activeToneCount(true) == 8);
    assert(d50.activeToneCount(false) == 8);
    std::cout << "PASS: JD-800 24-Tone resources (1 and 4 Tone patches); D-50 WHOLE 16 / DUAL 8\n";
}
