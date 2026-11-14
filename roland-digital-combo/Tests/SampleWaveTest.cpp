#include "DspCommon.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    dc::SampleWave w;
    w.data = { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };
    w.loop = true;
    w.loopStart = 2;
    w.loopEnd = 6;
    float pos = 5.5f;
    const float boundary = w.read (pos, 0.5f);
    assert (std::abs (boundary - 0.7f) < 1.0e-6f); // interpolate end->loop start
    assert (std::abs (pos - 2.0f) < 1.0e-6f);
    assert (std::abs (w.read (pos, 1.0f) - 0.4f) < 1.0e-6f);
    pos = 5.0f;
    w.read (pos, 1000000.0f); // must wrap in constant time
    assert (std::isfinite (pos) && pos >= 2.0f && pos < 6.0f);
    pos = -120.0f;
    assert (std::isfinite (w.read (pos, 0.3f)));
    pos = 1000000.5f;
    assert (std::isfinite (w.read (pos, 0.3f)) && pos >= 2.0f && pos < 6.0f);
    w.loop = false;
    pos = 4.5f;
    assert (std::abs (w.read (pos, 0.2f) - 0.9f) < 1.0e-6f);
    pos = 6.0f;
    assert (w.read (pos, 0.3f) == 0.0f);
    pos = -100.0f;
    assert (std::isfinite (w.read (pos, 1.0f)));
    std::cout << "PASS: sample loop interpolation, fast wrap and out-of-range guards\n";
}
