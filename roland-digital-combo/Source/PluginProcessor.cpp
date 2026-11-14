#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UserWaveLoader.h"

namespace
{
const juce::StringArray& nonPresetParams()
{
    static const juce::StringArray ids { params::id::sysexRx, params::id::sysexChannel, params::id::masterVolume,
                                         params::id::jdUnit, params::id::modWheel, params::id::pitchBend };
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
    pModParam = apvts.getRawParameterValue (modWheel);
    pBendParam = apvts.getRawParameterValue (pitchBend);
    clearNoteMaps();
    for (auto& t : jdTones) t.store (0);
    for (auto& p : outPeak) p.store (0.0f);
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
    lastModParam = lastBendParam = -1; // re-apply the stored wheel positions
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

void DigitalComboProcessor::routeNoteOn (int note, float velocity, int mode, int midiChannel) noexcept
{
    if (note < 0 || note > 127 || midiChannel < 1 || midiChannel > 16) return;
    const size_t slot = (size_t) ((midiChannel - 1) * 128 + note);
    bool toD50 = false, toJd = false;
    getRouting (note, mode, toD50, toJd);
    auto releasePrevious = [&] (auto& sent, bool isD50)
    {
        const int prior = sent[slot];
        sent[slot] = -1;
        if (prior < 0) return;
        // D-50 tracks source MIDI channels; JD-800 is omni internally, so
        // leave its pitch sounding while another source key still owns it.
        if (isD50)
        {
            if (! dc::midi::hasOwner (sent, prior, midiChannel))
                d50.noteOff (prior, midiChannel);
        }
        else if (! dc::midi::hasOwner (sent, prior))
            jd.noteOff (prior);
    };
    releasePrevious (sentD50, true);
    releasePrevious (sentJd, false);
    auto transpose = [note] (const EngineMix& m) { return note + (int) std::lround (m.transpose->load()); };
    if (toD50)
    {
        const int target = transpose (mixD50);
        if (target >= 0 && target <= 127)
        {
            d50.noteOn (target, velocity, midiChannel);
            sentD50[slot] = target;
        }
    }
    if (toJd)
    {
        const int target = transpose (mixJd);
        if (target >= 0 && target <= 127)
        {
            jd.noteOn (target, velocity);
            sentJd[slot] = target;
        }
    }
}

void DigitalComboProcessor::routeNoteOff (int note, int midiChannel) noexcept
{
    if (note < 0 || note > 127 || midiChannel < 1 || midiChannel > 16) return;
    const size_t slot = (size_t) ((midiChannel - 1) * 128 + note);
    const int dPitch = sentD50[slot];
    const int jPitch = sentJd[slot];
    sentD50[slot] = sentJd[slot] = -1;
    if (dPitch >= 0 && ! dc::midi::hasOwner (sentD50, dPitch, midiChannel))
        d50.noteOff (dPitch, midiChannel);
    if (jPitch >= 0 && ! dc::midi::hasOwner (sentJd, jPitch))
        jd.noteOff (jPitch);
}

void DigitalComboProcessor::applyModWheel (int v) noexcept
{
    v = juce::jlimit (0, 127, v);
    d50.setModWheel ((float) v / 127.0f);
    jd.setModWheel ((float) v / 127.0f);
    uiMod = v;
}

void DigitalComboProcessor::applyPitchBend (int v) noexcept
{
    v = juce::jlimit (0, 16383, v);
    const float b = juce::jlimit (-1.0f, 1.0f, float (v - 8192) / 8192.0f);
    d50.setPitchBend (b);
    jd.setPitchBend (b);
    uiBend = v;
}

float DigitalComboProcessor::getBendSemitones (bool forJd) const noexcept
{
    const float b = juce::jlimit (-1.0f, 1.0f, float (uiBend.load() - 8192) / 8192.0f);
    if (! forJd)
        return b * float (getD50Byte ((int) d50::kPatchFactors + d50::patch::BenderRange));
    const float up = (float) getJdByte (jd800::kCommonOffset + jd800::common::BenderUp);
    const float down = (float) getJdByte (jd800::kCommonOffset + jd800::common::BenderDown);
    return b >= 0.0f ? b * up : b * down;
}

void DigitalComboProcessor::sendPerformanceMessage (const juce::MidiMessage& m)
{
    if (m.getRawDataSize() < 2 || m.getRawDataSize() > 3) return;
    const uint8_t* d = m.getRawData();
    const uint32_t w = (uint32_t) d[0] << 16 | (uint32_t) d[1] << 8 | (m.getRawDataSize() > 2 ? (uint32_t) d[2] : 0u);
    int s1, n1, s2, n2;
    uiFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) { uiFifoData[(size_t) s1] = w; uiFifo.finishedWrite (1); }
}

void DigitalComboProcessor::handleMidi (const juce::MidiMessage& m, int mode) noexcept
{
    midiInCount.fetch_add (1);
    if (m.isNoteOn())
        routeNoteOn (m.getNoteNumber(), m.getFloatVelocity(), mode, m.getChannel());
    else if (m.isNoteOff())
        routeNoteOff (m.getNoteNumber(), m.getChannel());
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        if (cc == 1) applyModWheel (val);
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
            applyPitchBend (8192); applyModWheel (0);
            d50.setAftertouch (0); jd.setAftertouch (0);
        }
        else if (cc >= 123) { d50.allNotesOff(); jd.allNotesOff(); clearNoteMaps(); } // D-50 doc *2-10: 123-127 = All Notes Off
    }
    else if (m.isPitchWheel())
    {
        applyPitchBend (m.getPitchWheelValue());
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

    // Wheel / controller messages queued by the editor
    {
        int s1, n1, s2, n2;
        uiFifo.prepareToRead (uiFifo.getNumReady(), s1, n1, s2, n2);
        for (int part = 0; part < 2; ++part)
        {
            const int start = part == 0 ? s1 : s2, count = part == 0 ? n1 : n2;
            for (int i = 0; i < count; ++i)
            {
                const uint32_t w = uiFifoData[(size_t) (start + i)];
                const uint8_t b[3] = { (uint8_t) (w >> 16), (uint8_t) (w >> 8), (uint8_t) w };
                midi.addEvent (juce::MidiMessage (b, 3), 0);
            }
        }
        uiFifo.finishedRead (n1 + n2);
    }

    // Pick up the current user wave set (hazard-pointer handshake with reloadUserWaves()).
    const dc::UserWaveSet* waves = nullptr;
    do
    {
        waves = liveWaves.load();
        wavesInUse.store (waves);
    } while (waves != liveWaves.load());
    d50.setUserWaves (waves);
    jd.setUserWaves (waves);

    // Host automation / the on-screen wheels' parameters: applied whenever the value changes.
    {
        const int mp = juce::jlimit (0, 127, (int) std::lround (pModParam->load()));
        const int bp = juce::jlimit (0, 16383, (int) std::lround (pBendParam->load()));
        if (mp != lastModParam.load()) { lastModParam = mp; applyModWheel (mp); }
        if (bp != lastBendParam.load()) { lastBendParam = bp; applyPitchBend (bp); }
    }

    d50Area.applyParameters();
    jdArea.applyParameters();
    decodeD50();
    d50.setBasicReceiveChannel (juce::jlimit (1, 16, (int) std::lround (pSysexCh->load())));
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

    for (int ch = 0; ch < 2; ++ch)
    {
        float pk = 0.0f;
        if (ch < buffer.getNumChannels())
        {
            const float* d = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (d[i]));
        }
        outPeak[(size_t) ch].store (pk);
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

// ---------------------------------------------------------------------------
bool DigitalComboProcessor::isImportedBankSlotValid (bool isJd, int slot) const noexcept
{
    return (isJd ? jdArea : d50Area).isSlotValid (slot);
}

bool DigitalComboProcessor::isImportedBankSlotComplete (bool isJd, int slot) const noexcept
{
    return (isJd ? jdArea : d50Area).isSlotComplete (slot);
}

juce::String DigitalComboProcessor::getImportedBankSlotName (bool isJd, int slot) const
{
    if (! isImportedBankSlotValid (isJd, slot)) return {};
    juce::String name;
    const auto& area = isJd ? jdArea : d50Area;
    const int start = isJd ? jd800::kCommonOffset : int (d50::kPatchFactors + d50::patch::Name);
    const int count = isJd ? 16 : 18;
    for (int n = 0; n < count; ++n)
    {
        const uint8_t ch = area.getBank (slot, start + n);
        name += juce::String::charToString (isJd ? (juce::juce_wchar) juce::jlimit (32, 126, int (ch))
                                               : (juce::juce_wchar) d50::decodeChar (ch));
    }
    return name.trim();
}

bool DigitalComboProcessor::loadImportedBankSlot (bool isJd, int slot)
{
    auto& area = isJd ? jdArea : d50Area;
    if (! area.isSlotValid (slot)) return false;
    area.loadSlotIntoTemp (slot);
    area.syncParameters();
    currentPatchName = juce::String (isJd ? "JD-800" : "D-50") + " SysEx "
                     + juce::String (slot / 8 + 1) + "-" + juce::String (slot % 8 + 1);
    return true;
}

int DigitalComboProcessor::importSysExData (const void* data, size_t size)
{
    // The GUI limits imports to 4 MiB. Guard the public API as well.
    if (data == nullptr || size == 0 || size > 8u * 1024u * 1024u) return 0;
    const auto* bytes = static_cast<const uint8_t*> (data);
    const int before = sysexStats.accepted.load();
    size_t i = 0;
    while (i < size)
    {
        if (bytes[i] != 0xF0) { ++i; continue; }
        size_t j = i + 1;
        while (j < size && bytes[j] != 0xF7 && (bytes[j] & 0x80) == 0) ++j;
        if (j < size && bytes[j] == 0xF7)
        {
            handleSysEx (bytes + i, (int) (j - i + 1), nullptr, 0);
            i = j + 1;
        }
        else if (j < size && bytes[j] == 0xF0)
        {
            // A truncated message can be followed immediately by a valid F0.
            // Restart at that F0 instead of silently discarding it.
            i = j;
        }
        else
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

// Export bank-memory addresses as native Roland DT1 packets. Never export a
// partially received patch: doing so would silently invent data for missing bytes.
static juce::MemoryBlock exportCompleteBankSlots (const ParamArea& area, bool isJd, uint8_t device, uint32_t base)
{
    juce::MemoryBlock mb;
    uint8_t bytes[d50::kMaxDataBytesPerMessage + 12];
    std::vector<uint8_t> raw ((size_t) area.size());
    for (int slot = 0; slot < area.numSlots(); ++slot)
    {
        if (! area.isSlotComplete (slot)) continue;
        for (int i = 0; i < area.size(); ++i) raw[(size_t) i] = area.getBank (slot, i);
        for (int off = 0; off < area.size(); off += d50::kMaxDataBytesPerMessage)
        {
            const int n = std::min (d50::kMaxDataBytesPerMessage, area.size() - off);
            const uint32_t address = base + uint32_t (slot * area.size() + off);
            const auto len = isJd ? jd800::buildDT1 (device, address, raw.data() + off, n, bytes)
                                  : d50::buildDT1 (device, address, raw.data() + off, n, bytes);
            mb.append (bytes, len);
        }
    }
    return mb;
}

juce::MemoryBlock DigitalComboProcessor::exportCompleteBanksAsSysEx() const
{
    const uint8_t dDevice = (uint8_t) (juce::jlimit (1, 16, (int) std::lround (pSysexCh->load())) - 1);
    const uint8_t jDevice = jd800::deviceForUnit (juce::jlimit (jd800::kFirstUnit, jd800::kLastUnit, (int) std::lround (pJdUnit->load())));
    auto mb = exportCompleteBankSlots (d50Area, false, dDevice, d50::kPatchMemoryBase);
    const auto jdMb = exportCompleteBankSlots (jdArea, true, jDevice, jd800::kPatchMemoryBase);
    mb.append (jdMb.getData(), jdMb.getSize());
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
        if (! area.isSlotComplete (s))
        {
            // Keep coverage as a hex string of 0/1 bytes. Without this, a partially
            // received SysEx patch incorrectly becomes "complete" after state reload.
            std::vector<uint8_t> seen ((size_t) area.size());
            for (int i = 0; i < area.size(); ++i)
                seen[(size_t) i] = area.isBankByteSeen (s, i) ? 1 : 0;
            e->setAttribute ("seen", juce::String::toHexString (seen.data(), area.size(), 0));
        }
    }
}

static bool loadArea (const juce::XmlElement& root, ParamArea& area, const char* tempTag, const char* bankTag)
{
    bool haveTemp = false;
    // Remove stale user-bank slots when restoring a different DAW project. Only the
    // slots present in the incoming state are permitted to survive.
    area.clearBank();
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
            if (! juce::isPositiveAndBelow (s, area.numSlots()) || (int) mb.getSize() != area.size())
                continue;
            if (e->hasAttribute ("seen"))
            {
                juce::MemoryBlock seen;
                seen.loadFromHexString (e->getStringAttribute ("seen"));
                if ((int) seen.getSize() != area.size()) continue; // malformed coverage
                const auto* ptr = static_cast<const uint8_t*> (seen.getData());
                bool canonical = true;
                for (int i = 0; i < area.size(); ++i)
                    if (ptr[i] > 1) { canonical = false; break; }
                if (! canonical) continue;
                area.loadSlotWithCoverage (s, static_cast<const uint8_t*> (mb.getData()), ptr);
            }
            else // previous saved-state versions stored only complete bank slots
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
    root.setAttribute ("editorScale", (double) editorScale.load());
    root.setAttribute ("userWavesD50", getUserWaveFolder (false).getFullPathName());
    root.setAttribute ("userWavesJD800", getUserWaveFolder (true).getFullPathName());
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
    editorScale = juce::jlimit (0.5f, 3.0f, (float) xml->getDoubleAttribute ("editorScale", 1.0));
    currentPatchName = xml->getStringAttribute ("patchName");
    lastModParam = lastBendParam = -1; // the restored wheel parameters are applied on the next block
    syncParametersFromD50Temp();

    auto folderFrom = [&xml] (const char* attr)
    {
        const auto path = xml->getStringAttribute (attr);
        return juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
    };
    const auto d50Folder = folderFrom ("userWavesD50"), jdFolder = folderFrom ("userWavesJD800");
    const juce::ScopedLock sl (wavesLock);
    if (d50Folder != userWaveFolderD50 || jdFolder != userWaveFolderJd)
    {
        userWaveFolderD50 = d50Folder;
        userWaveFolderJd = jdFolder;
        reloadUserWaves();
    }
}

// ---------------------------------------------------------------------------
// Undo / redo and INIT
DigitalComboProcessor::Snapshot DigitalComboProcessor::takeSnapshot() const
{
    Snapshot s;
    d50Area.snapshot (s.d50.data());
    jdArea.snapshot (s.jd.data());
    return s;
}

void DigitalComboProcessor::restoreSnapshot (const Snapshot& s)
{
    d50Area.loadTemp (s.d50.data());
    jdArea.loadTemp (s.jd.data());
    syncParametersFromD50Temp();
}

void DigitalComboProcessor::pushUndoPoint()
{
    const juce::ScopedLock sl (undoLock);
    auto snap = takeSnapshot();
    if (! undoStack.empty() && undoStack.back().d50 == snap.d50 && undoStack.back().jd == snap.jd)
        return;
    undoStack.push_back (std::move (snap));
    if (undoStack.size() > 200) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

bool DigitalComboProcessor::canUndo() const { const juce::ScopedLock sl (undoLock); return ! undoStack.empty(); }
bool DigitalComboProcessor::canRedo() const { const juce::ScopedLock sl (undoLock); return ! redoStack.empty(); }

bool DigitalComboProcessor::undo()
{
    const juce::ScopedLock sl (undoLock);
    if (undoStack.empty()) return false;
    redoStack.push_back (takeSnapshot());
    const auto s = undoStack.back();
    undoStack.pop_back();
    restoreSnapshot (s);
    return true;
}

bool DigitalComboProcessor::redo()
{
    const juce::ScopedLock sl (undoLock);
    if (redoStack.empty()) return false;
    undoStack.push_back (takeSnapshot());
    const auto s = redoStack.back();
    redoStack.pop_back();
    restoreSnapshot (s);
    return true;
}

void DigitalComboProcessor::initPatch (bool forJd)
{
    pushUndoPoint();
    if (forJd)
    {
        std::array<uint8_t, jd800::kPatchSize> raw {};
        params::writeInitJdPatch (raw.data());
        jdArea.loadTemp (raw.data());
    }
    else
    {
        std::array<uint8_t, d50::kTempSize> raw {};
        params::writeInitD50Tone (raw.data());
        d50Area.loadTemp (raw.data());
    }
    syncParametersFromD50Temp();
    currentPatchName = {};
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
    {
        const juce::ScopedLock sl (wavesLock);
        (forJd ? userWaveFolderJd : userWaveFolderD50) = folder;
    }
    reloadUserWaves();
    const int n = getUserWaveCount (forJd);
    juce::String s;
    s << (forJd ? "JD-800" : "D-50") << ": " << n << " user wave" << (n == 1 ? "" : "s") << " loaded";
    if (userWaveMessages.size() > 0) s << ", " << userWaveMessages.size() << " file(s) skipped";
    return s;
}

int DigitalComboProcessor::getUserWaveCount (bool forJd) const
{
    const juce::ScopedLock sl (wavesLock);
    const auto* set = ownedWaves.get();
    return set == nullptr ? 0 : (forJd ? set->countJd() : set->countD50());
}

void DigitalComboProcessor::reloadUserWaves()
{
    const juce::ScopedLock sl (wavesLock);
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
    const juce::ScopedLock sl (wavesLock);
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
