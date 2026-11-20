#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AI/PatchOps.h"

namespace vsx
{
using namespace pid;

ViceSynthProcessor::ViceSynthProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VSX", ParamRegistry::get().createLayout())
{
    const auto& reg = ParamRegistry::get();
    for (const auto& d : reg.all())
    {
        paramPtr[(size_t) d.index] = apvts.getRawParameterValue (d.id);
        params[(size_t) d.index] = apvts.getParameter (d.id);
        jassert (paramPtr[(size_t) d.index] != nullptr && params[(size_t) d.index] != nullptr);
    }
    scopeBuffer.setSize (2, 2048);
    scopeBuffer.clear();
    applyPatchInternal (PatchOps::initPatch());
    currentMeta.name = "Init";
    startTimerHz (10);
}

ViceSynthProcessor::~ViceSynthProcessor()
{
    stopTimer();
    ai.reset();
}

AIController& ViceSynthProcessor::getAI()
{
    if (ai == nullptr)
    {
        ai = std::make_unique<AIController> (*this);
        if (uiState.hasProperty ("aiConversation"))
            ai->fromVar (uiState["aiConversation"]);
    }
    return *ai;
}

bool ViceSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ViceSynthProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    engine.prepare (sr, samplesPerBlock);
    rack.prepare (sr, SynthEngine::kChunk);
    chunk.setSize (2, SynthEngine::kChunk);
    chunkMidi.ensureSize (2048);
    masterGain.setTime (0.02f, sr);
    masterGain.reset (dbToGain (getParam (G (global::MasterVolume))));
    loadMeasurer.reset (sr, samplesPerBlock);
    lastLatency = engine.latencyFor ((int) getParam (G (global::Quality)));
    setLatencySamples (lastLatency);
}

void ViceSynthProcessor::snapshotParams (ParamSnapshot& s) const
{
    for (int i = 0; i < kNumParams; ++i)
        s.value[(size_t) i] = paramPtr[(size_t) i]->load (std::memory_order_relaxed);
}

void ViceSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::AudioProcessLoadMeasurer::ScopedTimer timer (loadMeasurer, buffer.getNumSamples());
    const int total = buffer.getNumSamples();
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, total);
    if (buffer.getNumChannels() < 2 || total == 0) { buffer.clear(); midi.clear(); return; }

    keyboardState.processNextMidiBuffer (midi, 0, total, true);
    {
        const auto scope = auditionFifo.read (auditionFifo.getNumReady());
        auto addEvent = [&] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
            {
                const auto& e = auditionEvents[(size_t) i];
                midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.velocity) : juce::MidiMessage::noteOff (1, e.note), 0);
            }
        };
        addEvent (scope.startIndex1, scope.blockSize1);
        addEvent (scope.startIndex2, scope.blockSize2);
    }
    {
        const auto scope = injectFifo.read (injectFifo.getNumReady());
        auto addRaw = [&] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
                midi.addEvent (injectData[(size_t) i].data(), juce::MidiMessage::getMessageLengthFromFirstByte (injectData[(size_t) i][0]), 0);
        };
        addRaw (scope.startIndex1, scope.blockSize1);
        addRaw (scope.startIndex2, scope.blockSize2);
    }

    snapshotParams (snapshot);

    TransportInfo transport;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) transport.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { transport.ppqPosition = *ppq; transport.hasPpq = true; }
            transport.isPlaying = pos->getIsPlaying();
        }
    hostBpm.store (transport.bpm, std::memory_order_relaxed);
    hostPlaying.store (transport.isPlaying, std::memory_order_relaxed);
    const double samplesPerBeat = sampleRate * 60.0 / std::max (20.0, transport.bpm);

    float* const* chans = buffer.getArrayOfWritePointers();
    for (int start = 0; start < total; start += SynthEngine::kChunk)
    {
        const int n = std::min (SynthEngine::kChunk, total - start);
        chunkMidi.clear();
        for (const auto meta : midi)
            if (meta.samplePosition >= start && meta.samplePosition < start + n)
                chunkMidi.addEvent (meta.data, meta.numBytes, meta.samplePosition - start);

        juce::AudioBuffer<float> view (chans, 2, start, n);
        TransportInfo t = transport;
        t.ppqPosition += start / samplesPerBeat;
        engine.process (view, chunkMidi, snapshot, t);
        FxContext fc { &snapshot, &engine.getGlobalMods(), transport.bpm };
        rack.process (chans[0] + start, chans[1] + start, n, fc, patch);
    }

    // master gain, protection and metering
    masterGain.setTarget (dbToGain (snapshot.get (G (global::MasterVolume))));
    float* l = chans[0];
    float* r = chans[1];
    float pl = 0.0f, pr = 0.0f;
    bool bad = false;
    for (int i = 0; i < total; ++i)
    {
        const float g = masterGain.next();
        float a = l[i] * g, b = r[i] * g;
        if (! std::isfinite (a) || ! std::isfinite (b)) { bad = true; a = b = 0.0f; }
        // transparent below -1 dBFS, smooth ceiling just under 0 dBFS
        auto safe = [] (float x)
        {
            const float ax = std::abs (x);
            if (ax <= 0.89f) return x;
            return (x < 0.0f ? -1.0f : 1.0f) * (0.89f + 0.1f * fastTanh ((ax - 0.89f) / 0.1f));
        };
        a = safe (a); b = safe (b);
        l[i] = a; r[i] = b;
        pl = std::max (pl, std::abs (a));
        pr = std::max (pr, std::abs (b));
    }
    if (bad)
    {
        engine.reset();
        rack.reset();
        nanResets.fetch_add (1);
    }
    outputPeakL.store (std::max (pl, outputPeakL.load() * 0.9f), std::memory_order_relaxed);
    outputPeakR.store (std::max (pr, outputPeakR.load() * 0.9f), std::memory_order_relaxed);

    // oscilloscope ring buffer
    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < total; ++i)
    {
        scopeBuffer.setSample (0, w, l[i]);
        scopeBuffer.setSample (1, w, r[i]);
        w = (w + 1) & 2047;
    }
    scopeWrite.store (w, std::memory_order_relaxed);
    cpuLoad.store ((float) loadMeasurer.getLoadAsProportion(), std::memory_order_relaxed);
    midi.clear();
}

// ---------------------------------------------------------------------------------
void ViceSynthProcessor::timerCallback()
{
    // audition note-offs
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    for (auto it = pendingAuditionOffs.begin(); it != pendingAuditionOffs.end();)
    {
        if (now >= it->first)
        {
            const auto scope = auditionFifo.write (1);
            if (scope.blockSize1 > 0) auditionEvents[(size_t) scope.startIndex1] = { it->second, 0, false };
            it = pendingAuditionOffs.erase (it);
        }
        else
            ++it;
    }

    ParamSnapshot s;
    snapshotParams (s);
    rack.updateConvolution (s, patch.userIrPath);

    const int lat = engine.latencyFor (s.geti (G (global::Quality)));
    if (lat != lastLatency)
    {
        lastLatency = lat;
        setLatencySamples (lat);
    }

    if (++recoveryCounter % 10 == 0)
        patch.collectGarbage();
    if (recoveryCounter % 300 == 0) // every 30 s: crash-recovery snapshot when the sound changed
    {
        auto spec = getCurrentPatch();
        auto json = spec.toJson();
        if (json != lastRecoveryJson)
        {
            lastRecoveryJson = json;
            presets.writeRecovery (spec);
        }
    }
}

void ViceSynthProcessor::audition (int note, int velocity, double seconds)
{
    const auto scope = auditionFifo.write (1);
    if (scope.blockSize1 > 0)
    {
        auditionEvents[(size_t) scope.startIndex1] = { note, velocity, true };
        pendingAuditionOffs.push_back ({ juce::Time::getMillisecondCounterHiRes() * 0.001 + seconds, note });
    }
}

void ViceSynthProcessor::injectMidi (const juce::MidiMessage& m)
{
    if (m.getRawDataSize() > 3) return;
    const auto scope = injectFifo.write (1);
    if (scope.blockSize1 > 0)
    {
        auto& d = injectData[(size_t) scope.startIndex1];
        d = { 0, 0, 0 };
        std::memcpy (d.data(), m.getRawData(), (size_t) m.getRawDataSize());
    }
}

void ViceSynthProcessor::setParam (int index, float realValue)
{
    auto* p = params[(size_t) index];
    p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

// ---------------------------------------------------------------------------------
PatchSpec ViceSynthProcessor::getCurrentPatch()
{
    PatchSpec spec;
    for (int i = 0; i < kNumParams; ++i)
        spec.values[(size_t) i] = paramPtr[(size_t) i]->load();
    auto v = patch.toVar();
    if (auto* oscs = v["oscillators"].getArray())
        for (auto& o : *oscs)
            if (auto* obj = o.getDynamicObject()) obj->removeProperty ("wavetableData");
    spec.stateFromVar (v);
    for (int o = 0; o < kNumOscs; ++o)
    {
        const auto& id = patch.osc[(size_t) o].wavetableId;
        spec.wavetableId[(size_t) o] = id;
        if (id.startsWith ("custom:"))
            spec.customWavetable[(size_t) o] = patch.osc[(size_t) o].wavetable.getShared();
    }
    spec.meta = currentMeta;
    return spec;
}

void ViceSynthProcessor::applyPatchInternal (const PatchSpec& spec)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        const float v = spec.values[(size_t) i];
        if (std::abs (paramPtr[(size_t) i]->load() - v) > 1.0e-7f)
            params[(size_t) i]->setValueNotifyingHost (params[(size_t) i]->convertTo0to1 (v));
    }
    patch.fromVar (spec.stateToVar(), true);
    currentMeta = spec.meta;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void ViceSynthProcessor::pushUndo (const juce::String& label)
{
    undoStack.push_back ({ getCurrentPatch(), label });
    if (undoStack.size() > 50) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void ViceSynthProcessor::applyPatch (const PatchSpec& spec, const juce::String& label)
{
    pushUndo (label);
    applyPatchInternal (spec);
}

bool ViceSynthProcessor::undo()
{
    if (undoStack.empty()) return false;
    redoStack.push_back ({ getCurrentPatch(), undoStack.back().second });
    const auto spec = undoStack.back().first;
    undoStack.pop_back();
    applyPatchInternal (spec);
    return true;
}

bool ViceSynthProcessor::redo()
{
    if (redoStack.empty()) return false;
    undoStack.push_back ({ getCurrentPatch(), redoStack.back().second });
    const auto spec = redoStack.back().first;
    redoStack.pop_back();
    applyPatchInternal (spec);
    return true;
}

void ViceSynthProcessor::loadPresetEntry (const PresetManager::Entry& e)
{
    PatchSpec spec;
    juce::String err;
    if (presets.load (e, spec, err))
        applyPatch (spec, "Load " + e.name);
}

void ViceSynthProcessor::stepPreset (int delta)
{
    const auto& entries = presets.getEntries();
    if (entries.empty()) return;
    int idx = presets.indexOfName (currentMeta.name);
    idx = idx < 0 ? 0 : (idx + delta + (int) entries.size()) % (int) entries.size();
    loadPresetEntry (entries[(size_t) idx]);
}

// ---------------------------------------------------------------------------------
int ViceSynthProcessor::addModulation (int source, int destParam, float amount)
{
    if (source <= modsrc::None || source >= modsrc::Count || ! juce::isPositiveAndBelow (destParam, kNumParams)
        || ! ParamRegistry::get()[destParam].modulatable)
        return -1;
    int slot = -1;
    for (int s = 0; s < kNumModSlots && slot < 0; ++s)
        if (patch.slots[(size_t) s].source.load() == source && patch.slots[(size_t) s].dest.load() == destParam) slot = s;
    for (int s = 0; s < kNumModSlots && slot < 0; ++s)
        if (! patch.slots[(size_t) s].active()) slot = s;
    if (slot < 0) return -1;
    auto& st = patch.slots[(size_t) slot];
    st.dest = destParam;
    st.source = source;
    setParam (MODAMT (slot), juce::jlimit (-1.0f, 1.0f, amount));
    return slot;
}

void ViceSynthProcessor::removeModulation (int slot) { clearModulationSlot (slot); }

void ViceSynthProcessor::clearModulationSlot (int slot)
{
    if (! juce::isPositiveAndBelow (slot, kNumModSlots)) return;
    auto& st = patch.slots[(size_t) slot];
    st.source = modsrc::None;
    st.dest = -1;
    st.aux = modsrc::None;
    st.polarity = ModSlotState::Native;
    st.curve = 0.0f;
    setParam (MODAMT (slot), 0.0f);
}

juce::Array<int> ViceSynthProcessor::slotsTargeting (int destParam) const
{
    juce::Array<int> r;
    for (int s = 0; s < kNumModSlots; ++s)
        if (patch.slots[(size_t) s].source.load() != modsrc::None && patch.slots[(size_t) s].dest.load() == destParam) r.add (s);
    return r;
}

// ---------------------------------------------------------------------------------
void ViceSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "VICE-SYNTH-X-STATE");
    root->setProperty ("version", 1);
    root->setProperty ("preset", getCurrentPatch().toPresetVar());
    juce::var ui = uiState.isObject() ? uiState : juce::var (new juce::DynamicObject());
    if (ai != nullptr)
        if (auto* o = ui.getDynamicObject()) o->setProperty ("aiConversation", ai->toVar());
    root->setProperty ("ui", ui);
    const auto json = juce::JSON::toString (juce::var (root), true);
    destData.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void ViceSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto json = juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes);
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject() || v["format"].toString() != "VICE-SYNTH-X-STATE")
        return; // keep the current state on corrupt or foreign data
    PatchSpec spec;
    juce::String err;
    if (PatchSpec::fromPresetVar (v["preset"], spec, err))
        applyPatchInternal (spec);
    uiState = v["ui"];
    if (ai != nullptr && uiState.hasProperty ("aiConversation"))
        ai->fromVar (uiState["aiConversation"]);
}

juce::AudioProcessorEditor* ViceSynthProcessor::createEditor()
{
    return new ViceSynthEditor (*this);
}

} // namespace vsx

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vsx::ViceSynthProcessor();
}
