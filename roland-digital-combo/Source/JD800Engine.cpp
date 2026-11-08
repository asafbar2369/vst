#include "JD800Engine.h"

namespace dc
{
const char* jdWaveName (int index) noexcept
{
    static const char* names[] = { "Saw", "Square", "Pulse 25", "Pulse 12", "Triangle", "Sine", "Super Saw", "Noise",
                                   "Organ", "FM Bell", "FM Piano", "Metal", "Sync Saw", "Vox", "PWM", "Sub Saw" };
    return (index >= 0 && index < (int) JdWave::Count) ? names[index] : "?";
}

void JD800Engine::prepare (double sr, int maxBlock)
{
    sampleRate = float (sr);
    const size_t n = (size_t) std::max (64, maxBlock);
    for (auto* b : { &busL, &busR, &mono, &wetL, &wetR, &revL, &revR })
        b->assign (n, 0.0f);
    chorus.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    reset();
}

void JD800Engine::reset() noexcept
{
    allSoundOff();
    chorus.reset();
    delay.reset();
    reverb.reset();
}

int JD800Engine::activeVoiceCount() const noexcept
{
    int c = 0;
    for (auto& v : voices) c += v.isActive() ? 1 : 0;
    return c;
}

void JD800Engine::noteOn (int note, float velocity) noexcept
{
    Voice* target = nullptr;
    for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
    if (target == nullptr)
    {
        uint64_t best = UINT64_MAX;
        for (auto& v : voices)
            if (! v.held && ! v.sustained && v.age < best) { best = v.age; target = &v; }
        if (target == nullptr)
            for (auto& v : voices)
                if (v.age < best) { best = v.age; target = &v; }
    }
    auto& v = *target;
    v.note = note;
    v.velocity = clampf (velocity, 0.0f, 1.0f);
    v.held = true;
    v.sustained = false;
    v.age = ++counter;
    for (int t = 0; t < 4; ++t)
    {
        auto& ts = v.tone[t];
        const auto& tp = params.tone[t];
        ts.active = tp.on;
        if (! tp.on) { ts.aEnv.kill(); ts.fEnv.kill(); continue; }
        for (int k = 0; k < 8; ++k) ts.phase[k] = float (k) * 0.137f;
        ts.modPhase = 0;
        ts.fmEnv = 1.0f;
        ts.filter.reset(); ts.formant1.reset(); ts.formant2.reset();
        ts.rng.state = 0xABCD1234u + uint32_t (note * 31 + t);
        ts.lastAmp = 0;

        auto setupAdsr = [this] (SegmentEnvelope& e, float a, float d, float s, float r)
        {
            e.sampleRate = sampleRate;
            e.numSegments = 2;
            e.startLevel = 0.0f;
            e.times[0] = timeFromValue (a); e.levels[0] = 1.0f;
            e.times[1] = timeFromValue (d); e.levels[1] = clampf (s / 100.0f, 0.0f, 1.0f);
            e.releaseTime = timeFromValue (r);
            e.releaseLevel = 0.0f;
            e.noteOn();
        };
        setupAdsr (ts.aEnv, tp.aA, tp.aD, tp.aS, tp.aR);
        setupAdsr (ts.fEnv, tp.fA, tp.fD, tp.fS, tp.fR);
    }
}

void JD800Engine::releaseVoice (Voice& v) noexcept
{
    v.held = false;
    v.sustained = false;
    for (auto& ts : v.tone)
        if (ts.active) { ts.aEnv.noteOff(); ts.fEnv.noteOff(); }
}

void JD800Engine::noteOff (int note) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && v.held && v.note == note)
        {
            if (sustainDown) { v.held = false; v.sustained = true; }
            else releaseVoice (v);
        }
}

void JD800Engine::setSustain (bool down) noexcept
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.sustained) releaseVoice (v);
}

void JD800Engine::allNotesOff() noexcept
{
    sustainDown = false;
    for (auto& v : voices)
        if (v.isActive()) releaseVoice (v);
}

void JD800Engine::allSoundOff() noexcept
{
    sustainDown = false;
    for (auto& v : voices)
    {
        v.held = v.sustained = false;
        v.note = -1;
        for (auto& ts : v.tone) { ts.active = false; ts.aEnv.kill(); ts.fEnv.kill(); }
    }
}

float JD800Engine::oscillator (ToneState& ts, int wave, float dt) noexcept
{
    auto advance = [] (float& p, float inc) { p += inc; if (p >= 1.0f) p -= std::floor (p); };
    auto saw = [] (float p, float d) { return 2.0f * p - 1.0f - polyBlep (p, d); };
    auto pulse = [] (float p, float d, float duty)
    {
        float x = p < duty ? 1.0f : -1.0f;
        x += polyBlep (p, d);
        float p2 = p + 1.0f - duty;
        p2 -= std::floor (p2);
        return x - polyBlep (p2, d);
    };
    float x = 0.0f;
    float& p = ts.phase[0];
    switch ((JdWave) wave)
    {
        case JdWave::Saw:      x = saw (p, dt); break;
        case JdWave::Square:   x = pulse (p, dt, 0.5f); break;
        case JdWave::Pulse25:  x = pulse (p, dt, 0.25f); break;
        case JdWave::Pulse12:  x = pulse (p, dt, 0.125f); break;
        case JdWave::Triangle: x = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; break;
        case JdWave::Sine:     x = std::sin (kTwoPi * p); break;
        case JdWave::SuperSaw:
        {
            static constexpr float detune[7] = { 1.0f, 0.9937f, 1.0063f, 0.9891f, 1.0109f, 0.9811f, 1.0191f };
            for (int k = 0; k < 7; ++k)
            {
                const float d = std::min (0.45f, dt * detune[k]);
                x += saw (ts.phase[k], d) * (k == 0 ? 0.3f : 0.18f);
                if (k > 0) advance (ts.phase[k], d);
            }
            break;
        }
        case JdWave::Noise:    x = ts.rng.next(); break;
        case JdWave::Organ:
        {
            static constexpr float h[6] = { 1, 2, 3, 4, 6, 8 };
            static constexpr float a[6] = { 0.5f, 0.4f, 0.3f, 0.2f, 0.15f, 0.1f };
            for (int k = 0; k < 6; ++k) x += a[k] * std::sin (kTwoPi * p * h[k]);
            break;
        }
        case JdWave::FmBell:
            advance (ts.modPhase, std::min (0.45f, dt * 3.5f));
            ts.fmEnv *= 0.99997f;
            x = std::sin (kTwoPi * p + 4.0f * ts.fmEnv * std::sin (kTwoPi * ts.modPhase));
            break;
        case JdWave::FmPiano:
            advance (ts.modPhase, dt);
            ts.fmEnv *= 0.99985f;
            x = std::sin (kTwoPi * p + 1.8f * ts.fmEnv * std::sin (kTwoPi * ts.modPhase));
            break;
        case JdWave::Metal:
        {
            static constexpr float r[4] = { 1.0f, 2.76f, 5.40f, 8.93f };
            x = 0.4f * std::sin (kTwoPi * p);
            for (int k = 1; k < 4; ++k)
            {
                advance (ts.phase[k], std::min (0.45f, dt * r[k]));
                x += 0.3f / float (k) * std::sin (kTwoPi * ts.phase[k]);
            }
            break;
        }
        case JdWave::SyncSaw:
        {
            const float d = std::min (0.45f, dt * 2.3f);
            x = 2.0f * ts.phase[1] - 1.0f;
            advance (ts.phase[1], d);
            if (p + dt >= 1.0f) ts.phase[1] = 0.0f; // hard sync to master
            break;
        }
        case JdWave::Vox:
        {
            const float s = saw (p, dt);
            float l, b, hp, b2;
            ts.formant1.process (s, l, b, hp);
            ts.formant2.process (s, l, b2, hp);
            x = 1.5f * (b + 0.6f * b2);
            break;
        }
        case JdWave::Pwm:
            advance (ts.pwmPhase, 0.7f / sampleRate);
            x = pulse (p, dt, 0.5f + 0.4f * std::sin (kTwoPi * ts.pwmPhase));
            break;
        case JdWave::SubSaw:
        {
            x = 0.7f * saw (p, dt);
            const float d = dt * 0.5f;
            x += 0.4f * pulse (ts.phase[1], d, 0.5f);
            advance (ts.phase[1], d);
            break;
        }
        case JdWave::Count: break;
    }
    advance (p, dt);
    return x;
}

void JD800Engine::renderTone (ToneState& ts, const JdToneParams& tp, float note, float velocity,
                              float* L, float* R, int n) noexcept
{
    const float panL = std::cos ((tp.pan + 1.0f) * kPi * 0.25f);
    const float panR = std::sin ((tp.pan + 1.0f) * kPi * 0.25f);
    const float velGain = 1.0f - tp.velSens * (1.0f - velocity);
    const float levelGain = tp.level * tp.level * velGain * 0.25f;

    if ((JdWave) tp.wave == JdWave::Vox)
    {
        ts.formant1.set (700.0f, 6.0f, sampleRate);
        ts.formant2.set (1150.0f, 8.0f, sampleRate);
    }

    for (int start = 0; start < n; start += kControlRate)
    {
        const int cb = std::min (kControlRate, n - start);
        const float lfo = std::sin (kTwoPi * lfoPhase);
        const float pitch = note + tp.coarse + tp.fineCents * 0.01f + bend * float (params.bendRange)
                          + lfo * params.lfoPitchDepth * modWheel;
        const float dt = std::min (0.45f, midiToHz (clampf (pitch, -24.0f, 150.0f)) / sampleRate);

        const float fEnv = ts.fEnv.advance (cb);
        const float prevAmp = ts.lastAmp;
        const float aEnv = ts.aEnv.advance (cb);
        const float oct = tp.cutoff * 0.1f + 0.5f * (note - 60.0f) / 12.0f + tp.envDepth * 7.0f * fEnv;
        ts.filter.set (20.0f * std::exp2 (clampf (oct, 0.0f, 10.5f)), 0.5f * std::pow (20.0f, tp.resonance / 100.0f), sampleRate);

        const float target = aEnv * levelGain;
        const float inc = (target - prevAmp) / float (cb);
        float a = prevAmp;
        for (int s = 0; s < cb; ++s)
        {
            float lp, bp, hp;
            ts.filter.process (oscillator (ts, tp.wave, dt), lp, bp, hp);
            const float y = tp.filterMode == 0 ? lp : (tp.filterMode == 1 ? bp : hp);
            a += inc;
            L[start + s] += y * a * panL;
            R[start + s] += y * a * panR;
        }
        ts.lastAmp = target;
    }
    ts.active = ts.aEnv.isActive();
}

void JD800Engine::render (float* L, float* R, int n) noexcept
{
    const int maxChunk = (int) busL.size();
    for (int offset = 0; offset < n; offset += maxChunk)
    {
        const int len = std::min (maxChunk, n - offset);
        std::fill (busL.begin(), busL.begin() + len, 0.0f);
        std::fill (busR.begin(), busR.begin() + len, 0.0f);

        for (auto& v : voices)
        {
            if (! v.isActive()) continue;
            for (int t = 0; t < 4; ++t)
                if (v.tone[t].active)
                    renderTone (v.tone[t], params.tone[t], float (v.note), v.velocity, busL.data(), busR.data(), len);
            if (! v.isActive()) { v.note = -1; v.held = v.sustained = false; }
        }
        lfoPhase += params.lfoRate * float (len) / sampleRate;
        lfoPhase -= std::floor (lfoPhase);

        // Chorus (wet only, blended so tone panning is preserved)
        for (int i = 0; i < len; ++i) mono[(size_t) i] = 0.5f * (busL[(size_t) i] + busR[(size_t) i]);
        chorus.process (mono.data(), wetL.data(), wetR.data(), len, params.chorusRate, 7.0f, 4.0f, 0.0f, 0.0f, 1.0f);
        const float cm = params.chorusMix;
        for (int i = 0; i < len; ++i)
        {
            busL[(size_t) i] = busL[(size_t) i] * (1.0f - 0.5f * cm) + wetL[(size_t) i] * cm;
            busR[(size_t) i] = busR[(size_t) i] * (1.0f - 0.5f * cm) + wetR[(size_t) i] * cm;
        }
        delay.process (busL.data(), busR.data(), len, params.delayTimeMs, params.delayFeedback, params.delayMix);
        reverb.process (busL.data(), busR.data(), revL.data(), revR.data(), len,
                        params.reverbSize, 0.6f + 5.0f * params.reverbSize, 7000.0f, 12.0f);
        for (int i = 0; i < len; ++i)
        {
            L[offset + i] += (busL[(size_t) i] + revL[(size_t) i] * params.reverbMix) * params.volume;
            R[offset + i] += (busR[(size_t) i] + revR[(size_t) i] * params.reverbMix) * params.volume;
        }
    }
}
} // namespace dc
