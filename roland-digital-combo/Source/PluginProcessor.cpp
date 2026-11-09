#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UserWaveLoader.h"

namespace
{
const juce::StringArray& nonPresetParams()
{
    static const juce::StringArray ids { params::id::sysexRx, params::id::sysexChannel, params::id::masterVolume,
                                         params::id::jdUnit };
    return ids;
}

ParamArea::Spec d50Spec()
{
    ParamArea::Spec s;
    s.size = d50::kTempSize;
    s.numSlots = d50::kNumPatches;
    s.bankBase = d50::kPatchMemoryBase;
    s.slotStride = d50::kPatchStride;
    s.minFor = [] (int) { return 0; };
    s.maxFor = [] (int o) { return (int) d50::maxValueForTempOffset (o % d50::kTempSize); };
    s.writeInit = [] (uint8_t* p) { params::writeInitD50Tone (p); };
    return s;
}

ParamArea::Spec jdSpec()
{
    ParamArea::Spec s;
    s.size = jd800::kPatchSize;
    s.numSlots = jd800::kNumPatches;
    s.bankBase = jd800::kPatchMemoryBase;
    s.slotStride = jd800::kPatchSize;
    s.minFor = [] (int o) { return (int) jd800::paramForOffset (o % jd800::kPatchSize).minValue; };
    s.maxFor = [] (int o) { return (int) jd800::paramForOffset (o % jd800::kPatchSize).maxValue; };
    s.writeInit = [] (uint8_t* p) { params::writeInitJdPatch (p); };
    return s;
}

ParamArea::Kind toKind (params::JdBinding::Kind k)
{
    switch (k)
    {
        case params::JdBinding::Waveform: return ParamArea::Kind::Waveform;
        case params::JdBinding::LayerBit: return ParamArea::Kind::LayerBit;
        case params::JdBinding::Byte:
        default:                          return ParamArea::Kind::Byte;
    }
}
} // namespace

DigitalComboProcessor::DigitalComboProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DigitalCombo", params::createLayout()),
      d50Area (d50Spec()), jdArea (jdSpec())
{
    for (auto& b : params::d50Bindings())
        d50Area.bind (apvts.getParameter (b.id), apvts.getRawParameterValue (b.id), b.tempOffset, ParamArea::Kind::Byte);
    for (auto& b : params::jdBindings())
        jdArea.bind (apvts.getParameter (b.id), apvts.getRawParameterValue (b.id), b.offset, toKind (b.kind), b.bit);

    using namespace params::id;
    pMode = apvts.getRawParameterValue (mode);
    pSplit = apvts.getRawParameterValue (splitPoint);
    pSwap = apvts.getRawParameterValue (splitSwap);
    pBalance = apvts.getRawParameterValue (layerBalance);
    pVolume = apvts.getRawParameterValue (masterVolume);
    pSysexRx = apvts.getRawParameterValue (sysexRx);
    pSysexCh = apvts.getRawParameterValue (sysexChannel);
    pJdUnit = apvts.getRawParameterValue (jdUnit);
    mixD50 = { apvts.getRawParameterValue (d50Level), apvts.getRawParameterValue (d50Pan), apvts.getRawParameterValue (d50Transpose),
               apvts.getRawParameterValue (d50KeyLow), apvts.getRawParameterValue (d50KeyHigh) };
    mixJd = { apvts.getRawParameterValue (jdLevel), apvts.getRawParameterValue (jdPan), apvts.getRawParameterValue (jdTranspose),
              apvts.getRawParameterValue (jdKeyLow), apvts.getRawParameterValue (jdKeyHigh) };
    clearNoteMaps();
    for (auto& t : jdTones) t.store (0);
    startTimerHz (20);
}

DigitalComboProcessor::~DigitalComboProcessor()
{
    stopTimer();
    liveWaves = nullptr;
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
    d50Area.forceParametersToArea();
    jdArea.forceParametersToArea();
}

void DigitalComboProcessor::decodeD50() noexcept
{
    uint8_t raw[d50::kTempSize];
    for (int i = 0; i < d50::kTempSize; ++i) raw[i] = d50Area.get (i);
    d50.setParams (dc::D50Params::decode (raw));
}

void DigitalComboProcessor::decodeJd() noexcept
{
    uint8_t raw[jd800::kPatchSize];
    for (int i = 0; i < jd800::kPatchSize; ++i) raw[i] = jdArea.get (i);
    jd.setPatch (dc::JdPatch::decode (raw));
}

void DigitalComboProcessor::getRouting (int note, int mode, bool& toD50, bool& toJd) const noexcept
{
    toD50 = toJd = false;
    switch (mode)
    {
        case params::ModeJD800: toJd = true; break;
        case params::ModeD50:   toD50 = true; break;
        case params::ModeLayer: toD50 = toJd = true; break;
        default:
        {
            const bool low = note < (int) pSplit->load();
            const bool jdLower = pSwap->load() > 0.5f;
            toD50 = low != jdLower;
            toJd = ! toD50;
            break;
        }
    }
    // Keyboard zones apply in every mode
    auto inZone = [note] (const EngineMix& m) { return note >= (int) m.keyLow->load() && note <= (int) m.keyHigh->load(); };
    toD50 = toD50 && inZone (mixD50);
    toJd = toJd && inZone (mixJd);
}

void DigitalComboProcessor::routeNoteOn (int note, float velocity, int mode) noexcept
{
    bool toD50 = false, toJd = false;
    getRouting (note, mode, toD50, toJd);
    auto send = [&] (auto& engine, std::array<int, 128>& sent, const EngineMix& m)
    {
        const int target = juce::jlimit (0, 127, note + (int) std::lround (m.transpose->load()));
        if (sent[(size_t) note] >= 0) engine.noteOff (sent[(size_t) note]); // same key retriggered
        engine.noteOn (target, velocity);
        sent[(size_t) note] = target;
    };
    if (toD50) send (d50, sentD50, mixD50);
    if (toJd) send (jd, sentJd, mixJd);
}

void DigitalComboProcessor::routeNoteOff (int note) noexcept
{
    if (sentD50[(size_t) note] >= 0) { d50.noteOff (sentD50[(size_t) note]); sentD50[(size_t) note] = -1; }
    if (sentJd[(size_t) note] >= 0) { jd.noteOff (sentJd[(size_t) note]); sentJd[(size_t) note] = -1; }
}

void DigitalComboProcessor::handleMidi (const juce::MidiMessage& m, int mode) noexcept
{
    if (m.isNoteOn())
        routeNoteOn (m.getNoteNumber(), m.getFloatVelocity(), mode);
    else if (m.isNoteOff())
        routeNoteOff (m.getNoteNumber());
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        if (cc == 1) { d50.setModWheel ((float) val / 127.0f); jd.setModWheel ((float) val / 127.0f); }
        else if (cc == 64) { d50.setSustain (val >= 64); jd.setSustain (val >= 64); }
        else if (cc == 65) d50.setPortamento (val >= 64);   // Portamento SW (D-50 MIDI chart)
        else if (cc == 5)                                    // Portamento Time (D-50 MIDI chart)
        {
            d50Area.write (d50::kPatchFactors + (uint32_t) d50::patch::PortamentoTime, (val * 100 + 63) / 127);
            decodeD50();
        }
        else if (cc == 120) { d50.allSoundOff(); jd.allSoundOff(); clearNoteMaps(); }
        else if (cc == 121)
        {
            d50.setPitchBend (0); jd.setPitchBend (0); d50.setModWheel (0); jd.setModWheel (0);
            d50.setAftertouch (0); jd.setAftertouch (0);
        }
        else if (cc >= 123) { d50.allNotesOff(); jd.allNotesOff(); clearNoteMaps(); } // D-50 doc *2-10: 123-127 = All Notes Off
    }
    else if (m.isPitchWheel())
    {
        const float b = juce::jlimit (-1.0f, 1.0f, float (m.getPitchWheelValue() - 8192) / 8192.0f);
        d50.setPitchBend (b);
        jd.setPitchBend (b);
    }
    else if (m.isChannelPressure())
    {
        d50.setAftertouch ((float) m.getChannelPressureValue() / 127.0f);
        jd.setAftertouch ((float) m.getChannelPressureValue() / 127.0f);
    }
    else if (m.isProgramChange())
    {
        // 0-63: load a patch received into Patch Memory (user bank), for each instrument that has one.
        const int prog = m.getProgramChangeNumber();
        if (prog < d50::kNumPatches && d50Area.isSlotValid (prog)) { d50Area.loadSlotIntoTemp (prog); decodeD50(); }
        if (prog < jd800::kNumPatches && jdArea.isSlotValid (prog)) { jdArea.loadSlotIntoTemp (prog); decodeJd(); }
    }
}

bool DigitalComboProcessor::handleModelSysEx (bool isJd, const d50::Message& msg, juce::MidiBuffer* replies, int samplePos) noexcept
{
    auto& area = isJd ? jdArea : d50Area;
    const bool fromFile = replies == nullptr;
    const uint8_t device = isJd ? jd800::deviceForUnit (juce::jlimit (jd800::kFirstUnit, jd800::kLastUnit, (int) std::lround (pJdUnit->load())))
                                : (uint8_t) (juce::jlimit (1, 16, (int) std::lround (pSysexCh->load())) - 1);
    if (! fromFile && (msg.device != device || pSysexRx->load() < 0.5f))
        return false;
    sysexStats.lastModel = isJd ? jd800::kModelId : d50::kModelId;
    sysexStats.lastCommand = msg.command;
    sysexStats.lastAddress = msg.address;

    switch (msg.command)
    {
        case d50::kDT1:
        {
            bool any = false;
            for (int k = 0; k < msg.dataLength; ++k)
                any = area.write (msg.address + (uint32_t) k, msg.data[k]) || any;
            return any; // D-50 Reverb Data and other areas we do not hold are ignored (D-50 doc *8-1)
        }
        case d50::kRQ1:
        {
            if (! area.isAddressRange (msg.address, msg.size))
                return false;
            if (replies != nullptr)
            {
                uint8_t chunk[d50::kMaxDataBytesPerMessage];
                uint8_t bytes[d50::kMaxDataBytesPerMessage + 12];
                for (uint32_t off = 0; off < msg.size; off += (uint32_t) d50::kMaxDataBytesPerMessage)
                {
                    const int n = (int) std::min<uint32_t> ((uint32_t) d50::kMaxDataBytesPerMessage, msg.size - off);
                    for (int k = 0; k < n; ++k)
                        area.readForReply (msg.address + off + (uint32_t) k, chunk[k]);
                    const auto len = isJd ? jd800::buildDT1 (device, msg.address + off, chunk, n, bytes)
                                          : d50::buildDT1 (device, msg.address + off, chunk, n, bytes);
                    replies->addEvent (bytes, (int) len, samplePos);
                    ++sysexStats.replies;
                }
            }
            return true;
        }
        case d50::kWSD:
        case d50::kRQD:
            // Handshake transfers are not implemented: answer with Rejection (RJC).
            if (replies != nullptr && ! isJd)
            {
                const uint8_t rjc[] = { d50::kExclusiveStatus, d50::kRolandId, device, d50::kModelId, d50::kRJC, d50::kEndOfExclusive };
                replies->addEvent (rjc, (int) sizeof (rjc), samplePos);
                ++sysexStats.replies;
            }
            return false;
        default:
            return false;
    }
}

void DigitalComboProcessor::handleSysEx (const uint8_t* data, int size, juce::MidiBuffer* replies, int samplePos) noexcept
{
    d50::Message msg;
    const size_t len = (size_t) juce::jmax (0, size);
    auto r = d50::parse (data, len, msg);
    bool isJd = false;
    if (r == d50::ParseResult::WrongModel)
    {
        r = jd800::parse (data, len, msg);
        isJd = true;
    }
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
    if (handleModelSysEx (isJd, msg, replies, samplePos)) ++sysexStats.accepted;
    else ++sysexStats.ignored;
    if (replies != nullptr) // audio thread: apply immediately (file imports are picked up at the next block)
    {
        if (isJd) decodeJd(); else decodeD50();
    }
}

void DigitalComboProcessor::renderSegment (juce::AudioBuffer<float>& buffer, int start, int end, int mode) noexcept
{
    const int numOut = buffer.getNumChannels();
    float gD = 1.0f, gJ = 1.0f;
    if (mode == params::ModeLayer)
    {
        // Equal-power crossfade: full level at the ends, -3 dB each at the centre
        const float b = juce::jlimit (0.0f, 1.0f, pBalance->load() / 100.0f);
        gD = std::cos (b * juce::MathConstants<float>::halfPi);
        gJ = std::sin (b * juce::MathConstants<float>::halfPi);
    }
    // Per-engine level (dB) and pan (balance law: centre = unity on both sides)
    auto mixGains = [] (const EngineMix& m, float base, float& gl, float& gr)
    {
        const float g = base * juce::Decibels::decibelsToGain (m.level->load(), -60.0f);
        const float p = juce::jlimit (-1.0f, 1.0f, m.pan->load() / 50.0f);
        gl = g * std::min (1.0f, 1.0f - p);
        gr = g * std::min (1.0f, 1.0f + p);
    };
    float dL, dR, jL, jR;
    mixGains (mixD50, gD, dL, dR);
    mixGains (mixJd, gJ, jL, jR);
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
                L[i] += dL * dl[i] + jL * jl[i];
                R[i] += dR * dr[i] + jR * jr[i];
            }
        }
        else if (numOut == 1)
        {
            float* M = buffer.getWritePointer (0, pos);
            for (int i = 0; i < len; ++i)
                M[i] += 0.5f * (dL * dl[i] + dR * dr[i] + jL * jl[i] + jR * jr[i]);
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

    // Pick up the current user wave set (hazard-pointer handshake with reloadUserWaves()).
    const dc::UserWaveSet* waves = nullptr;
    do
    {
        waves = liveWaves.load();
        wavesInUse.store (waves);
    } while (waves != liveWaves.load());
    d50.setUserWaves (waves);
    jd.setUserWaves (waves);

    d50Area.applyParameters();
    jdArea.applyParameters();
    decodeD50();
    decodeJd();

    const int mode = juce::jlimit (0, 3, (int) pMode->load());
    if (mode != lastMode)
    {
        if (lastMode >= 0) { d50.allNotesOff(); jd.allNotesOff(); clearNoteMaps(); }
        lastMode = mode;
    }

    replyBuffer.clear();
    int pos = 0;
    for (const auto meta : midi)
    {
        const int s = juce::jlimit (0, n, meta.samplePosition);
        if (s > pos) { renderSegment (buffer, pos, s, mode); pos = s; }
        if (meta.numBytes > 0 && meta.data[0] == 0xF0)
            handleSysEx (meta.data, meta.numBytes, &replyBuffer, s);
        else if (meta.numBytes > 0)
            handleMidi (juce::MidiMessage (meta.data, meta.numBytes, 0.0), mode);
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
    d50UpperTones = d50.activeToneCount (true);
    d50LowerTones = d50.activeToneCount (false);
    jdVoices = jd.activeVoiceCount();
    for (int t = 0; t < 4; ++t) jdTones[(size_t) t] = jd.activeToneCount (t);
}

// ---------------------------------------------------------------------------
juce::String DigitalComboProcessor::getD50Name (int offset, int length) const
{
    juce::String s;
    for (int i = 0; i < length && offset + i < d50::kTempSize; ++i)
        s += juce::String::charToString (d50::decodeChar (getD50Byte (offset + i)));
    return s.trimEnd();
}

void DigitalComboProcessor::setD50Name (int offset, int length, const juce::String& text)
{
    for (int i = 0; i < length && offset + i < d50::kTempSize; ++i)
        d50Area.write ((uint32_t) (offset + i), d50::encodeChar (i < text.length() ? (char) text[i] : ' '));
}

juce::String DigitalComboProcessor::getJdName() const
{
    juce::String s;
    for (int i = 0; i < 16; ++i)
        s += juce::String::charToString ((juce::juce_wchar) juce::jlimit (32, 126, (int) getJdByte (jd800::kCommonOffset + i)));
    return s.trimEnd();
}

void DigitalComboProcessor::setJdName (const juce::String& text)
{
    for (int i = 0; i < 16; ++i)
        jdArea.write ((uint32_t) (jd800::kCommonOffset + i), i < text.length() ? juce::jlimit (32, 126, (int) text[i]) : 32);
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

static juce::MemoryBlock exportArea (const ParamArea& area, bool isJd, uint8_t device)
{
    std::vector<uint8_t> raw ((size_t) area.size());
    area.snapshot (raw.data());
    juce::MemoryBlock mb;
    uint8_t bytes[d50::kMaxDataBytesPerMessage + 12];
    for (int off = 0; off < area.size(); off += d50::kMaxDataBytesPerMessage)
    {
        const int n = std::min (d50::kMaxDataBytesPerMessage, area.size() - off);
        const auto len = isJd ? jd800::buildDT1 (device, (uint32_t) off, raw.data() + off, n, bytes)
                              : d50::buildDT1 (device, (uint32_t) off, raw.data() + off, n, bytes);
        mb.append (bytes, len);
    }
    return mb;
}

juce::MemoryBlock DigitalComboProcessor::exportD50TempAsSysEx() const
{
    return exportArea (d50Area, false, (uint8_t) (juce::jlimit (1, 16, (int) std::lround (pSysexCh->load())) - 1));
}

juce::MemoryBlock DigitalComboProcessor::exportJdTempAsSysEx() const
{
    return exportArea (jdArea, true, jd800::deviceForUnit (juce::jlimit (jd800::kFirstUnit, jd800::kLastUnit, (int) std::lround (pJdUnit->load()))));
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
    currentPatchName = list[(size_t) index].name;
    d50Area.resetToInit();
    jdArea.resetToInit();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! nonPresetParams().contains (rp->getParameterID()))
                rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [pid, value] : list[(size_t) index].values)
        if (auto* p = apvts.getParameter (pid))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    d50Area.forceParametersToArea();
    jdArea.forceParametersToArea();
}

static void saveArea (juce::XmlElement& root, const ParamArea& area, const char* tempTag, const char* bankTag)
{
    std::vector<uint8_t> raw ((size_t) area.size());
    area.snapshot (raw.data());
    root.createNewChildElement (tempTag)->setAttribute ("hex", juce::String::toHexString (raw.data(), area.size(), 0));
    auto* bank = root.createNewChildElement (bankTag);
    for (int s = 0; s < area.numSlots(); ++s)
    {
        if (! area.isSlotValid (s)) continue;
        for (int i = 0; i < area.size(); ++i) raw[(size_t) i] = area.getBank (s, i);
        auto* e = bank->createNewChildElement ("Slot");
        e->setAttribute ("index", s);
        e->setAttribute ("hex", juce::String::toHexString (raw.data(), area.size(), 0));
    }
}

static bool loadArea (const juce::XmlElement& root, ParamArea& area, const char* tempTag, const char* bankTag)
{
    bool haveTemp = false;
    if (auto* d = root.getChildByName (tempTag))
    {
        juce::MemoryBlock mb;
        mb.loadFromHexString (d->getStringAttribute ("hex"));
        if ((int) mb.getSize() == area.size())
        {
            area.loadTemp (static_cast<const uint8_t*> (mb.getData()));
            haveTemp = true;
        }
    }
    if (auto* bank = root.getChildByName (bankTag))
        for (auto* e : bank->getChildIterator())
        {
            const int s = e->getIntAttribute ("index", -1);
            juce::MemoryBlock mb;
            mb.loadFromHexString (e->getStringAttribute ("hex"));
            if (juce::isPositiveAndBelow (s, area.numSlots()) && (int) mb.getSize() == area.size())
                area.loadSlot (s, static_cast<const uint8_t*> (mb.getData()));
        }
    return haveTemp;
}

void DigitalComboProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("DigitalComboState");
    root.setAttribute ("version", 2);
    root.setAttribute ("program", currentProgram);
    root.setAttribute ("guiRenderer", guiRenderer.load());
    root.setAttribute ("userWavesD50", userWaveFolderD50.getFullPathName());
    root.setAttribute ("userWavesJD800", userWaveFolderJd.getFullPathName());
    root.setAttribute ("patchName", currentPatchName);
    if (auto xml = apvts.copyState().createXml())
        root.addChildElement (xml.release());
    saveArea (root, d50Area, "D50Temp", "D50UserBank");
    saveArea (root, jdArea, "JD800Temp", "JD800UserBank");
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

    const bool haveD50 = loadArea (*xml, d50Area, "D50Temp", "D50UserBank");
    const bool haveJd = loadArea (*xml, jdArea, "JD800Temp", "JD800UserBank");
    apvts.replaceState (tree);
    if (! haveD50) { d50Area.resetToInit(); d50Area.forceParametersToArea(); }
    if (! haveJd) { jdArea.resetToInit(); jdArea.forceParametersToArea(); }
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, xml->getIntAttribute ("program", 0));
    guiRenderer = juce::jlimit (0, 1, xml->getIntAttribute ("guiRenderer", 0));
    currentPatchName = xml->getStringAttribute ("patchName");
    syncParametersFromD50Temp();

    auto folderFrom = [&xml] (const char* attr)
    {
        const auto path = xml->getStringAttribute (attr);
        return juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
    };
    const auto d50Folder = folderFrom ("userWavesD50"), jdFolder = folderFrom ("userWavesJD800");
    if (d50Folder != userWaveFolderD50 || jdFolder != userWaveFolderJd)
    {
        userWaveFolderD50 = d50Folder;
        userWaveFolderJd = jdFolder;
        reloadUserWaves();
    }
}

// ---------------------------------------------------------------------------
// Patch files
static const juce::StringArray& patchFileParamIds()
{
    // The non-area parameters a patch file carries (area-bound ones travel as the raw areas).
    static const juce::StringArray ids = []
    {
        using namespace params::id;
        juce::StringArray s { mode, splitPoint, splitSwap, layerBalance };
        s.addArray (params::mixerIds());
        return s;
    }();
    return ids;
}

juce::File DigitalComboProcessor::defaultPatchFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Digital Combo").getChildFile ("Patches");
}

juce::Array<juce::File> DigitalComboProcessor::findUserPatches() const
{
    auto files = patchFolder.isDirectory() ? patchFolder.findChildFiles (juce::File::findFiles, false, "*.dcpatch")
                                           : juce::Array<juce::File>();
    files.sort();
    return files;
}

bool DigitalComboProcessor::savePatchFile (const juce::File& file, const juce::String& name)
{
    juce::XmlElement root ("DigitalComboPatch");
    root.setAttribute ("version", 1);
    root.setAttribute ("name", name);
    for (auto& pid : patchFileParamIds())
        if (auto* p = apvts.getParameter (pid))
        {
            auto* e = root.createNewChildElement ("Param");
            e->setAttribute ("id", pid);
            e->setAttribute ("value", (double) p->convertFrom0to1 (p->getValue()));
        }
    std::vector<uint8_t> raw ((size_t) std::max (d50Area.size(), jdArea.size()));
    d50Area.snapshot (raw.data());
    root.createNewChildElement ("D50Temp")->setAttribute ("hex", juce::String::toHexString (raw.data(), d50Area.size(), 0));
    jdArea.snapshot (raw.data());
    root.createNewChildElement ("JD800Temp")->setAttribute ("hex", juce::String::toHexString (raw.data(), jdArea.size(), 0));
    if (! file.getParentDirectory().createDirectory())
        return false;
    if (! root.writeTo (file))
        return false;
    currentPatchName = name;
    return true;
}

bool DigitalComboProcessor::loadPatchFile (const juce::File& file)
{
    if (! file.existsAsFile() || file.getSize() > 1024 * 1024)
        return false;
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("DigitalComboPatch"))
        return false;
    juce::MemoryBlock d50Raw, jdRaw;
    if (auto* d = xml->getChildByName ("D50Temp")) d50Raw.loadFromHexString (d->getStringAttribute ("hex"));
    if (auto* d = xml->getChildByName ("JD800Temp")) jdRaw.loadFromHexString (d->getStringAttribute ("hex"));
    if ((int) d50Raw.getSize() != d50Area.size() || (int) jdRaw.getSize() != jdArea.size())
        return false;

    d50Area.loadTemp (static_cast<const uint8_t*> (d50Raw.getData()));
    jdArea.loadTemp (static_cast<const uint8_t*> (jdRaw.getData()));
    for (auto& pid : patchFileParamIds())
        if (auto* p = apvts.getParameter (pid))
        {
            float v = p->convertFrom0to1 (p->getDefaultValue());
            for (auto* e : xml->getChildWithTagNameIterator ("Param"))
                if (e->getStringAttribute ("id") == pid)
                    v = (float) e->getDoubleAttribute ("value", v);
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        }
    syncParametersFromD50Temp();
    currentPatchName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    return true;
}

// ---------------------------------------------------------------------------
// User wave files
juce::String DigitalComboProcessor::setUserWaveFolder (bool forJd, const juce::File& folder)
{
    (forJd ? userWaveFolderJd : userWaveFolderD50) = folder;
    reloadUserWaves();
    const int n = getUserWaveCount (forJd);
    juce::String s;
    s << (forJd ? "JD-800" : "D-50") << ": " << n << " user wave" << (n == 1 ? "" : "s") << " loaded";
    if (userWaveMessages.size() > 0) s << ", " << userWaveMessages.size() << " file(s) skipped";
    return s;
}

int DigitalComboProcessor::getUserWaveCount (bool forJd) const
{
    const auto* set = ownedWaves.get();
    return set == nullptr ? 0 : (forJd ? set->countJd() : set->countD50());
}

void DigitalComboProcessor::reloadUserWaves()
{
    auto set = std::make_unique<dc::UserWaveSet>();
    userWaveMessages.clear();
    const auto a = userwaves::loadFolder (userWaveFolderD50, false, *set);
    const auto b = userwaves::loadFolder (userWaveFolderJd, true, *set);
    for (auto& m : a.messages) userWaveMessages.add ("D-50 " + m);
    for (auto& m : b.messages) userWaveMessages.add ("JD-800 " + m);

    if (a.loaded + b.loaded == 0)
        set.reset();
    liveWaves.store (set.get());
    if (ownedWaves != nullptr)
        retiredWaves.push_back (std::move (ownedWaves));
    ownedWaves = std::move (set);
    freeRetiredWaveSets();
}

void DigitalComboProcessor::freeRetiredWaveSets()
{
    const auto* inUse = wavesInUse.load();
    retiredWaves.erase (std::remove_if (retiredWaves.begin(), retiredWaves.end(),
                                        [inUse] (const std::unique_ptr<dc::UserWaveSet>& w) { return w.get() != inUse; }),
                        retiredWaves.end());
}

juce::AudioProcessorEditor* DigitalComboProcessor::createEditor()
{
    return new DigitalComboEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DigitalComboProcessor();
}
