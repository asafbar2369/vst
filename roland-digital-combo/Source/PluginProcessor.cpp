#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int kBankBytes = d50::kNumPatches * d50::kTempSize;

const juce::StringArray& nonPresetParams()
{
    static const juce::StringArray ids { params::id::sysexRx, params::id::sysexChannel, params::id::masterVolume };
    return ids;
}
} // namespace

DigitalComboProcessor::DigitalComboProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DigitalCombo", params::createLayout())
{
    for (auto& d : dirtyBits) d.store (0);
    for (auto& v : userBankValid) v.store (false);

    // User bank slots start as our init tone so partial DT1 writes into Patch Memory stay playable.
    uint8_t init[d50::kTempSize];
    params::writeInitD50Tone (init);
    userBank = std::make_unique<std::atomic<uint8_t>[]> ((size_t) kBankBytes);
    for (int s = 0; s < d50::kNumPatches; ++s)
        for (int i = 0; i < d50::kTempSize; ++i)
            userBank[(size_t) (s * d50::kTempSize + i)].store (init[i]);

    resetD50TempToInit();

    for (auto& b : params::d50Bindings())
    {
        auto* p = apvts.getParameter (b.id);
        auto* v = apvts.getRawParameterValue (b.id);
        jassert (p != nullptr && v != nullptr);
        bound.push_back ({ p, v, b.tempOffset, -1 });
    }

    using namespace params::id;
    pMode = apvts.getRawParameterValue (mode);
    pSplit = apvts.getRawParameterValue (splitPoint);
    pSwap = apvts.getRawParameterValue (splitSwap);
    pBalance = apvts.getRawParameterValue (layerBalance);
    pVolume = apvts.getRawParameterValue (masterVolume);
    pSysexRx = apvts.getRawParameterValue (sysexRx);
    pSysexCh = apvts.getRawParameterValue (sysexChannel);
    for (int t = 0; t < 4; ++t)
        for (auto& s : params::jdToneSuffixes())
            jdTone[t].push_back (apvts.getRawParameterValue (params::jdToneId (t, s)));
    const juce::String jdIds[11] = { jdLfoRate, jdLfoDepth, jdBend, jdChorusMix, jdChorusRate, jdDelayTime,
                                     jdDelayFb, jdDelayMix, jdRevSize, jdRevMix, jdVolume };
    for (int i = 0; i < 11; ++i)
        pJd[i] = apvts.getRawParameterValue (jdIds[i]);

    startTimerHz (20);
}

DigitalComboProcessor::~DigitalComboProcessor()
{
    stopTimer();
}

bool DigitalComboProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() > 0 && ! layouts.getMainInputChannelSet().isDisabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void DigitalComboProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int block = juce::jmax (64, samplesPerBlock);
    d50.prepare (sampleRate, block);
    jd.prepare (sampleRate, block);
    d50Buf.setSize (2, block, false, true, false);
    jdBuf.setSize (2, block, false, true, false);
    replyBuffer.ensureSize (8192);
    lastMode = -1;
    forceParamsToTemp = true;
}

void DigitalComboProcessor::resetD50TempToInit() noexcept
{
    uint8_t init[d50::kTempSize];
    params::writeInitD50Tone (init);
    for (int i = 0; i < d50::kTempSize; ++i)
        d50Temp[(size_t) i].store (init[i]);
    for (auto& d : dirtyBits) d.store (0);
}

void DigitalComboProcessor::writeD50Byte (uint32_t linear, uint8_t value) noexcept
{
    value &= 0x7F;
    if (linear < (uint32_t) d50::kTempSize)
    {
        const uint8_t v = std::min (value, d50::maxValueForTempOffset ((int) linear));
        d50Temp[linear].store (v);
        dirtyBits[linear / 64].fetch_or (uint64_t (1) << (linear % 64));
        return;
    }
    if (linear >= d50::kPatchMemoryBase && linear < d50::kReverbBase)
    {
        const uint32_t rel = linear - d50::kPatchMemoryBase;
        const int slot = int (rel / (uint32_t) d50::kPatchStride);
        const int off = int (rel % (uint32_t) d50::kPatchStride);
        userBank[rel].store (std::min (value, d50::maxValueForTempOffset (off)));
        userBankValid[(size_t) slot].store (true);
    }
    // Reverb data 17-32 and addresses beyond the memory area are ignored (doc note *8-1).
}

void DigitalComboProcessor::applyParamsToD50Temp() noexcept
{
    const bool force = forceParamsToTemp.exchange (false);
    for (auto& b : bound)
    {
        const int v = (int) std::lround (b.value->load (std::memory_order_relaxed));
        if (force || v != b.lastSeen)
        {
            d50Temp[(size_t) b.offset].store ((uint8_t) juce::jlimit (0, 127, v));
            b.lastSeen = v;
        }
    }
}

void DigitalComboProcessor::decodeD50() noexcept
{
    uint8_t raw[d50::kTempSize];
    for (int i = 0; i < d50::kTempSize; ++i)
        raw[i] = d50Temp[(size_t) i].load (std::memory_order_relaxed);
    d50.setParams (dc::D50Params::decode (raw));
}

dc::JdParams DigitalComboProcessor::readJdParams() const noexcept
{
    dc::JdParams p;
    for (int t = 0; t < 4; ++t)
    {
        const auto& v = jdTone[t];
        auto& tp = p.tone[t];
        int i = 0;
        tp.on = v[(size_t) i++]->load() > 0.5f;
        tp.wave = juce::jlimit (0, (int) dc::JdWave::Count - 1, (int) v[(size_t) i++]->load());
        tp.coarse = v[(size_t) i++]->load();
        tp.fineCents = v[(size_t) i++]->load();
        tp.level = v[(size_t) i++]->load() / 100.0f;
        tp.pan = v[(size_t) i++]->load() / 50.0f;
        tp.filterMode = juce::jlimit (0, 2, (int) v[(size_t) i++]->load());
        tp.cutoff = v[(size_t) i++]->load();
        tp.resonance = v[(size_t) i++]->load();
        tp.envDepth = v[(size_t) i++]->load() / 50.0f;
        tp.fA = v[(size_t) i++]->load(); tp.fD = v[(size_t) i++]->load();
        tp.fS = v[(size_t) i++]->load(); tp.fR = v[(size_t) i++]->load();
        tp.aA = v[(size_t) i++]->load(); tp.aD = v[(size_t) i++]->load();
        tp.aS = v[(size_t) i++]->load(); tp.aR = v[(size_t) i++]->load();
        tp.velSens = v[(size_t) i++]->load() / 100.0f;
    }
    p.lfoRate = pJd[0]->load();
    p.lfoPitchDepth = pJd[1]->load() / 100.0f;
    p.bendRange = (int) pJd[2]->load();
    p.chorusMix = pJd[3]->load() / 100.0f;
    p.chorusRate = pJd[4]->load();
    p.delayTimeMs = pJd[5]->load();
    p.delayFeedback = pJd[6]->load() / 100.0f;
    p.delayMix = pJd[7]->load() / 100.0f;
    p.reverbSize = pJd[8]->load() / 100.0f;
    p.reverbMix = pJd[9]->load() / 100.0f;
    p.volume = pJd[10]->load() / 100.0f;
    return p;
}

void DigitalComboProcessor::routeNoteOn (int note, float velocity, int mode) noexcept
{
    switch (mode)
    {
        case params::ModeJD800: jd.noteOn (note, velocity); break;
        case params::ModeD50:   d50.noteOn (note, velocity); break;
        case params::ModeLayer: d50.noteOn (note, velocity); jd.noteOn (note, velocity); break;
        default:
        {
            const bool low = note < (int) pSplit->load();
            const bool jdLower = pSwap->load() > 0.5f;
            if (low != jdLower) d50.noteOn (note, velocity);
            else jd.noteOn (note, velocity);
            break;
        }
    }
}

void DigitalComboProcessor::handleMidi (const juce::MidiMessage& m, juce::MidiBuffer& out, int samplePos, int mode) noexcept
{
    juce::ignoreUnused (out, samplePos);
    if (m.isNoteOn())
        routeNoteOn (m.getNoteNumber(), m.getFloatVelocity(), mode);
    else if (m.isNoteOff())
    {
        d50.noteOff (m.getNoteNumber());
        jd.noteOff (m.getNoteNumber());
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        if (cc == 1) { d50.setModWheel ((float) val / 127.0f); jd.setModWheel ((float) val / 127.0f); }
        else if (cc == 64) { d50.setSustain (val >= 64); jd.setSustain (val >= 64); }
        else if (cc == 120) { d50.allSoundOff(); jd.allSoundOff(); }
        else if (cc == 121) { d50.setPitchBend (0); jd.setPitchBend (0); d50.setModWheel (0); jd.setModWheel (0); }
        else if (cc >= 123) { d50.allNotesOff(); jd.allNotesOff(); } // doc *2-10: 123-127 act as All Notes Off
    }
    else if (m.isPitchWheel())
    {
        const float b = juce::jlimit (-1.0f, 1.0f, float (m.getPitchWheelValue() - 8192) / 8192.0f);
        d50.setPitchBend (b);
        jd.setPitchBend (b);
    }
    else if (m.isChannelPressure())
        d50.setAftertouch ((float) m.getChannelPressureValue() / 127.0f);
    else if (m.isProgramChange())
    {
        // 0-63 = internal memory group on the D-50; we load a received user-bank slot if present.
        const int prog = m.getProgramChangeNumber();
        if (prog < d50::kNumPatches && userBankValid[(size_t) prog].load())
        {
            for (int i = 0; i < d50::kTempSize; ++i)
                writeD50Byte ((uint32_t) i, userBank[(size_t) (prog * d50::kTempSize + i)].load());
            decodeD50();
        }
    }
}

void DigitalComboProcessor::handleSysEx (const uint8_t* data, int size, juce::MidiBuffer* replies, int samplePos) noexcept
{
    d50::Message msg;
    const auto r = d50::parse (data, (size_t) juce::jmax (0, size), msg);
    if (r == d50::ParseResult::NotRoland || r == d50::ParseResult::WrongModel || r == d50::ParseResult::UnsupportedCommand)
    {
        ++sysexStats.ignored;
        return;
    }
    if (r != d50::ParseResult::Ok)
    {
        ++sysexStats.rejected;
        return;
    }

    const bool fromFile = replies == nullptr;
    const uint8_t device = (uint8_t) (juce::jlimit (1, 16, (int) std::lround (pSysexCh->load())) - 1);
    if (! fromFile && (msg.device != device || pSysexRx->load() < 0.5f))
    {
        ++sysexStats.ignored;
        return;
    }
    sysexStats.lastCommand = msg.command;
    sysexStats.lastAddress = msg.address;

    switch (msg.command)
    {
        case d50::kDT1:
            for (int k = 0; k < msg.dataLength; ++k)
                writeD50Byte (msg.address + (uint32_t) k, msg.data[k]);
            ++sysexStats.accepted;
            break;

        case d50::kRQ1:
        {
            const uint32_t end = msg.address + msg.size;
            const bool inTemp = msg.size > 0 && end <= (uint32_t) d50::kTempSize;
            const bool inBank = msg.size > 0 && msg.address >= d50::kPatchMemoryBase && end <= d50::kReverbBase;
            if (! inTemp && ! inBank)
            {
                ++sysexStats.ignored;
                break;
            }
            if (replies != nullptr)
            {
                uint8_t chunk[d50::kMaxDataBytesPerMessage];
                uint8_t bytes[d50::kMaxDataBytesPerMessage + 12];
                for (uint32_t off = 0; off < msg.size; off += (uint32_t) d50::kMaxDataBytesPerMessage)
                {
                    const int n = (int) std::min<uint32_t> ((uint32_t) d50::kMaxDataBytesPerMessage, msg.size - off);
                    for (int k = 0; k < n; ++k)
                    {
                        const uint32_t a = msg.address + off + (uint32_t) k;
                        chunk[k] = inTemp ? d50Temp[a].load() : userBank[a - d50::kPatchMemoryBase].load();
                    }
                    const auto len = d50::buildDT1 (device, msg.address + off, chunk, n, bytes);
                    replies->addEvent (bytes, (int) len, samplePos);
                    ++sysexStats.replies;
                }
            }
            ++sysexStats.accepted;
            break;
        }

        case d50::kWSD:
        case d50::kRQD:
            // Handshake transfers are not implemented: answer with Rejection (RJC), as the
            // document allows when the device is not ready for communication.
            if (replies != nullptr)
            {
                const uint8_t rjc[] = { d50::kExclusiveStatus, d50::kRolandId, device, d50::kModelId, d50::kRJC, d50::kEndOfExclusive };
                replies->addEvent (rjc, (int) sizeof (rjc), samplePos);
                ++sysexStats.replies;
            }
            ++sysexStats.ignored;
            break;

        default:
            ++sysexStats.ignored;
            break;
    }
}

void DigitalComboProcessor::renderSegment (juce::AudioBuffer<float>& buffer, int start, int end, int mode) noexcept
{
    const int numOut = buffer.getNumChannels();
    float gD = 1.0f, gJ = 1.0f;
    if (mode == params::ModeLayer)
    {
        const float b = pBalance->load();
        gD = std::min (1.0f, (100.0f - b) / 50.0f);
        gJ = std::min (1.0f, b / 50.0f);
    }
    int pos = start;
    while (pos < end)
    {
        const int len = std::min (end - pos, d50Buf.getNumSamples());
        d50Buf.clear (0, len);
        jdBuf.clear (0, len);
        d50.render (d50Buf.getWritePointer (0), d50Buf.getWritePointer (1), len);
        jd.render (jdBuf.getWritePointer (0), jdBuf.getWritePointer (1), len);
        const float* dl = d50Buf.getReadPointer (0); const float* dr = d50Buf.getReadPointer (1);
        const float* jl = jdBuf.getReadPointer (0);  const float* jr = jdBuf.getReadPointer (1);
        if (numOut >= 2)
        {
            float* L = buffer.getWritePointer (0, pos);
            float* R = buffer.getWritePointer (1, pos);
            for (int i = 0; i < len; ++i)
            {
                L[i] += gD * dl[i] + gJ * jl[i];
                R[i] += gD * dr[i] + gJ * jr[i];
            }
        }
        else if (numOut == 1)
        {
            float* M = buffer.getWritePointer (0, pos);
            for (int i = 0; i < len; ++i)
                M[i] += 0.5f * (gD * (dl[i] + dr[i]) + gJ * (jl[i] + jr[i]));
        }
        pos += len;
    }
}

void DigitalComboProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (d50Buf.getNumSamples() == 0) // host forgot prepareToPlay
        return;

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (resyncLastSeen.exchange (false))
        for (auto& b : bound)
            b.lastSeen = (int) std::lround (b.value->load());
    applyParamsToD50Temp();
    decodeD50();
    jd.setParams (readJdParams());

    const int mode = juce::jlimit (0, 3, (int) pMode->load());
    if (mode != lastMode)
    {
        if (lastMode >= 0) { d50.allNotesOff(); jd.allNotesOff(); }
        lastMode = mode;
    }

    replyBuffer.clear();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int s = juce::jlimit (0, n, meta.samplePosition);
        if (s > pos) { renderSegment (buffer, pos, s, mode); pos = s; }
        if (meta.numBytes > 0 && meta.data[0] == 0xF0)
        {
            handleSysEx (meta.data, meta.numBytes, &replyBuffer, s);
            decodeD50();
        }
        else if (meta.numBytes > 0)
        {
            handleMidi (juce::MidiMessage (meta.data, meta.numBytes, 0.0), replyBuffer, s, mode);
        }
    }
    renderSegment (buffer, pos, n, mode);

    const float gain = juce::Decibels::decibelsToGain (pVolume->load(), -60.0f);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const float x = d[i] * gain;
            d[i] = std::isfinite (x) ? juce::jlimit (-4.0f, 4.0f, x) : 0.0f;
        }
    }

    midi.swapWith (replyBuffer);
    d50Voices = d50.activeVoiceCount();
    jdVoices = jd.activeVoiceCount();
}

// ---------------------------------------------------------------------------
void DigitalComboProcessor::syncParametersFromD50Temp()
{
    uint64_t mask[7];
    bool any = false;
    for (int w = 0; w < 7; ++w)
    {
        mask[w] = dirtyBits[(size_t) w].exchange (0);
        any = any || mask[w] != 0;
    }
    if (! any)
        return;
    for (auto& b : bound)
    {
        if ((mask[b.offset / 64] & (uint64_t (1) << (b.offset % 64))) == 0)
            continue;
        const int raw = d50Temp[(size_t) b.offset].load();
        if ((int) std::lround (b.value->load()) != raw)
            b.param->setValueNotifyingHost (b.param->convertTo0to1 ((float) raw));
    }
}

juce::String DigitalComboProcessor::getD50PatchName() const
{
    juce::String s;
    for (int i = 0; i < 18; ++i)
        s += juce::String::charToString (d50::decodeChar (getD50Byte ((int) d50::kPatchFactors + i)));
    return s.trimEnd();
}

int DigitalComboProcessor::importSysExData (const void* data, size_t size)
{
    const auto* bytes = static_cast<const uint8_t*> (data);
    const int before = sysexStats.accepted.load();
    size_t i = 0;
    while (i < size)
    {
        if (bytes[i] != 0xF0) { ++i; continue; }
        size_t j = i + 1;
        while (j < size && bytes[j] != 0xF7 && (bytes[j] & 0x80) == 0) ++j;
        if (j < size && bytes[j] == 0xF7)
            handleSysEx (bytes + i, (int) (j - i + 1), nullptr, 0);
        i = j + 1;
    }
    syncParametersFromD50Temp();
    return sysexStats.accepted.load() - before;
}

juce::MemoryBlock DigitalComboProcessor::exportD50TempAsSysEx() const
{
    uint8_t raw[d50::kTempSize];
    for (int i = 0; i < d50::kTempSize; ++i) raw[i] = d50Temp[(size_t) i].load();
    for (auto& b : bound) // knobs moved since the last audio block are already in the parameter
        raw[b.offset] = (uint8_t) juce::jlimit (0, 127, (int) std::lround (b.value->load()));
    const uint8_t device = (uint8_t) (juce::jlimit (1, 16, (int) std::lround (pSysexCh->load())) - 1);
    juce::MemoryBlock mb;
    uint8_t bytes[d50::kMaxDataBytesPerMessage + 12];
    for (int off = 0; off < d50::kTempSize; off += d50::kMaxDataBytesPerMessage)
    {
        const int n = std::min (d50::kMaxDataBytesPerMessage, d50::kTempSize - off);
        const auto len = d50::buildDT1 (device, (uint32_t) off, raw + off, n, bytes);
        mb.append (bytes, len);
    }
    return mb;
}

// ---------------------------------------------------------------------------
int DigitalComboProcessor::getNumPrograms() { return (int) params::presets().size(); }

const juce::String DigitalComboProcessor::getProgramName (int index)
{
    const auto& p = params::presets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? p[(size_t) index].name : juce::String();
}

void DigitalComboProcessor::setCurrentProgram (int index)
{
    const auto& list = params::presets();
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;
    currentProgram = index;
    resetD50TempToInit();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! nonPresetParams().contains (rp->getParameterID()))
                rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [pid, value] : list[(size_t) index].values)
        if (auto* p = apvts.getParameter (pid))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    forceParamsToTemp = true;
}

void DigitalComboProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("DigitalComboState");
    root.setAttribute ("version", 1);
    root.setAttribute ("program", currentProgram);
    if (auto xml = apvts.copyState().createXml())
        root.addChildElement (xml.release());

    uint8_t raw[d50::kTempSize];
    for (int i = 0; i < d50::kTempSize; ++i) raw[i] = d50Temp[(size_t) i].load();
    for (auto& b : bound)
    {
        const bool pending = (dirtyBits[(size_t) (b.offset / 64)].load() & (uint64_t (1) << (b.offset % 64))) != 0;
        if (! pending) // keep the blob consistent with knob values not yet seen by the audio thread
            raw[b.offset] = (uint8_t) juce::jlimit (0, 127, (int) std::lround (b.value->load()));
    }
    root.createNewChildElement ("D50Temp")->setAttribute ("hex", juce::String::toHexString (raw, d50::kTempSize, 0));

    auto* bank = root.createNewChildElement ("D50UserBank");
    uint8_t slotData[d50::kTempSize];
    for (int s = 0; s < d50::kNumPatches; ++s)
    {
        if (! userBankValid[(size_t) s].load()) continue;
        for (int i = 0; i < d50::kTempSize; ++i) slotData[i] = getUserBankByte (s, i);
        auto* e = bank->createNewChildElement ("Slot");
        e->setAttribute ("index", s);
        e->setAttribute ("hex", juce::String::toHexString (slotData, d50::kTempSize, 0));
    }
    copyXmlToBinary (root, destData);
}

void DigitalComboProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("DigitalComboState"))
        return;
    auto* st = xml->getChildByName (apvts.state.getType());
    if (st == nullptr)
        return;
    const auto tree = juce::ValueTree::fromXml (*st);
    if (! tree.isValid())
        return;

    bool haveTemp = false;
    if (auto* d = xml->getChildByName ("D50Temp"))
    {
        juce::MemoryBlock mb;
        mb.loadFromHexString (d->getStringAttribute ("hex"));
        if ((int) mb.getSize() == d50::kTempSize)
        {
            const auto* b = static_cast<const uint8_t*> (mb.getData());
            for (int i = 0; i < d50::kTempSize; ++i)
                d50Temp[(size_t) i].store (std::min<uint8_t> (b[i] & 0x7F, d50::maxValueForTempOffset (i)));
            haveTemp = true;
        }
    }
    if (auto* bank = xml->getChildByName ("D50UserBank"))
    {
        for (auto* e : bank->getChildIterator())
        {
            const int s = e->getIntAttribute ("index", -1);
            juce::MemoryBlock mb;
            mb.loadFromHexString (e->getStringAttribute ("hex"));
            if (! juce::isPositiveAndBelow (s, d50::kNumPatches) || (int) mb.getSize() != d50::kTempSize)
                continue;
            const auto* b = static_cast<const uint8_t*> (mb.getData());
            for (int i = 0; i < d50::kTempSize; ++i)
                userBank[(size_t) (s * d50::kTempSize + i)].store (std::min<uint8_t> (b[i] & 0x7F, d50::maxValueForTempOffset (i)));
            userBankValid[(size_t) s].store (true);
        }
    }

    if (haveTemp)
    {
        resyncLastSeen = true;
        for (auto& b : bound) // the stored temp area is the source of truth for bound parameters
            dirtyBits[(size_t) (b.offset / 64)].fetch_or (uint64_t (1) << (b.offset % 64));
    }
    apvts.replaceState (tree);
    if (! haveTemp)
    {
        resetD50TempToInit();
        forceParamsToTemp = true;
    }
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, xml->getIntAttribute ("program", 0));
    syncParametersFromD50Temp();
}

juce::AudioProcessorEditor* DigitalComboProcessor::createEditor()
{
    return new DigitalComboEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DigitalComboProcessor();
}
