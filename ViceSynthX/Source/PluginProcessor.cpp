#include "PluginProcessor.h"
#include "Analysis/TruePeak.h"
#include "Presets/SoundRecipes.h"
#include "PluginEditor.h"
#include "AI/PatchOps.h"
#include "Analysis/SoundMatcher.h"

namespace vsx
{
static float masterOutToGain (float db) noexcept { return db <= -69.9f ? 0.0f : dbToGain (db); }

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
    aliveToken.reset();                    // pending AUTO GAIN results are dropped
    importPool.removeAllJobs (true, 10000);
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

bool ViceSynthProcessor::waitForImports (int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (juce::Time::getMillisecondCounter() < end)
    {
        bool loading = false;
        for (int o = 0; o < kNumOscs; ++o) loading = loading || getImportStatus (o).loading;
        if (deliverPendingImports() == 0 && ! loading)
        {
            patch.waitForLoads (std::max (1, (int) (end - juce::Time::getMillisecondCounter())));
            return true;
        }
        juce::Thread::sleep (5);
    }
    return false;
}

float ViceSynthProcessor::fmReleaseSeconds (const fm::Patch& v) { return fm::releaseSeconds (v); }

void ViceSynthProcessor::loadFmVoice (int o, const fm::Patch& voice, bool asNewPatch)
{
    if (! juce::isPositiveAndBelow (o, kNumOscs)) return;
    auto v = voice;
    v.clamp();
    PatchSpec spec;
    if (asNewPatch)
    {
        spec = Recipes::init (v.getName().isNotEmpty() ? v.getName() : juce::String ("FM Voice"), "FM");
        spec.meta.author = {};
        spec.meta.source = "user";
        Recipes::filterOff (spec, 0);
        Recipes::filterOff (spec, 1);
        o = 0;
    }
    else
        spec = getCurrentPatch();
    const auto id = Recipes::oscId (o);
    spec.p (id + "_on", 1.0f).p (id + "_mode", (float) (int) OscMode::FM);
    spec.fmVoice[(size_t) o] = v.toHex();
    if (asNewPatch)
    {
        // the voice's own operator envelopes shape the sound; the amp envelope only gates it
        Recipes::env (spec, 0, 0.001f, 0.5f, 1.0f, fmReleaseSeconds (v));
        spec.p (id + "_level", 0.8f);
    }
    applyPatch (spec, "Load FM voice " + v.getName());
}

float ViceSynthProcessor::autoGainDelta (float singleRmsDb, float singlePeakDb, float chordPeakDb)
{
    return std::min ({ -20.0f - singleRmsDb, -6.0f - singlePeakDb, -1.5f - chordPeakDb });
}

void ViceSynthProcessor::autoGain (std::function<void (float, juce::String)> done)
{
    const auto spec = getCurrentPatch();
    const float currentDb = getParam (G (global::MasterVolume));
    importPool.addJob ([this, spec, currentDb, done, alive = std::weak_ptr<int> (aliveToken)]
    {
        auto renderer = std::make_unique<OfflineRenderer> (48000.0);
        const int root = spec.meta.category == "Bass" ? 36 : 48;
        const double secs = std::max (1.5, (double) spec.get ("env1_attack") + 1.0);
        const auto single = renderer->render (spec, root + 12, 100, secs, 0.5, 1);
        const auto chord = renderer->renderNotes (spec, { root, root + 4, root + 7, root + 11, root + 14, root + 19 }, 127, secs, 0.5, 1);
        const int win = (int) (0.4 * 48000.0), hop = win / 4;
        double best = 0.0;
        for (int st = 0; st + win <= single.getNumSamples(); st += hop)
        {
            double sq = 0.0;
            for (int c = 0; c < 2; ++c)
                for (int i = st; i < st + win; ++i) sq += (double) single.getSample (c, i) * single.getSample (c, i);
            best = std::max (best, sq / (2.0 * win));
        }
        const float rms = (float) (10.0 * std::log10 (best + 1.0e-12));
        const float pk = juce::Decibels::gainToDecibels (single.getMagnitude (0, single.getNumSamples()), -150.0f);
        const float cpk = TruePeak().measureDb (chord); // inter-sample (true) peak, as in the library calibration
        const float newDb = juce::jlimit (-60.0f, 12.0f, currentDb + (rms < -100.0f ? 0.0f : autoGainDelta (rms, pk, cpk)));
        const auto report = "single note " + juce::String (rms, 1) + " dBFS RMS / " + juce::String (pk, 1) + " dBFS peak, six-note chord "
                            + juce::String (cpk, 1) + " dBTP true peak -> Preset Volume " + juce::String (newDb, 1) + " dB";
        juce::MessageManager::callAsync ([this, alive, newDb, report, done]
        {
            if (alive.expired()) return;
            pushUndo ("Auto gain");
            setParam (G (global::MasterVolume), newDb);
            if (done) done (newDb, report);
        });
    });
}

void ViceSynthProcessor::loadInitPatch()
{
    auto spec = Recipes::init ("Init", "Init");
    spec.meta.author = {};
    spec.meta.source = "user";
    applyPatch (spec, "Init patch");
}

bool ViceSynthProcessor::importSampleFiles (int oscillator, const juce::StringArray& files)
{
    if (! juce::isPositiveAndBelow (oscillator, kNumOscs) || files.isEmpty()) return false;
    if (files.size() == 1) return importSampleFile (oscillator, juce::File (files[0]));
    // several audio files: one multisample whose zones come from the note names in the file names
    juce::Array<juce::var> paths;
    for (const auto& path : files)
    {
        if (! isAudioFile (juce::File (path)))
        {
            setImportStatus (oscillator, { "Multi-file drop accepts WAV / AIFF / FLAC files only: " + juce::File (path).getFileName(), false, true });
            return false;
        }
        paths.add (path);
    }
    startImport (oscillator, "files:" + juce::JSON::toString (juce::var (paths), true), ImportKind::Multisample,
                 juce::String (files.size()) + " files", {});
    return true;
}

bool ViceSynthProcessor::importSampleFile (int oscillator, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (oscillator, kNumOscs) || ! isImportable (file))
    {
        if (juce::isPositiveAndBelow (oscillator, kNumOscs))
            setImportStatus (oscillator, { "Unsupported sample format. Use WAV, AIFF, FLAC, SFZ or a folder.", false, true });
        return false;
    }
    importToOscillator (oscillator, file);
    return true;
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
    masterOutGain.reset (sr, 0.03);
    masterOutGain.setCurrentAndTargetValue (masterOutToGain (getParam (G (global::MasterOut))));
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

    // Preset change or PANIC: stop every voice, the arpeggiator / sequencer, sustain, effect tails and feedback
    // buffers and drop pending injected MIDI. Only notes started after this point sound.
    if (silenceRequest.exchange (false))
    {
        engine.reset();
        rack.reset();
        const auto pending = injectFifo.read (injectFifo.getNumReady());
        juce::ignoreUnused (pending);
        fadeRemaining = 64; // fade from the last output sample to zero: no click
    }
    keyboardState.processNextMidiBuffer (midi, 0, total, true);
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

    snapshotParams (rawSnapshot);
    snapshot.value = rawSnapshot.value;

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
        snapshot.value = rawSnapshot.value; // the engine writes macro-applied values into the snapshot
        engine.process (view, chunkMidi, snapshot, t);
        FxContext fc { &snapshot, &engine.getGlobalMods(), transport.bpm };
        rack.process (chans[0] + start, chans[1] + start, n, fc, patch);
    }

    // master gain, protection and metering
    masterGain.setTarget (dbToGain (snapshot.get (G (global::MasterVolume))));
    masterOutGain.setTargetValue (masterOutToGain (snapshot.get (G (global::MasterOut))));
    bool clipped = false;
    float* l = chans[0];
    float* r = chans[1];
    float pl = 0.0f, pr = 0.0f;
    bool bad = false;
    for (int i = 0; i < total; ++i)
    {
        const float g = masterGain.next() * masterOutGain.getNextValue();
        float a = l[i] * g, b = r[i] * g;
        if (fadeRemaining > 0)
        {
            const float f = (float) fadeRemaining / 64.0f;
            a += lastOutL * f;
            b += lastOutR * f;
            --fadeRemaining;
        }
        if (! std::isfinite (a) || ! std::isfinite (b)) { bad = true; a = b = 0.0f; }
        // transparent below -1 dBFS, smooth ceiling just under 0 dBFS
        auto safe = [] (float x)
        {
            const float ax = std::abs (x);
            if (ax <= 0.89f) return x;
            return (x < 0.0f ? -1.0f : 1.0f) * (0.89f + 0.1f * fastTanh ((ax - 0.89f) / 0.1f));
        };
        clipped = clipped || std::abs (a) > 0.89f || std::abs (b) > 0.89f;
        a = safe (a); b = safe (b);
        l[i] = a; r[i] = b;
        pl = std::max (pl, std::abs (a));
        pr = std::max (pr, std::abs (b));
    }
    if (clipped) clipLatched.store (true);
    if (total > 0 && fadeRemaining == 0) { lastOutL = l[total - 1]; lastOutR = r[total - 1]; }
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

void ViceSynthProcessor::panic()
{
    hold = false;
    keyboardState.allNotesOff (0);
    injectMidi (juce::MidiMessage::controllerEvent (1, 64, 0));
    injectMidi (juce::MidiMessage::allNotesOff (1));
    injectMidi (juce::MidiMessage::allSoundOff (1));
    silenceRequest.store (true); // also clears effect tails, arp / sequencer and pending events
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
    if (! committingImport) ++externalPatchSerial; // invalidates imports that started before this change
    // a new sound starts from silence: nothing from the previous preset keeps sounding
    silenceRequest.store (true);
    keyboardState.allNotesOff (0);
    for (int i = 0; i < kNumParams; ++i)
    {
        if (pid::isSessionParam (i)) continue; // MASTER VOL and GLOBAL FX BYPASS are never changed by presets
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
    // session parameters are not part of a preset but belong to the project
    auto* session = new juce::DynamicObject();
    for (int i = 0; i < kNumParams; ++i)
        if (pid::isSessionParam (i)) session->setProperty (ParamRegistry::get()[i].id, getParam (i));
    root->setProperty ("session", juce::var (session));
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
    if (auto* session = v["session"].getDynamicObject())
        for (int i = 0; i < kNumParams; ++i)
            if (pid::isSessionParam (i) && session->hasProperty (ParamRegistry::get()[i].id))
                setParam (i, (float) session->getProperty (ParamRegistry::get()[i].id));
    uiState = v["ui"];
    if (ai != nullptr && uiState.hasProperty ("aiConversation"))
        ai->fromVar (uiState["aiConversation"]);
}

juce::AudioProcessorEditor* ViceSynthProcessor::createEditor()
{
    return new ViceSynthEditor (*this);
}

// =================================================================================
// Sample / multisample / wavetable import
// =================================================================================
bool ViceSynthProcessor::isAudioFile (const juce::File& f)
{
    return f.existsAsFile() && f.hasFileExtension ("wav;wave;aif;aiff;flac");
}

bool ViceSynthProcessor::isImportable (const juce::File& f)
{
    return isAudioFile (f) || (f.existsAsFile() && f.hasFileExtension ("sfz")) || f.isDirectory();
}

ViceSynthProcessor::ImportStatus ViceSynthProcessor::getImportStatus (int o) const
{
    return juce::isPositiveAndBelow (o, kNumOscs) ? importStatus[(size_t) o] : ImportStatus();
}

void ViceSynthProcessor::setImportStatus (int o, ImportStatus st)
{
    importStatus[(size_t) o] = std::move (st);
    if (onImportStatusChanged) onImportStatusChanged (o);
}

void ViceSynthProcessor::importToOscillator (int o, const juce::File& file, ImportKind kind, ImportCallback done)
{
    auto fail = [&] (const juce::String& msg)
    {
        if (juce::isPositiveAndBelow (o, kNumOscs)) setImportStatus (o, { msg, false, true });
        if (done) done (false, msg);
    };
    if (! juce::isPositiveAndBelow (o, kNumOscs)) { if (done) done (false, "Invalid oscillator."); return; }
    if (kind == ImportKind::Auto)
    {
        if (file.isDirectory() || file.hasFileExtension ("sfz")) kind = ImportKind::Multisample;
        else if (isAudioFile (file)) kind = ImportKind::Sample;
        else { fail ("Unsupported file \"" + file.getFileName() + "\": use WAV, AIFF or FLAC audio, an .sfz file or a folder of samples."); return; }
    }
    if ((kind == ImportKind::Sample || kind == ImportKind::Wavetable) && ! isAudioFile (file))
    {
        fail (file.existsAsFile() ? "Unsupported audio format: " + file.getFileName() + " (WAV, AIFF or FLAC)." : "File not found: " + file.getFullPathName());
        return;
    }
    juce::String sourceId;
    switch (kind)
    {
        case ImportKind::Sample:      sourceId = "file:" + file.getFullPathName(); break;
        case ImportKind::Wavetable:   sourceId = "user:" + file.getFullPathName(); break;
        case ImportKind::Multisample: sourceId = juce::String (file.isDirectory() ? "folder:" : "sfz:") + file.getFullPathName(); break;
        case ImportKind::Auto: break;
    }

    startImport (o, sourceId, kind, file.getFileName(), std::move (done));
}

void ViceSynthProcessor::startImport (int o, const juce::String& sourceId, ImportKind kind, const juce::String& displayName, ImportCallback done)
{
    ImportResult job;
    job.osc = o;
    job.gen = ++importGen[(size_t) o];
    job.serial = externalPatchSerial;
    job.kind = kind;
    job.sourceId = sourceId;
    job.name = displayName;
    job.done = std::move (done);
    setImportStatus (o, { "Loading " + displayName + "...", true, false });
    ++pendingImports;
    importPool.addJob ([this, job]() mutable
    {
        juce::String err;
        if (job.kind == ImportKind::Sample)
        {
            auto s = SampleLibrary::instance().loadSample (job.sourceId, err);
            job.ok = s != nullptr;
            if (job.ok)
                job.info = juce::String (s->numChannels > 1 ? "stereo" : "mono") + ", " + juce::String (s->length / s->sampleRate, 2) + " s"
                         + (s->truncated ? ", first " + juce::String ((int) SampleData::kMaxSeconds) + " s loaded" : juce::String())
                         + (s->rootFromFile ? ", file root " + juce::MidiMessage::getMidiNoteName (s->rootNote, true, true, 4) : juce::String());
        }
        else if (job.kind == ImportKind::Multisample)
        {
            auto m = SampleLibrary::instance().loadMulti (job.sourceId, err);
            job.ok = m != nullptr && ! m->zones.empty();
            if (job.ok) job.info = juce::String ((int) m->zones.size()) + " zones";
            if (m != nullptr && m->zones.empty() && err.isEmpty()) err = "No playable zones found in " + job.name;
        }
        else
        {
            auto wt = WavetableLibrary::instance().load (job.sourceId, err);
            job.ok = wt != nullptr;
            if (job.ok) job.info = juce::String (wt->numFrames) + " frames";
        }
        if (! job.ok && err.isEmpty()) err = "Could not load " + job.name;
        job.error = err;
        importQueue.push (std::move (job));
    });
}

int ViceSynthProcessor::deliverPendingImports()
{
    for (auto& r : importQueue.take())
    {
        --pendingImports;
        commitImport (std::move (r));
    }
    return pendingImports.load();
}

void ViceSynthProcessor::commitImport (ImportResult r)
{
    const int o = r.osc;
    if (r.gen != importGen[(size_t) o]) return; // superseded by a newer import on this oscillator
    if (r.serial != externalPatchSerial)
    {
        const auto msg = "Import of " + r.name + " discarded: the patch changed while it was loading.";
        setImportStatus (o, { msg, false, true });
        if (r.done) r.done (false, msg);
        return;
    }
    if (! r.ok)
    {
        setImportStatus (o, { r.error, false, true });
        if (r.done) r.done (false, r.error);
        return;
    }
    using namespace pid;
    auto spec = getCurrentPatch();
    auto setP = [&spec] (int idx, float v) { spec.values[(size_t) idx] = v; };
    setP (O (o, osc::On), 1.0f);
    if (r.kind == ImportKind::Sample)
    {
        spec.sampleId[(size_t) o] = r.sourceId;
        setP (O (o, osc::Mode), 2.0f);   // Sample: one file pitched chromatically over the whole keyboard
        setP (O (o, osc::Source), 1.0f);
        // a new file plays from its beginning to its end (Position is the sample start in Sample mode)
        setP (O (o, osc::Position), 0.0f);
        setP (O (o, osc::SampleEnd), 1.0f);
    }
    else if (r.kind == ImportKind::Multisample)
    {
        spec.multiId[(size_t) o] = r.sourceId;
        setP (O (o, osc::Mode), 3.0f);
        setP (O (o, osc::Position), 0.0f);
        setP (O (o, osc::SampleEnd), 1.0f);
    }
    else
    {
        spec.wavetableId[(size_t) o] = r.sourceId;
        setP (O (o, osc::Mode), 0.0f);
    }
    committingImport = true;
    applyPatch (spec, "Load " + r.name);
    committingImport = false;
    juce::String text = r.name + " - " + r.info;
    if (r.kind == ImportKind::Sample)
        text << " - root " << juce::MidiMessage::getMidiNoteName ((int) getParam (O (o, osc::RootNote)), true, true, 4)
             << (getParam (O (o, osc::RootAuto)) > 0.5f ? " (Auto)" : "");
    setImportStatus (o, { text, false, false });
    if (r.done) r.done (true, text);
}

} // namespace vsx

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vsx::ViceSynthProcessor();
}
