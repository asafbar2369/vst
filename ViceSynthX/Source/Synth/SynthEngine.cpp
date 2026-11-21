#include "SynthEngine.h"

namespace vsx
{
using namespace pid;

SynthEngine::SynthEngine (PatchState& ps) : patch (ps)
{
    for (int k = 0; k < 128; ++k) tuningShared[(size_t) k].store ((float) k, std::memory_order_relaxed);
    for (auto& m : modDisplay) m.store (-1.0f);
    for (auto& e : envDisplay) e.store (0.0f);
    for (auto& l : lfoDisplay) l.store (0.0f);
}

void SynthEngine::prepare (double sr, int)
{
    sampleRate = std::isfinite (sr) && sr > 0.0 ? sr : 44100.0;
    using OS = juce::dsp::Oversampling<float>;
    oversamplers[0].reset();
    oversamplers[1] = std::make_unique<OS> (2, 1, OS::filterHalfBandPolyphaseIIR, true, false);
    oversamplers[2] = std::make_unique<OS> (2, 2, OS::filterHalfBandPolyphaseIIR, true, false);
    oversamplers[3] = std::make_unique<OS> (2, 3, OS::filterHalfBandPolyphaseIIR, true, false);
    for (int i = 1; i < 4; ++i)
    {
        oversamplers[(size_t) i]->initProcessing ((size_t) kChunk);
        oversamplers[(size_t) i]->reset();
    }
    chunkBuffer.setSize (2, kChunk, false, true, false);
    chunkMidi.ensureSize (4096);
    arpMidi.ensureSize (4096);
    arp.prepare (sampleRate);
    prepared = true;
    quality = -1;
    setQuality (1);
    reset();
}

int SynthEngine::latencyFor (int q) const noexcept
{
    q = juce::jlimit (0, 3, q);
    if (q == 0 || oversamplers[(size_t) q] == nullptr) return 0;
    return (int) std::round (oversamplers[(size_t) q]->getLatencyInSamples());
}

void SynthEngine::setQuality (int q)
{
    q = juce::jlimit (0, 3, q);
    if (q == quality) return;
    quality = q;
    osFactor = 1 << q;
    const double srOs = sampleRate * osFactor;
    const double controlRate = sampleRate / kTick;
    for (auto& v : voices)
    {
        v.prepare (srOs, controlRate);
        v.hardStop();
    }
    for (int i = 1; i < 4; ++i)
        if (oversamplers[(size_t) i] != nullptr) oversamplers[(size_t) i]->reset();
    ctx.sampleRateOs = srOs;
    ctx.controlRate = controlRate;
    ctx.osFactor = osFactor;
}

void SynthEngine::reset()
{
    for (auto& v : voices) v.hardStop();
    noteStackSize = 0;
    sustainPedal = false;
    // A fresh render must not inherit pitch glide, random seeds or macro motion
    // from a previous candidate evaluated by SoundMatcher.
    ageCounter = 0;
    lastPlayedPitch = -1.0f;
    motionTime = 0.0;
    globalMods.clearAll();
    globalSources.fill (0.0f);
    ctx.seqMod = 0.0f;
    ctx.lfoGlobalPhase.fill (0.0);
    ctx.lfoGlobalCycle.fill (0.0);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    lastNote.store (-1, std::memory_order_relaxed);
    seqStepDisplay.store (-1, std::memory_order_relaxed);
    for (auto& m : modDisplay) m.store (-1.0f, std::memory_order_relaxed);
    for (auto& e : envDisplay) e.store (0.0f, std::memory_order_relaxed);
    for (auto& l : lfoDisplay) l.store (0.0f, std::memory_order_relaxed);
    numDisplayed = 0;
    arp.reset();
    for (auto& p : lfoPhase) p = 0.0;
    for (auto& c : lfoCycle) c = 0.0;
    for (int i = 1; i < 4; ++i)
        if (oversamplers[(size_t) i] != nullptr) oversamplers[(size_t) i]->reset();
}

void SynthEngine::allNotesOff()
{
    for (auto& v : voices) if (v.isActive()) v.release();
    noteStackSize = 0;
}

// ---------------------------------------------------------------------------------
Voice* SynthEngine::findFreeVoice()
{
    for (auto& v : voices)
        if (! v.isActive()) return &v;
    return nullptr;
}

Voice* SynthEngine::chooseVictim()
{
    Voice* best = nullptr;
    // 1) released voices, quietest first
    for (auto& v : voices)
        if (v.isActive() && ! v.isKilled() && v.isReleased() && ! v.sustained)
            if (best == nullptr || v.level() < best->level()) best = &v;
    if (best != nullptr) return best;
    // 2) oldest held voice
    for (auto& v : voices)
        if (v.isActive() && ! v.isKilled())
            if (best == nullptr || v.age < best->age) best = &v;
    return best;
}

Voice* SynthEngine::monoVoice()
{
    Voice* best = nullptr;
    for (auto& v : voices)
        if (v.isActive() && ! v.isKilled())
            if (best == nullptr || v.age > best->age) best = &v;
    return best;
}

Voice* SynthEngine::newestVoice()
{
    Voice* best = nullptr;
    for (auto& v : voices)
        if (v.isActive() && (best == nullptr || v.age > best->age)) best = &v;
    return best;
}

void SynthEngine::publishTuning (const tuning::Table& t) noexcept
{
    tuningVersion.fetch_add (1, std::memory_order_acq_rel); // odd: being written
    for (int k = 0; k < 128; ++k) tuningShared[(size_t) k].store (t[(size_t) k], std::memory_order_relaxed);
    tuningVersion.fetch_add (1, std::memory_order_acq_rel); // even: complete
}

void SynthEngine::refreshTuning() noexcept
{
    const uint32_t v = tuningVersion.load (std::memory_order_acquire);
    if (v == tuningSeen || (v & 1u) != 0u) return;
    tuning::Table copy;
    for (int k = 0; k < 128; ++k) copy[(size_t) k] = tuningShared[(size_t) k].load (std::memory_order_relaxed);
    std::atomic_thread_fence (std::memory_order_acquire);
    if (tuningVersion.load (std::memory_order_relaxed) != v) return; // changed while copying: take it next block
    tuningLocal = copy;
    tuningSeen = v;
}

void SynthEngine::noteOn (int note, int velocity, const ParamSnapshot& p)
{
    if (note < 0 || note > 127 || tuningLocal[(size_t) note] <= tuning::kUnmapped + 1.0f) return; // unmapped key: silent
    const int voiceMode = p.geti (G (global::VoiceMode));
    const float glideTime = p.get (G (global::GlideTime));
    const bool glideAlways = p.geti (G (global::GlideMode)) == 0;
    const uint32_t seed = hash32 ((uint32_t) (++ageCounter * 2654435761ull) ^ (uint32_t) note);
    lastNote.store (note);

    if (voiceMode == 0)
    {
        bool anyHeld = false;
        for (auto& v : voices)
        {
            if (v.isActive() && ! v.isReleased()) anyHeld = true;
            if (v.isActive() && v.note == note && ! v.isKilled()) v.release();
        }

        const int poly = p.geti (G (global::Polyphony));
        int count = 0;
        for (auto& v : voices) if (v.isActive() && ! v.isKilled()) ++count;
        while (count >= poly)
        {
            if (auto* victim = chooseVictim()) { victim->kill(); --count; }
            else break;
        }

        Voice* v = findFreeVoice();
        if (v == nullptr)
        {
            // every slot is fading out: hard-steal the quietest
            for (auto& c : voices) if (v == nullptr || c.level() < v->level()) v = &c;
            v->hardStop();
        }
        const bool glide = glideTime > 1.0e-4f && lastPlayedPitch >= 0.0f && (glideAlways || anyHeld);
        v->start (note, velocity, seed, glide ? lastPlayedPitch : -1.0f, false, ctx);
        v->age = ageCounter;
    }
    else
    {
        // mono / legato with last-note priority
        for (int i = 0; i < noteStackSize; ++i)
            if (noteStack[(size_t) i] == note) { std::copy (noteStack.begin() + i + 1, noteStack.begin() + noteStackSize, noteStack.begin() + i); --noteStackSize; break; }
        if (noteStackSize < 128) noteStack[(size_t) noteStackSize++] = note;

        Voice* mv = monoVoice();
        if (mv != nullptr && ! mv->isReleased())
        {
            const bool retrig = voiceMode == 1;
            mv->retarget (note, velocity, retrig, glideTime <= 1.0e-4f, ctx);
            mv->age = ageCounter;
        }
        else if (mv != nullptr)
        {
            mv->retarget (note, velocity, true, ! (glideAlways && glideTime > 1.0e-4f), ctx);
            mv->age = ageCounter;
        }
        else
        {
            for (auto& v : voices) if (v.isActive()) v.kill();
            Voice* v = findFreeVoice();
            if (v == nullptr) { v = &voices[0]; v->hardStop(); }
            const bool glide = glideAlways && glideTime > 1.0e-4f && lastPlayedPitch >= 0.0f;
            v->start (note, velocity, seed, glide ? lastPlayedPitch : -1.0f, false, ctx);
            v->age = ageCounter;
        }
    }
    lastPlayedPitch = tuningLocal[(size_t) note];
}

void SynthEngine::noteOff (int note, const ParamSnapshot& p)
{
    const int voiceMode = p.geti (G (global::VoiceMode));
    if (voiceMode == 0)
    {
        for (auto& v : voices)
            if (v.isActive() && v.note == note && ! v.isReleased())
            {
                if (sustainPedal) v.sustained = true;
                else v.release();
            }
        return;
    }

    for (int i = 0; i < noteStackSize; ++i)
        if (noteStack[(size_t) i] == note) { std::copy (noteStack.begin() + i + 1, noteStack.begin() + noteStackSize, noteStack.begin() + i); --noteStackSize; break; }

    Voice* mv = monoVoice();
    if (mv == nullptr || mv->note != note) return;
    if (noteStackSize > 0)
    {
        const float glideTime = p.get (G (global::GlideTime));
        mv->retarget (noteStack[(size_t) noteStackSize - 1], mv->velocity, voiceMode == 1, glideTime <= 1.0e-4f, ctx);
        lastPlayedPitch = tuningLocal[(size_t) noteStack[(size_t) noteStackSize - 1]];
    }
    else if (sustainPedal)
        mv->sustained = true;
    else
        mv->release();
}

void SynthEngine::handleMidi (const juce::MidiMessage& m, const ParamSnapshot& p)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getVelocity(), p);
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber(), p);
    else if (m.isPitchWheel())
        bend = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8191.0f);
    else if (m.isChannelPressure())
        pressure = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
    {
        for (auto& v : voices)
            if (v.isActive() && v.note == m.getNoteNumber()) v.polyPressure = (float) m.getAfterTouchValue() / 127.0f;
    }
    else if (m.isController())
    {
        const float val = (float) m.getControllerValue() / 127.0f;
        switch (m.getControllerNumber())
        {
            case 1: modWheel = val; break;
            case 2: breath = val; break;
            case 11: expression = val; break;
            case 74: timbre = val; break;
            case 64:
                sustainPedal = val >= 0.5f;
                if (! sustainPedal)
                    for (auto& v : voices)
                        if (v.isActive() && v.sustained) { v.sustained = false; v.release(); }
                break;
            case 120: for (auto& v : voices) v.hardStop(); noteStackSize = 0; break;
            case 123: allNotesOff(); break;
            default: break;
        }
    }
    else if (m.isAllNotesOff())
        allNotesOff();
    else if (m.isAllSoundOff())
        for (auto& v : voices) v.hardStop();
}

// ---------------------------------------------------------------------------------
void SynthEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, ParamSnapshot& params,
                           const TransportInfo& transport)
{
    jassert (prepared);
    const int total = buffer.getNumSamples();
    buffer.clear();
    if (! prepared || total == 0 || buffer.getNumChannels() == 0) return;

    setQuality (params.geti (G (global::Quality)));

    // engine-level macros (Mood, Filter Sweep, Motion, ...) with a slow internal motion signal
    motionTime += (double) total / sampleRate;
    auto smoothRandom = [] (double t, uint32_t seed)
    {
        const double c = std::floor (t);
        const float f = (float) (t - c);
        const float a = hashToBi ((uint32_t) (int64_t) c * 2654435761u + seed), b = hashToBi ((uint32_t) (int64_t) (c + 1.0) * 2654435761u + seed);
        return a + (b - a) * (0.5f - 0.5f * std::cos (kPi * f));
    };
    macroBase.value = params.value;
    Macros::apply (macroBase, params, smoothRandom (motionTime * 0.23, 17u), smoothRandom (motionTime * 0.31, 91u));
    matrix.capture (patch, params);

    refreshTuning();
    ctx.tuning = tuningLocal.data();
    ctx.params = &params;
    ctx.matrix = &matrix;
    for (int o = 0; o < kNumOscs; ++o)
        ctx.res[(size_t) o] = { patch.osc[(size_t) o].wavetable.get(), patch.osc[(size_t) o].sample.get(), patch.osc[(size_t) o].multi.get(),
                                 patch.osc[(size_t) o].fm.get() };
    for (int l = 0; l < kNumLfos; ++l)
        ctx.lfoTables[(size_t) l] = patch.lfoTables[(size_t) l].get();
    ctx.bpm = transport.bpm > 0.0 ? transport.bpm : 120.0;
    for (int m = 0; m < kNumMacros; ++m)
        ctx.macros[(size_t) m] = params.get (MACRO (m));

    const double samplesPerBeat = sampleRate * 60.0 / ctx.bpm;
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int start = 0; start < total; start += kChunk)
    {
        const int n = std::min (kChunk, total - start);
        chunkMidi.clear();
        for (const auto meta : midi)
            if (meta.samplePosition >= start && meta.samplePosition < start + n)
                chunkMidi.addEvent (meta.data, meta.numBytes, meta.samplePosition - start);

        TransportInfo t = transport;
        t.ppqPosition += start / samplesPerBeat;

        chunkBuffer.clear();
        processChunk (chunkBuffer.getWritePointer (0), chunkBuffer.getWritePointer (1), n, chunkMidi, params, t);

        std::copy (chunkBuffer.getReadPointer (0), chunkBuffer.getReadPointer (0) + n, outL + start);
        if (outR != nullptr)
            std::copy (chunkBuffer.getReadPointer (1), chunkBuffer.getReadPointer (1) + n, outR + start);
    }

    publishDisplay (params);
}

void SynthEngine::processChunk (float* outL, float* outR, int numSamples, const juce::MidiBuffer& midi, ParamSnapshot& params,
                                const TransportInfo& transport)
{
    arpMidi.clear();
    arp.process (midi, arpMidi, numSamples, params, patch.arpPattern.get(), patch.seqPattern.get(), transport);
    ctx.seqMod = arp.getSeqMod();
    seqStepDisplay.store (arp.getCurrentStep());

    float* osL = outL;
    float* osR = outR;
    juce::dsp::AudioBlock<float> outBlock (chunkBuffer);
    outBlock = outBlock.getSubBlock (0, (size_t) numSamples);
    juce::dsp::AudioBlock<float> osBlock;
    if (quality > 0)
    {
        osBlock = oversamplers[(size_t) quality]->processSamplesUp (outBlock);
        osBlock.clear();
        osL = osBlock.getChannelPointer (0);
        osR = osBlock.getChannelPointer (1);
    }

    const double samplesPerBeat = sampleRate * 60.0 / ctx.bpm;
    auto it = arpMidi.cbegin();
    const auto end = arpMidi.cend();
    int pos = 0;
    while (pos < numSamples)
    {
        while (it != end && (*it).samplePosition <= pos)
        {
            handleMidi ((*it).getMessage(), params);
            ++it;
        }
        int next = std::min (numSamples, pos + kTick);
        if (it != end) next = std::min (next, std::max (pos + 1, (*it).samplePosition));
        renderTick (osL, osR, pos, next - pos, params, transport, transport.ppqPosition + pos / samplesPerBeat);
        pos = next;
    }
    // events exactly at the block end
    while (it != end) { handleMidi ((*it).getMessage(), params); ++it; }

    if (quality > 0)
        oversamplers[(size_t) quality]->processSamplesDown (outBlock);
}

void SynthEngine::renderTick (float* osL, float* osR, int startBase, int numBase, const ParamSnapshot& p, const TransportInfo& t,
                              double tickPpq)
{
    const double dtSec = (double) numBase / sampleRate;

    // global LFO phases (Free mode): host-locked when synced and playing
    for (int l = 0; l < kNumLfos; ++l)
    {
        if (p.getb (L (l, lfo::Sync)))
        {
            const double beats = ParamRegistry::syncDivisionBeats (p.geti (L (l, lfo::SyncDiv)));
            if (t.isPlaying && t.hasPpq)
                lfoCycle[(size_t) l] = tickPpq / beats;
            else
                lfoCycle[(size_t) l] += dtSec * (ctx.bpm / 60.0) / beats;
        }
        else
            lfoCycle[(size_t) l] += dtSec * globalMods.get (p, L (l, lfo::Rate));
        if (lfoCycle[(size_t) l] > 1.0e9) lfoCycle[(size_t) l] = 0.0;
        lfoPhase[(size_t) l] = lfoCycle[(size_t) l] - std::floor (lfoCycle[(size_t) l]);
    }
    ctx.lfoGlobalPhase = lfoPhase;
    ctx.lfoGlobalCycle = lfoCycle;
    ctx.modWheel = modWheel;
    ctx.channelPressure = pressure;
    ctx.pitchBend = bend;
    ctx.expression = expression;
    ctx.breath = breath;
    ctx.timbre = timbre;

    const int n = numBase * osFactor;
    float* l = osL + startBase * osFactor;
    float* r = osR + startBase * osFactor;
    int count = 0;
    for (auto& v : voices)
    {
        if (! v.isActive()) continue;
        v.renderTick (ctx, l, r, n);
        if (v.isActive()) ++count;
    }
    activeVoiceCount.store (count, std::memory_order_relaxed);

    // global modulation context (used by the effects and display): newest voice, else global LFOs
    if (auto* nv = newestVoice())
        std::copy (nv->getSources(), nv->getSources() + modsrc::Count, globalSources.begin());
    else
    {
        globalSources.fill (0.0f);
        for (int li = 0; li < kNumLfos; ++li)
            globalSources[(size_t) (modsrc::Lfo1 + li)] = Lfo::shapeValue (p.geti (L (li, lfo::Shape)), (float) lfoPhase[(size_t) li],
                                                                          hashToBi ((uint32_t) std::floor (lfoCycle[(size_t) li])),
                                                                          hashToBi ((uint32_t) std::floor (lfoCycle[(size_t) li]) + 1u),
                                                                          ctx.lfoTables[(size_t) li]) * p.get (L (li, lfo::Depth));
        for (int m = 0; m < kNumMacros; ++m) globalSources[(size_t) (modsrc::Macro1 + m)] = ctx.macros[(size_t) m];
        globalSources[modsrc::ModWheel] = modWheel;
        globalSources[modsrc::Aftertouch] = pressure;
        globalSources[modsrc::PitchBend] = bend;
        globalSources[modsrc::Expression] = expression;
        globalSources[modsrc::Breath] = breath;
        globalSources[modsrc::Timbre] = timbre;
        globalSources[modsrc::SeqMod] = ctx.seqMod;
    }
    globalMods.apply (matrix, globalSources.data());
}

void SynthEngine::publishDisplay (const ParamSnapshot& params)
{
    for (int i = 0; i < numDisplayed; ++i)
        modDisplay[(size_t) displayed[(size_t) i]].store (-1.0f, std::memory_order_relaxed);
    numDisplayed = 0;

    const Voice* nv = newestVoice();
    const ModOffsets& m = nv != nullptr ? nv->getMods() : globalMods;
    for (int d = 0; d < matrix.numDests; ++d)
    {
        const int idx = matrix.dests[(size_t) d];
        const float v = juce::jlimit (0.0f, 1.0f, params.norm[(size_t) idx] + m.offset[(size_t) idx]);
        modDisplay[(size_t) idx].store (v, std::memory_order_relaxed);
        displayed[(size_t) numDisplayed++] = idx;
    }
    for (int e = 0; e < kNumEnvs; ++e)
        envDisplay[(size_t) e].store (globalSources[(size_t) (modsrc::Env1 + e)], std::memory_order_relaxed);
    for (int l = 0; l < kNumLfos; ++l)
        lfoDisplay[(size_t) l].store (globalSources[(size_t) (modsrc::Lfo1 + l)], std::memory_order_relaxed);
}

} // namespace vsx
