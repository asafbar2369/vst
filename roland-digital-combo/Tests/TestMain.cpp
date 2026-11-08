// Automated tests for Digital Combo.
// Every D-50 SysEx vector below is copied from Roland's "D-50 MIDI Implementation, Version 2.00"
// (sections 4.2, 7.2.1, 10.1, 10.2, 10.3, 10.4). Exit code = number of failed checks.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "D50SysEx.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace
{
int passed = 0, failed = 0;
juce::String currentSection;

void check (bool ok, const juce::String& what)
{
    if (ok) ++passed; else ++failed;
    std::printf ("%s  [%s] %s\n", ok ? "PASS" : "FAIL", currentSection.toRawUTF8(), what.toRawUTF8());
    std::fflush (stdout);
}
void section (const juce::String& s)
{
    currentSection = s;
    std::printf ("\n== %s ==\n", s.toRawUTF8());
}
juce::String hex (const uint8_t* b, size_t n) { return juce::String::toHexString (b, (int) n, 1).toUpperCase(); }
bool equalBytes (const uint8_t* a, std::initializer_list<int> b, size_t n)
{
    if (b.size() != n) return false;
    size_t i = 0;
    for (int v : b) if (a[i++] != (uint8_t) v) return false;
    return true;
}

struct Rendered
{
    std::vector<float> L, R;
    bool allFinite = true;
    float peak = 0;
    float rms (size_t from = 0, size_t to = SIZE_MAX) const
    {
        to = std::min (to, L.size());
        double s = 0;
        for (size_t i = from; i < to; ++i) s += double (L[i]) * L[i] + double (R[i]) * R[i];
        return to > from ? float (std::sqrt (s / double (2 * (to - from)))) : 0.0f;
    }
    float zeroCrossingHz (double sr, size_t from) const
    {
        int crossings = 0;
        size_t first = 0, last = 0;
        for (size_t i = from + 1; i < L.size(); ++i)
            if (L[i - 1] <= 0.0f && L[i] > 0.0f)
            {
                if (crossings == 0) first = i;
                last = i;
                ++crossings;
            }
        if (crossings < 2) return 0.0f;
        return float ((crossings - 1) * sr / double (last - first));
    }
    float brightness() const // RMS of first difference relative to RMS
    {
        double d = 0, s = 0;
        for (size_t i = 1; i < L.size(); ++i) { d += double (L[i] - L[i - 1]) * (L[i] - L[i - 1]); s += double (L[i]) * L[i]; }
        return s > 0 ? float (std::sqrt (d / s)) : 0.0f;
    }
};

struct Host
{
    std::unique_ptr<DigitalComboProcessor> proc = std::make_unique<DigitalComboProcessor>();
    double sr = 48000.0;
    int block = 512;
    int channels = 2;
    juce::MidiBuffer lastMidiOut;
    std::vector<juce::MidiMessage> allMidiOut;

    void prepare (double sampleRate = 48000.0, int blockSize = 512, int numChannels = 2)
    {
        sr = sampleRate; block = blockSize; channels = numChannels;
        proc->setPlayConfigDetails (0, channels, sr, block);
        proc->prepareToPlay (sr, block);
    }
    void set (const juce::String& id, float plain)
    {
        auto* p = proc->apvts.getParameter (id);
        jassert (p != nullptr);
        if (p != nullptr) p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }
    float get (const juce::String& id) { return proc->apvts.getRawParameterValue (id)->load(); }

    Rendered run (double seconds, std::vector<std::pair<int, juce::MidiMessage>> events = {}, int blockSize = -1)
    {
        Rendered r;
        const int bs = blockSize > 0 ? blockSize : block;
        const int total = int (seconds * sr);
        juce::AudioBuffer<float> buf (channels, bs);
        for (int done = 0; done < total; done += bs)
        {
            const int n = std::min (bs, total - done);
            buf.setSize (channels, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& [pos, m] : events)
                if (pos >= done && pos < done + n) midi.addEvent (m, pos - done);
            proc->processBlock (buf, midi);
            lastMidiOut = midi;
            for (const auto meta : midi) allMidiOut.push_back (meta.getMessage());
            for (int i = 0; i < n; ++i)
            {
                const float l = buf.getSample (0, i), rr = buf.getSample (channels > 1 ? 1 : 0, i);
                if (! std::isfinite (l) || ! std::isfinite (rr)) r.allFinite = false;
                r.peak = std::max (r.peak, std::max (std::abs (l), std::abs (rr)));
                r.L.push_back (l); r.R.push_back (rr);
            }
        }
        return r;
    }
    void sysex (const std::vector<uint8_t>& bytes)
    {
        run (0.01, { { 0, juce::MidiMessage (bytes.data(), (int) bytes.size()) } });
        proc->syncParametersFromD50Temp();
    }
};

juce::MidiMessage on (int note, int vel = 100) { return juce::MidiMessage::noteOn (1, note, (juce::uint8) vel); }
juce::MidiMessage off (int note) { return juce::MidiMessage::noteOff (1, note); }
juce::MidiMessage cc (int num, int val) { return juce::MidiMessage::controllerEvent (1, num, val); }

std::vector<uint8_t> dt1 (uint8_t device, uint32_t address, std::vector<uint8_t> data)
{
    std::vector<uint8_t> out (data.size() + 12);
    out.resize (d50::buildDT1 (device, address, data.data(), (int) data.size(), out.data()));
    return out;
}

// Mono, unfiltered D-50 test voice: partial 1 only, no effects
void makeCleanD50 (Host& h)
{
    h.set (params::id::mode, params::ModeD50);
    h.set ("d50_uc_mute", 2);        // "10": partial 1 on, partial 2 off
    h.set ("d50_up1_cutoff", 100);
    h.set ("d50_up1_reso", 0);
    h.set ("d50_up1_tvfDepth", 0);
    h.set ("d50_uc_chorusBal", 0);
    h.set ("d50_pt_revBal", 0);
}
void makeCleanJd (Host& h)
{
    h.set (params::id::mode, params::ModeJD800);
    h.set (params::jdToneId (0, "wave"), (float) dc::JdWave::Sine);
    h.set (params::jdToneId (0, "cutoff"), 100);
    h.set (params::jdToneId (0, "reso"), 0);
    h.set (params::jdToneId (0, "envDepth"), 0);
    h.set (params::jdToneId (1, "on"), 0);
    h.set (params::id::jdChorusMix, 0);
    h.set (params::id::jdDelayMix, 0);
    h.set (params::id::jdRevMix, 0);
}
} // namespace

// ===========================================================================
static void testDocumentVectors()
{
    section ("D-50 doc: checksum & message builders");
    {
        const uint8_t body[] = { 0x00, 0x00, 0x00, 0x24, 0x32 };
        check (d50::checksum (body, 5) == 0x2A, "10.1 DT1 example checksum = 2AH");
        const uint8_t d[] = { 0x24, 0x32 };
        uint8_t out[32];
        const auto n = d50::buildDT1 (0x00, 0, d, 2, out);
        check (equalBytes (out, { 0xF0, 0x41, 0x00, 0x14, 0x12, 0x00, 0x00, 0x00, 0x24, 0x32, 0x2A, 0xF7 }, n),
               "10.1 DT1 'Pitch Coarse=C4, Fine=0' reproduced byte-exact: " + hex (out, n));
        const auto m = d50::buildRQ1 (0x00, 0, 448, out);
        check (equalBytes (out, { 0xF0, 0x41, 0x00, 0x14, 0x11, 0x00, 0x00, 0x00, 0x00, 0x03, 0x40, 0x3D, 0xF7 }, m),
               "10.2 RQ1 'whole temp area, size 448' reproduced byte-exact: " + hex (out, m));
    }
    {
        const uint8_t wsd[] = { 0xF0, 0x41, 0x00, 0x14, 0x40, 0x02, 0x00, 0x00, 0x00, 0x03, 0x40, 0x3B, 0xF7 };
        d50::Message msg;
        check (d50::parse (wsd, sizeof (wsd), msg) == d50::ParseResult::Ok && msg.command == d50::kWSD
                   && msg.address == d50::patchMemoryAddress (0) && msg.size == 448,
               "10.3 WSD 'Patch memory 1-1, size 448' parses: address 02-00-00, size 448, checksum 3BH");
        const uint8_t rqd[] = { 0xF0, 0x41, 0x00, 0x14, 0x41, 0x02, 0x00, 0x00, 0x00, 0x03, 0x40, 0x3B, 0xF7 };
        check (d50::parse (rqd, sizeof (rqd), msg) == d50::ParseResult::Ok && msg.command == d50::kRQD && msg.size == 448,
               "10.4 RQD 'Patch memory 1-1, size 448' parses with checksum 3BH");
        const uint8_t bulk[] = { 0xF0, 0x41, 0x00, 0x14, 0x40, 0x02, 0x00, 0x00, 0x02, 0x0F, 0x00, 0x6D, 0xF7 };
        check (d50::parse (bulk, sizeof (bulk), msg) == d50::ParseResult::Ok
                   && msg.size == uint32_t (64 * 448 + 16 * 376) && msg.size == d50::kMemoryEnd - d50::kPatchMemoryBase,
               "7.2.1 Bulk-dump WSD size 02-0F-00 (checksum 6DH) = 64 patches x 448 + 16 reverbs x 376 = "
                   + juce::String ((int) msg.size));
    }

    section ("D-50 doc: address map (4.2)");
    uint8_t a[3];
    auto addr = [&a] (uint32_t lin) { d50::toAddress (lin, a); return juce::String::toHexString (a, 3, 1).toUpperCase(); };
    check (addr (d50::kUpperPartial1) == "00 00 00", "Upper Partial-1 = [00-00-00]");
    check (addr (d50::kUpperPartial2) == "00 00 40", "Upper Partial-2 = [00-00-40]");
    check (addr (d50::kUpperCommon) == "00 01 00", "Upper Common = [00-01-00]");
    check (addr (d50::kLowerPartial1) == "00 01 40", "Lower Partial-1 = [00-01-40]");
    check (addr (d50::kLowerPartial2) == "00 02 00", "Lower Partial-2 = [00-02-00]");
    check (addr (d50::kLowerCommon) == "00 02 40", "Lower Common = [00-02-40]");
    check (addr (d50::kPatchFactors) == "00 03 00", "Patch = [00-03-00]");
    check (addr (d50::patchMemoryAddress (0)) == "02 00 00", "Patch Memory 1-1 = [02-00-00]");
    check (addr (d50::patchMemoryAddress (1)) == "02 03 40", "Patch Memory 1-2 = [02-03-40]");
    check (addr (d50::reverbAddress (17)) == "03 60 00", "Reverb Data 17 = [03-60-00]");
    check (addr (d50::reverbAddress (18)) == "03 62 78", "Reverb Data 18 = [03-62-78]");
    check (addr (d50::reverbAddress (32)) == "04 0C 08", "Reverb Data 32 = [04-0C-08]");
    check (d50::patchMemoryAddress (63) + 448 == d50::kReverbBase, "Patch Memory 8-8 ends exactly where Reverb 17 starts");
    check (d50::kTempSize == 448, "Temporary area = 7 blocks x 64 = 448 bytes (doc 10.2 size 448)");

    section ("D-50 doc: parameter tables (*4-4, *4-5, *4-6)");
    struct R { int off; int max; const char* name; };
    for (auto r : { R { 0, 72, "WG Pitch Coarse" }, R { 1, 100, "WG Pitch Fine" }, R { 2, 16, "WG Pitch Keyfollow" },
                    R { 6, 1, "WG Waveform" }, R { 7, 99, "WG PCM Wave No." }, R { 13, 100, "TVF Cutoff Frequency" },
                    R { 14, 30, "TVF Resonance" }, R { 15, 14, "TVF Keyfollow" }, R { 31, 1, "TVF ENV End Level" },
                    R { 38, 12, "TVA Bias Level" }, R { 48, 1, "TVA ENV End Level" }, R { 53, 14, "TVA Mod After touch Range" } })
        check (d50::partialParam (r.off).maxValue == r.max && juce::String (d50::partialParam (r.off).name).isNotEmpty(),
               juce::String ("partial ") + juce::String (r.off) + " " + r.name + " range 0-" + juce::String (r.max));
    for (auto r : { R { 10, 6, "Structure No." }, R { 13, 50, "P-ENV Time 1" }, R { 28, 2, "LFO-1 Sync" },
                    R { 32, 1, "LFO-2 Sync" }, R { 37, 15, "Low EQ Frequency" }, R { 38, 24, "Low EQ Gain" },
                    R { 39, 21, "High EQ Frequency" }, R { 40, 8, "High EQ Q" }, R { 42, 7, "Chorus Type" },
                    R { 46, 3, "Partial Mute" }, R { 47, 100, "Partial Balance" } })
        check (d50::commonParam (r.off).maxValue == r.max,
               juce::String ("common ") + juce::String (r.off) + " " + r.name + " range 0-" + juce::String (r.max));
    for (auto r : { R { 18, 8, "Key Mode" }, R { 19, 60, "Split Point" }, R { 22, 48, "Upper Tone Key Shift" },
                    R { 26, 12, "Bender Range" }, R { 27, 24, "After touch Bend Range" }, R { 29, 3, "Output Mode" },
                    R { 30, 31, "Reverb Type" }, R { 37, 16, "MIDI Transmit Channel" }, R { 39, 100, "MIDI Transmit Prog. Change" } })
        check (d50::patchParam (r.off).maxValue == r.max,
               juce::String ("patch ") + juce::String (r.off) + " " + r.name + " range 0-" + juce::String (r.max));
    check (d50::kLowEqFreqHz[0] == 63 && d50::kLowEqFreqHz[15] == 840, "Low EQ table 63 .. 840 Hz (16 entries)");
    check (d50::kHighEqFreqHz[0] == 250 && d50::kHighEqFreqHz[21] == 9500, "High EQ table 250 Hz .. 9.5 kHz (22 entries)");
    check (d50::kHighEqQ[0] == 0.3f && d50::kHighEqQ[8] == 6.0f, "High EQ Q table 0.3 .. 6.0 (9 entries)");

    bool charsOk = d50::encodeChar (' ') == 0 && d50::encodeChar ('A') == 1 && d50::encodeChar ('Z') == 26
                   && d50::encodeChar ('a') == 27 && d50::encodeChar ('z') == 52 && d50::encodeChar ('1') == 53
                   && d50::encodeChar ('9') == 61 && d50::encodeChar ('0') == 62 && d50::encodeChar ('-') == 63;
    for (int v = 0; v < 64; ++v) charsOk = charsOk && d50::encodeChar (d50::decodeChar ((uint8_t) v)) == v;
    check (charsOk, "Name character set ' ', A-Z, a-z, 1-9, 0, - (values 0..63) round-trips");

    section ("D-50 SysEx parser robustness");
    d50::Message msg;
    const uint8_t badSum[] = { 0xF0, 0x41, 0x00, 0x14, 0x12, 0x00, 0x00, 0x00, 0x24, 0x32, 0x2B, 0xF7 };
    check (d50::parse (badSum, sizeof (badSum), msg) == d50::ParseResult::BadChecksum, "wrong checksum rejected");
    const uint8_t other[] = { 0xF0, 0x43, 0x10, 0x4C, 0x00, 0xF7 };
    check (d50::parse (other, sizeof (other), msg) == d50::ParseResult::NotRoland, "non-Roland message ignored");
    const uint8_t mt32[] = { 0xF0, 0x41, 0x10, 0x16, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7 };
    check (d50::parse (mt32, sizeof (mt32), msg) == d50::ParseResult::WrongModel, "other Roland model ID (16H) ignored");
    const uint8_t trunc[] = { 0xF0, 0x41, 0x00, 0x14, 0x12, 0x00, 0xF7 };
    check (d50::parse (trunc, sizeof (trunc), msg) == d50::ParseResult::Malformed, "truncated DT1 rejected");
    std::vector<uint8_t> big (257, 0);
    auto tooLong = dt1 (0, 0, big);
    check (d50::parse (tooLong.data(), tooLong.size(), msg) == d50::ParseResult::Malformed, "DT1 with 257 data bytes rejected (limit 256)");
    check (d50::parse (nullptr, 0, msg) == d50::ParseResult::Malformed, "null input handled");
}

// ===========================================================================
static void testDspUnits()
{
    section ("DSP building blocks");
    dc::DelayLine d;
    d.prepare (100);
    bool exact = true, inRange = true;
    for (int i = 0; i < 1000; ++i)
    {
        d.push ((float) i);
        exact = exact && d.read (1.0f) == (float) i && d.read (5.0f) == (float) (i - 4 < 0 ? 0 : i - 4) + (i < 4 ? 0.0f : 0.0f);
        const float frac = d.read (2.5f);
        if (i > 5) inRange = inRange && std::abs (frac - ((float) i - 1.5f)) < 1.0e-3f;
        for (float t : { 1.0f, 1.0001f, 97.9999f, 98.0f, 1.0e9f, -5.0f }) // extremes are clamped, never out of bounds
            inRange = inRange && std::isfinite (d.read (t));
    }
    check (exact, "delay line: integer delays return the exact past sample (delay 1 = newest)");
    check (inRange, "delay line: fractional delay 2.5 interpolates; extreme delays stay inside the buffer");

    dc::SegmentEnvelope env;
    env.sampleRate = 1000.0f;
    env.numSegments = 2;
    env.times[0] = 0.01f; env.levels[0] = 1.0f;
    env.times[1] = 0.01f; env.levels[1] = 0.5f;
    env.releaseTime = 0.02f; env.releaseLevel = 0.0f;
    env.noteOn();
    const float a = env.advance (10), b = env.advance (10), s = env.advance (100);
    env.noteOff();
    const float r = env.advance (25);
    check (std::abs (a - 1.0f) < 1.0e-4f && std::abs (b - 0.5f) < 1.0e-4f && std::abs (s - 0.5f) < 1.0e-4f
               && r == 0.0f && ! env.isActive(),
           "segment envelope: attack 10 ms -> 1.0, decay -> sustain 0.5, release ends at 0 and goes idle");
}

static void testProcessorSysEx()
{
    section ("Processor: D-50 SysEx receive / reply");
    Host h;
    h.prepare();

    h.sysex ({ 0xF0, 0x41, 0x00, 0x14, 0x12, 0x00, 0x00, 0x00, 0x30, 0x40, 0x10, 0xF7 });
    check (h.proc->getD50Byte (0) == 0x30 && h.proc->getD50Byte (1) == 0x40, "DT1 writes Upper Partial-1 Coarse/Fine into the temp area");
    check ((int) h.get ("d50_up1_coarse") == 0x30 && (int) h.get ("d50_up1_fine") == 0x40,
           "host parameters follow SysEx (coarse=" + juce::String (h.get ("d50_up1_coarse")) + ")");

    const uint8_t doc101[] = { 0xF0, 0x41, 0x00, 0x14, 0x12, 0x00, 0x00, 0x00, 0x24, 0x32, 0x2A, 0xF7 };
    h.sysex (std::vector<uint8_t> (doc101, doc101 + sizeof (doc101)));
    check (h.proc->getD50Byte (0) == 36 && h.proc->getD50Byte (1) == 50 && (int) h.get ("d50_up1_coarse") == 36,
           "doc 10.1 message sets Coarse=C4 (36), Fine=0 (50)");

    h.sysex (dt1 (5, 0, { 10 }));
    check (h.proc->getD50Byte (0) == 36, "message for device 5 ignored while SysEx channel = 1");
    h.set (params::id::sysexChannel, 6);
    h.sysex (dt1 (5, 0, { 10 }));
    check (h.proc->getD50Byte (0) == 10, "message for device 5 accepted after SysEx channel = 6");
    h.set (params::id::sysexChannel, 1);
    h.set (params::id::sysexRx, 0);
    h.sysex (dt1 (0, 0, { 20 }));
    check (h.proc->getD50Byte (0) == 10, "SysEx receive OFF ignores DT1");
    h.set (params::id::sysexRx, 1);

    h.sysex (dt1 (0, d50::kUpperPartial1 + d50::partial::TvfResonance, { 127 }));
    check (h.proc->getD50Byte (d50::partial::TvfResonance) == 30, "out-of-range Resonance 127 clamped to documented max 30");

    h.sysex (dt1 (0, d50::kUpperCommon + d50::common::Lfo2Rate, { 77 }));
    check (h.proc->getD50Byte ((int) d50::kUpperCommon + d50::common::Lfo2Rate) == 77, "non-automatable byte (LFO-2 Rate) stored");

    // RQ1 for the whole temp area -> two DT1 replies (256 + 192)
    h.allMidiOut.clear();
    uint8_t rq[16];
    const auto rqLen = d50::buildRQ1 (0, 0, 448, rq);
    h.run (0.01, { { 0, juce::MidiMessage (rq, (int) rqLen) } });
    int replies = 0, totalData = 0;
    bool repliesValid = true, dataMatches = true;
    for (auto& m : h.allMidiOut)
    {
        if (! m.isSysEx()) continue;
        d50::Message r;
        const auto res = d50::parse (m.getRawData(), (size_t) m.getRawDataSize(), r);
        repliesValid = repliesValid && res == d50::ParseResult::Ok && r.command == d50::kDT1 && r.dataLength <= 256;
        for (int k = 0; k < r.dataLength; ++k)
            dataMatches = dataMatches && r.data[k] == h.proc->getD50Byte ((int) r.address + k);
        totalData += r.dataLength;
        ++replies;
    }
    check (replies == 2 && totalData == 448 && repliesValid, "RQ1(448) answered with 2 valid DT1 messages, 448 data bytes");
    check (dataMatches, "RQ1 reply data equals the temp area");

    // WSD (handshake) -> RJC
    h.allMidiOut.clear();
    const uint8_t wsd[] = { 0xF0, 0x41, 0x00, 0x14, 0x40, 0x02, 0x00, 0x00, 0x00, 0x03, 0x40, 0x3B, 0xF7 };
    h.run (0.01, { { 0, juce::MidiMessage (wsd, (int) sizeof (wsd)) } });
    bool gotRjc = false;
    for (auto& m : h.allMidiOut)
        if (m.isSysEx() && m.getRawDataSize() == 6 && m.getRawData()[4] == d50::kRJC) gotRjc = true;
    check (gotRjc, "handshake WSD answered with Rejection (RJC), handshake not implemented");

    // Patch Memory 1-2 + Program Change 1
    std::vector<uint8_t> patchData (64, 0);
    patchData[d50::partial::Coarse] = 48;
    h.sysex (dt1 (0, d50::patchMemoryAddress (1), patchData));
    check (h.proc->isUserBankSlotValid (1) && h.proc->getUserBankByte (1, 0) == 48, "DT1 into Patch Memory 1-2 stored in user bank slot 2");
    h.run (0.01, { { 0, juce::MidiMessage::programChange (1, 1) } });
    h.proc->syncParametersFromD50Temp();
    check (h.proc->getD50Byte (0) == 48 && (int) h.get ("d50_up1_coarse") == 48, "Program Change 2 loads user bank slot into the temp area");

    h.sysex (dt1 (0, d50::reverbAddress (17), std::vector<uint8_t> (200, 5)));
    h.sysex (dt1 (0, d50::kMemoryEnd + 10, { 1, 2, 3 }));
    check (true, "Reverb Data writes and beyond-memory writes ignored without crash");

    // Export / import round trip
    const auto exported = h.proc->exportD50TempAsSysEx();
    Host h2;
    h2.prepare();
    const int accepted = h2.proc->importSysExData (exported.getData(), exported.getSize());
    bool same = accepted == 2;
    for (int i = 0; i < d50::kTempSize; ++i) same = same && h2.proc->getD50Byte (i) == h.proc->getD50Byte (i);
    check (same, ".syx export -> import round trip reproduces all 448 bytes (" + juce::String (accepted) + " messages)");
}

// ===========================================================================
static void testModesAndAudio()
{
    section ("Play modes and note routing");
    const char* names[] = { "JD-800", "D-50", "LAYER", "SPLIT" };
    for (int mode = 0; mode < 4; ++mode)
    {
        Host h;
        h.prepare();
        h.set (params::id::mode, (float) mode);
        h.set (params::id::splitPoint, 60);
        const int note = mode == params::ModeSplit ? 48 : 60;
        auto r = h.run (0.5, { { 0, on (note) } });
        const int dv = h.proc->getD50ActiveVoices(), jv = h.proc->getJdActiveVoices();
        bool routeOk = false;
        switch (mode)
        {
            case params::ModeJD800: routeOk = jv == 1 && dv == 0; break;
            case params::ModeD50:   routeOk = dv == 1 && jv == 0; break;
            case params::ModeLayer: routeOk = dv == 1 && jv == 1; break;
            default:                routeOk = dv == 1 && jv == 0; break; // 48 < split 60 -> D-50
        }
        check (r.allFinite && r.rms() > 0.003f && r.peak < 4.0f,
               juce::String (names[mode]) + ": sound produced, finite (rms " + juce::String (r.rms(), 4) + ", peak " + juce::String (r.peak, 3) + ")");
        check (routeOk, juce::String (names[mode]) + ": routed to D-50 voices=" + juce::String (dv) + " JD voices=" + juce::String (jv));
    }
    {
        Host h;
        h.prepare();
        h.set (params::id::mode, params::ModeSplit);
        h.set (params::id::splitPoint, 60);
        h.run (0.1, { { 0, on (72) } });
        check (h.proc->getJdActiveVoices() == 1 && h.proc->getD50ActiveVoices() == 0, "SPLIT: note 72 (>= split) goes to JD-800");
        h.set (params::id::splitSwap, 1);
        h.run (0.1, { { 0, on (40) } });
        check (h.proc->getJdActiveVoices() == 2 && h.proc->getD50ActiveVoices() == 0, "SPLIT swapped: note 40 goes to JD-800");
    }
    {
        Host h;
        h.prepare();
        h.set (params::id::mode, params::ModeLayer);
        h.set (params::id::layerBalance, 0);
        auto onlyD50 = h.run (0.3, { { 0, on (60) } });
        Host h2;
        h2.prepare();
        h2.set (params::id::mode, params::ModeD50);
        auto refD50 = h2.run (0.3, { { 0, on (60) } });
        check (std::abs (onlyD50.rms() - refD50.rms()) < 1.0e-4f, "LAYER balance 0 = D-50 only");
    }

    section ("Pitch accuracy (D-50 coarse 36 = C4, bend range)");
    {
        Host h;
        h.prepare();
        makeCleanD50 (h);
        auto r = h.run (1.0, { { 0, on (69) } });
        const float f = r.zeroCrossingHz (h.sr, 24000);
        check (std::abs (f - 440.0f) < 4.0f, "D-50 saw partial, note A4 -> " + juce::String (f, 2) + " Hz (expected 440)");
        auto r2 = h.run (1.0, { { 0, juce::MidiMessage::pitchWheel (1, 16383) } });
        const float fb = r2.zeroCrossingHz (h.sr, 24000);
        check (std::abs (fb - 493.88f) < 5.0f, "pitch bend +max with Bender Range 2 -> " + juce::String (fb, 2) + " Hz (expected 493.9)");
        h.set ("d50_up1_coarse", 48);
        auto r3 = h.run (1.0, { { 0, juce::MidiMessage::pitchWheel (1, 8192) } });
        const float fc = r3.zeroCrossingHz (h.sr, 24000);
        check (std::abs (fc - 880.0f) < 8.0f, "Coarse 48 (C5) raises by one octave -> " + juce::String (fc, 2) + " Hz (expected 880)");
    }
    {
        Host h;
        h.prepare();
        makeCleanJd (h);
        auto r = h.run (1.0, { { 0, on (69) } });
        const float f = r.zeroCrossingHz (h.sr, 24000);
        check (std::abs (f - 440.0f) < 2.0f, "JD-800 sine tone A, note A4 -> " + juce::String (f, 2) + " Hz (expected 440)");
        check (r.peak < 1.0f && r.rms (24000) > 0.01f, "JD-800 sine level sane: peak " + juce::String (r.peak, 3) + " rms " + juce::String (r.rms (24000), 4)
                   + " first-half rms " + juce::String (r.rms (0, 24000), 4));
    }

    section ("Envelopes, sustain, all-notes-off, polyphony");
    {
        Host h;
        h.prepare();
        h.set (params::id::mode, params::ModeLayer);
        h.run (0.3, { { 0, on (60) } });
        h.run (3.0, { { 0, off (60) } });
        check (h.proc->getD50ActiveVoices() == 0 && h.proc->getJdActiveVoices() == 0, "voices end after note-off and release");

        h.run (0.2, { { 0, cc (64, 127) }, { 10, on (64) }, { 2000, off (64) } });
        h.run (1.0);
        check (h.proc->getD50ActiveVoices() == 1 && h.proc->getJdActiveVoices() == 1, "sustain pedal holds released note");
        h.run (3.0, { { 0, cc (64, 0) } });
        check (h.proc->getD50ActiveVoices() == 0 && h.proc->getJdActiveVoices() == 0, "pedal up releases the note");

        std::vector<std::pair<int, juce::MidiMessage>> chord;
        for (int i = 0; i < 20; ++i) chord.push_back ({ i, on (40 + i * 2) });
        auto r = h.run (0.3, chord);
        check (h.proc->getD50ActiveVoices() == 16 && h.proc->getJdActiveVoices() == 16 && r.allFinite,
               "20 notes -> 16-voice limit with stealing, output finite (peak " + juce::String (r.peak, 3) + ")");
        h.run (3.0, { { 0, cc (123, 0) } });
        check (h.proc->getD50ActiveVoices() == 0 && h.proc->getJdActiveVoices() == 0, "CC123 All Notes Off releases everything");
    }

    section ("Sample-accurate MIDI timing");
    {
        Host h;
        h.prepare (48000, 512);
        h.set (params::id::mode, params::ModeJD800);
        h.set (params::jdToneId (0, "aA"), 0);
        auto r = h.run (512.0 / 48000.0, { { 300, on (60) } });
        bool silentBefore = true, soundAfter = false;
        for (size_t i = 0; i < 300; ++i) silentBefore = silentBefore && r.L[i] == 0.0f;
        for (size_t i = 300; i < 512; ++i) soundAfter = soundAfter || r.L[i] != 0.0f;
        check (silentBefore && soundAfter, "note-on at sample 300: silence before, sound after");
    }

    section ("Automation changes the sound");
    {
        auto brightnessFor = [] (int cutoff)
        {
            Host h;
            h.prepare();
            makeCleanD50 (h);
            h.set ("d50_up1_cutoff", (float) cutoff);
            return h.run (0.5, { { 0, on (48) } }).brightness();
        };
        const float dark = brightnessFor (25), bright = brightnessFor (95);
        check (bright > dark * 2.0f, "D-50 TVF cutoff 25 vs 95: brightness " + juce::String (dark, 4) + " -> " + juce::String (bright, 4));

        auto jdLevel = [] (int level)
        {
            Host h;
            h.prepare();
            makeCleanJd (h);
            h.set (params::jdToneId (0, "level"), (float) level);
            return h.run (0.3, { { 0, on (60) } }).rms();
        };
        const float quiet = jdLevel (30), loud = jdLevel (100);
        check (loud > quiet * 3.0f, "JD tone level 30 vs 100: rms " + juce::String (quiet, 4) + " -> " + juce::String (loud, 4));

        Host h;
        h.prepare();
        h.set (params::id::mode, params::ModeJD800);
        h.run (0.2, { { 0, on (60) } });
        h.set (params::id::masterVolume, -60.0f);
        auto r = h.run (0.2);
        check (r.rms (4800) < 0.002f, "master volume -60 dB mutes output during a held note");
    }

    section ("Sample rates, block sizes, mono output");
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int bs : { 1, 17, 64, 512, 2048 })
        {
            Host h;
            h.prepare (sr, bs);
            h.set (params::id::mode, params::ModeLayer);
            auto r = h.run (0.15, { { 0, on (60) }, { 10, on (67) } }, bs);
            if (! (r.allFinite && r.rms() > 0.002f && r.peak < 4.0f))
                check (false, "sr " + juce::String (sr) + " block " + juce::String (bs) + " rms " + juce::String (r.rms()));
        }
    check (true, "5 sample rates x 5 block sizes in LAYER mode: finite, audible, no clipping beyond safety limit (failures listed above)");
    {
        Host h;
        h.prepare (48000, 512);
        auto r = h.run (0.2, { { 0, on (60) } }, 4096); // block larger than prepared size
        check (r.allFinite && r.rms() > 0.002f, "block of 4096 after prepare(512) handled");
        Host m;
        m.prepare (48000, 512, 1);
        auto rm = m.run (0.2, { { 0, on (60) } });
        check (rm.allFinite && rm.rms() > 0.002f, "mono output layout renders");
    }
}

// ===========================================================================
static void testStateAndPresets()
{
    section ("State save / restore");
    Host h;
    h.prepare();
    h.set (params::id::mode, params::ModeSplit);
    h.set (params::id::splitPoint, 55);
    h.set ("d50_up1_cutoff", 33);
    h.set ("d50_pt_keyMode", 1);
    h.set (params::jdToneId (2, "wave"), (float) dc::JdWave::Metal);
    h.set (params::id::jdDelayTime, 777);
    h.run (0.02);
    h.sysex (dt1 (0, d50::kLowerCommon + d50::common::Lfo3Rate, { 91 }));
    h.sysex (dt1 (0, d50::patchMemoryAddress (5), { 60, 55 }));
    h.set ("d50_up2_cutoff", 12); // moved after the last audio block
    juce::MemoryBlock state;
    h.proc->getStateInformation (state);

    Host h2;
    h2.prepare();
    h2.proc->setStateInformation (state.getData(), (int) state.getSize());
    h2.run (0.02);
    h2.proc->syncParametersFromD50Temp();
    bool paramsEqual = true;
    juce::String firstDiff;
    for (auto* p : h.proc->getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;
        const float a = h.get (rp->getParameterID()), b = h2.get (rp->getParameterID());
        if (std::abs (a - b) > 1.0e-4f) { paramsEqual = false; if (firstDiff.isEmpty()) firstDiff = rp->getParameterID(); }
    }
    check (paramsEqual, "all " + juce::String (h.proc->getParameters().size()) + " parameters restored" + (firstDiff.isEmpty() ? "" : " (diff: " + firstDiff + ")"));
    bool tempEqual = true;
    h.run (0.02);
    for (int i = 0; i < d50::kTempSize; ++i) tempEqual = tempEqual && h.proc->getD50Byte (i) == h2.proc->getD50Byte (i);
    check (tempEqual, "all 448 D-50 temp-area bytes restored (incl. non-automatable LFO-3 Rate)");
    check (h2.proc->isUserBankSlotValid (5) && h2.proc->getUserBankByte (5, 0) == 60, "received user bank slot restored");

    const float before = h2.get ("d50_up1_cutoff");
    const char junk[] = "this is not a plug-in state";
    h2.proc->setStateInformation (junk, (int) sizeof (junk));
    h2.proc->setStateInformation (nullptr, 0);
    juce::MemoryBlock truncated (state.getData(), state.getSize() / 2);
    h2.proc->setStateInformation (truncated.getData(), (int) truncated.getSize());
    juce::Random rng (1234);
    for (int k = 0; k < 50; ++k)
    {
        juce::MemoryBlock rnd ((size_t) rng.nextInt (4000) + 1);
        for (size_t i = 0; i < rnd.getSize(); ++i) rnd[i] = (char) rng.nextInt (256);
        h2.proc->setStateInformation (rnd.getData(), (int) rnd.getSize());
    }
    auto r = h2.run (0.1, { { 0, on (60) } });
    check (h2.get ("d50_up1_cutoff") == before && r.allFinite, "garbage / truncated / random states ignored safely");

    {
        // pluginval-style: hosts may write arbitrary normalised values into switch parameters
        Host hs;
        hs.prepare();
        juce::StringArray switches { params::id::splitSwap, params::id::sysexRx };
        for (int t = 0; t < 4; ++t) switches.add (params::jdToneId (t, "on"));
        for (auto& sid : switches) hs.proc->apvts.getParameter (sid)->setValueNotifyingHost (0.538f);
        std::vector<float> saved;
        for (auto& sid : switches) saved.push_back (hs.proc->apvts.getParameter (sid)->getValue());
        juce::MemoryBlock st;
        hs.proc->getStateInformation (st);
        for (auto& sid : switches) hs.proc->apvts.getParameter (sid)->setValueNotifyingHost (0.219f);
        hs.proc->setStateInformation (st.getData(), (int) st.getSize());
        bool ok = true;
        for (int i = 0; i < switches.size(); ++i)
            ok = ok && std::abs (hs.proc->apvts.getParameter (switches[i])->getValue() - saved[(size_t) i]) < 0.1f;
        check (ok, "switch parameters written with in-between values (0.538 / 0.219) restore exactly (pluginval regression)");
    }

    section ("Presets (original sounds)");
    for (int p = 0; p < h.proc->getNumPrograms(); ++p)
    {
        Host hp;
        hp.prepare();
        hp.proc->setCurrentProgram (p);
        auto rp = hp.run (0.4, { { 0, on (48) }, { 0, on (72) } });
        check (rp.allFinite && rp.rms() > 0.002f && rp.peak < 4.0f && hp.proc->getCurrentProgram() == p,
               "preset " + juce::String (p + 1) + " '" + hp.proc->getProgramName (p) + "' plays (rms " + juce::String (rp.rms(), 4) + ")");
    }
}

// ===========================================================================

// ===========================================================================
static void testD50Expression()
{
    section ("D-50 key/velocity/aftertouch scaling, portamento, hold, aftertouch bend");
    const uint32_t p1 = d50::kUpperPartial1, p2 = d50::kUpperPartial2;

    {   // TVA Bias: ">C4" (point 64+27), level 0 (= -12) -> 2 octaves above => -24 dB (estimate: dB per octave)
        auto level = [&] (int note, bool bias)
        {
            Host h; h.prepare(); makeCleanD50 (h);
            if (bias) h.sysex (dt1 (0, p1 + d50::partial::TvaBiasPoint, { 64 + 27, 0 }));
            return h.run (0.4, { { 0, on (note) } }).rms();
        };
        const float hiRef = level (84, false), hiBias = level (84, true), loRef = level (48, false), loBias = level (48, true);
        const float ratioDb = 20.0f * std::log10 (hiBias / hiRef);
        check (std::abs (ratioDb + 24.0f) < 1.0f && std::abs (loBias - loRef) < 1.0e-6f,
               "TVA Bias >C4 level -12: note C6 " + juce::String (ratioDb, 2) + " dB, note C3 unchanged");
    }
    {   // Aftertouch bend range +12 with channel pressure 127 -> one octave up
        Host h; h.prepare(); makeCleanD50 (h);
        h.sysex (dt1 (0, d50::kPatchFactors + d50::patch::AftertouchBendRange, { 24 }));
        auto r = h.run (1.0, { { 0, on (69) }, { 0, juce::MidiMessage::channelPressureChange (1, 127) } });
        const float f = r.zeroCrossingHz (h.sr, 24000);
        check (std::abs (f - 880.0f) < 8.0f, "After touch Bend Range +12, pressure 127: A4 -> " + juce::String (f, 1) + " Hz (expected 880)");
    }
    {   // Portamento: CC65 on, time 60 (~0.25 s); 57 -> 69 glides upward
        Host h; h.prepare(); makeCleanD50 (h);
        h.sysex (dt1 (0, d50::kPatchFactors + d50::patch::PortamentoTime, { 60 }));
        h.run (0.5, { { 0, cc (65, 127) }, { 0, on (57) }, { 12000, off (57) } });
        auto r = h.run (1.2, { { 0, on (69) } });
        Rendered early; early.L.assign (r.L.begin() + 4800, r.L.begin() + 9600); early.R = early.L;
        const float fEarly = early.zeroCrossingHz (h.sr, 0), fLate = r.zeroCrossingHz (h.sr, 36000);
        check (fEarly > 250.0f && fEarly < 420.0f && std::abs (fLate - 440.0f) < 4.0f,
               "Portamento A3 -> A4: 100-200 ms after note-on " + juce::String (fEarly, 1) + " Hz, settles at " + juce::String (fLate, 1) + " Hz");
        Host hn; hn.prepare(); makeCleanD50 (hn);
        hn.sysex (dt1 (0, d50::kPatchFactors + d50::patch::PortamentoTime, { 60 }));
        hn.run (0.5, { { 0, on (57) }, { 12000, off (57) } });
        auto r2 = hn.run (0.3, { { 0, on (69) } });
        Rendered e2; e2.L.assign (r2.L.begin() + 4800, r2.L.begin() + 9600); e2.R = e2.L;
        check (std::abs (e2.zeroCrossingHz (hn.sr, 0) - 440.0f) < 6.0f, "without CC65 there is no glide");
    }
    {   // Hold Mode U: in Dual, pedal sustains only the upper tone
        Host h; h.prepare();
        h.set (params::id::mode, params::ModeD50);
        h.set ("d50_pt_keyMode", 1);
        h.sysex (dt1 (0, d50::kPatchFactors + d50::patch::HoldMode, { 0 }));
        h.run (1.5, { { 0, cc (64, 127) }, { 10, on (60) }, { 4800, off (60) } });
        const int up = h.proc->getD50ActiveTones (true), lo = h.proc->getD50ActiveTones (false);
        check (up == 1 && lo == 0, "Hold Mode U (Dual): pedal keeps upper tone (" + juce::String (up) + "), lower released (" + juce::String (lo) + ")");
        h.sysex (dt1 (0, d50::kPatchFactors + d50::patch::HoldMode, { 2 }));
        h.run (1.5, { { 0, on (64) }, { 4800, off (64) } });
        check (h.proc->getD50ActiveTones (true) == 2 && h.proc->getD50ActiveTones (false) == 1, "Hold Mode UL keeps both tones");
        h.run (2.0, { { 0, cc (64, 0) } });
        check (h.proc->getD50ActiveTones (true) == 0 && h.proc->getD50ActiveTones (false) == 0, "pedal up releases everything");
    }
    {   // Every newly modelled byte audibly changes the output
        struct Case { const char* name; uint32_t addr; uint8_t value; bool penv; };
        const Case cases[] = {
            { "WG PW Velocity Range (P2)", p2 + d50::partial::PwVelocity, 14, false },
            { "WG PW After touch Range (P2)", p2 + d50::partial::PwAftertouch, 14, false },
            { "TVF Bias Level", p1 + d50::partial::TvfBiasLevel, 14, false },
            { "TVF ENV Depth Keyfollow", p1 + d50::partial::TvfEnvDepthKf, 4, false },
            { "TVF ENV Time Keyfollow", p1 + d50::partial::TvfEnvTimeKf, 4, false },
            { "TVF After touch Range", p1 + d50::partial::TvfAftertouch, 14, false },
            { "TVA ENV Velocity Follow", p1 + d50::partial::TvaVelocityFollow, 4, false },
            { "TVA ENV Time Keyfollow", p1 + d50::partial::TvaTimeKf, 4, false },
            { "TVA After touch Range", p1 + d50::partial::TvaAftertouch, 0, false },
            { "P-ENV Time Keyfollow", d50::kUpperCommon + d50::common::PEnvTimeKf, 4, true },
        };
        auto renderCase = [] (const Case* cs)
        {
            Host h; h.prepare();
            h.set (params::id::mode, params::ModeD50);
            h.set ("d50_up1_tvfDepth", 60);
            if (cs != nullptr && cs->penv)
            {
                h.sysex (dt1 (0, d50::kUpperCommon + d50::common::PEnvL0, { 0 }));
                h.sysex (dt1 (0, d50::kUpperPartial1 + d50::partial::PEnvMode, { 1 }));
            }
            if (cs != nullptr) h.sysex (dt1 (0, cs->addr, { cs->value }));
            return h.run (0.3, { { 0, on (84, 100) }, { 0, juce::MidiMessage::channelPressureChange (1, 100) } });
        };
        const auto base = renderCase (nullptr);
        for (auto& cs : cases)
        {
            auto r = renderCase (&cs);
            if (cs.penv) { // compare against the same P-ENV setup without the keyfollow change
                Case ref = cs; ref.value = 0;
                auto rr = renderCase (&ref);
                float diff = 0; for (size_t i = 0; i < r.L.size(); ++i) diff = std::max (diff, std::abs (r.L[i] - rr.L[i]));
                check (diff > 1.0e-3f && r.allFinite, juce::String (cs.name) + " changes the sound (max diff " + juce::String (diff, 4) + ")");
                continue;
            }
            float diff = 0;
            for (size_t i = 0; i < r.L.size(); ++i) diff = std::max (diff, std::abs (r.L[i] - base.L[i]));
            check (diff > 1.0e-3f && r.allFinite, juce::String (cs.name) + " changes the sound (max diff " + juce::String (diff, 4) + ")");
        }
    }
}

static void testPerformance()
{
    section ("CPU load (informational, this machine)");
    const char* names[] = { "JD-800", "D-50 (Dual)", "LAYER" };
    for (int k = 0; k < 3; ++k)
    {
        Host h; h.prepare (48000, 512);
        h.set (params::id::mode, (float) (k == 0 ? params::ModeJD800 : (k == 1 ? params::ModeD50 : params::ModeLayer)));
        if (k == 1) h.set ("d50_pt_keyMode", 1);
        for (int t = 0; t < 4; ++t) h.set (params::jdToneId (t, "on"), 1);
        std::vector<std::pair<int, juce::MidiMessage>> chord;
        for (int i = 0; i < 16; ++i) chord.push_back ({ 0, on (36 + i * 3) });
        const double seconds = 5.0;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        auto r = h.run (seconds, chord);
        const double cpu = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
        const double rtf = seconds / cpu;
        check (r.allFinite && rtf > 1.0, juce::String (names[k]) + ", 16 held notes (all 4 JD tones on), 48 kHz/512: "
                   + juce::String (cpu, 3) + " s CPU for " + juce::String (seconds, 1) + " s audio = "
                   + juce::String (rtf, 1) + "x real time (" + juce::String (100.0 / rtf, 1) + " % of one core)");
    }
}

static void testParameterBindings()
{
    section ("Parameter <-> D-50 address bindings");
    DigitalComboProcessor proc;
    bool rangesOk = true;
    for (auto& b : params::d50Bindings())
    {
        auto* p = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter (b.id));
        if (p == nullptr || p->getRange().getStart() != 0 || p->getRange().getEnd() != d50::maxValueForTempOffset (b.tempOffset))
        {
            rangesOk = false;
            check (false, b.id + " range does not match document");
        }
    }
    check (rangesOk, juce::String ((int) params::d50Bindings().size()) + " automatable D-50 parameters use exactly the documented 0..max ranges");
    check (proc.apvts.getParameter ("d50_up1_coarse")->getCurrentValueAsText() == "C4", "Coarse 36 displays as C4");
    check (proc.apvts.getParameter ("d50_pt_keyMode")->getText (proc.apvts.getParameter ("d50_pt_keyMode")->convertTo0to1 (8), 32) == "Separate-S",
           "Key Mode 8 displays as Separate-S");
}

static void testEditor()
{
    section ("Editor (GUI) controls");
    DigitalComboProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000, 512);
    proc.prepareToPlay (48000, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    check (ed != nullptr && ed->getWidth() > 0 && ed->getHeight() > 0, "editor created, size " + juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()));
    auto* combo = dynamic_cast<DigitalComboEditor*> (ed.get());
    const auto ids = combo != nullptr ? combo->getAttachedParameterIds() : juce::StringArray();
    juce::StringArray missing;
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! ids.contains (rp->getParameterID())) missing.add (rp->getParameterID());
    check (missing.isEmpty() && ids.size() == proc.getParameters().size(),
           "every parameter (" + juce::String (proc.getParameters().size()) + ") has an attached on-screen control"
               + (missing.isEmpty() ? juce::String() : " missing: " + missing.joinIntoString (",")));

    // Drive a real slider and check the parameter follows
    juce::Slider* cutoff = nullptr;
    std::function<void (juce::Component&)> find = [&] (juce::Component& c)
    {
        if (auto* s = dynamic_cast<juce::Slider*> (&c))
            if (s->getTitle() == proc.apvts.getParameter ("d50_up1_cutoff")->getName (64)) cutoff = s;
        for (auto* child : c.getChildren()) find (*child);
    };
    find (*ed);
    if (cutoff != nullptr) cutoff->setValue (17.0, juce::sendNotificationSync);
    check (cutoff != nullptr && (int) proc.apvts.getRawParameterValue ("d50_up1_cutoff")->load() == 17,
           "moving the on-screen TVF Cutoff knob changes the parameter");
    if (combo != nullptr)
    {
        auto& fe = combo->getFullEditor();
        bool allBlocks = true;
        for (int b = 0; b < 7; ++b) { fe.selectBlock (b); allBlocks = allBlocks && fe.getNumRows() > 10; }
        fe.selectBlock (5); // Lower Common
        auto* s = fe.sliderForOffset ((int) d50::kLowerCommon + d50::common::Lfo2Rate);
        if (s != nullptr) s->setValue (63.0, juce::sendNotificationSync);
        check (allBlocks && s != nullptr && proc.getD50Byte ((int) d50::kLowerCommon + d50::common::Lfo2Rate) == 63,
               "full D-50 editor: all 7 blocks listed; Lower Common LFO-2 Rate slider writes the temp area");
        fe.getNameEditor().setText ("My Lower", false);
        fe.getNameEditor().onReturnKey();
        check (proc.getD50Name ((int) d50::kLowerCommon, 10) == "My Lower", "full D-50 editor: tone name edit writes D-50 characters");
        fe.selectBlock (0);
        auto* cut = fe.sliderForOffset (d50::partial::TvfCutoff);
        if (cut != nullptr) cut->setValue (42.0, juce::sendNotificationSync);
        check (cut != nullptr && (int) proc.apvts.getRawParameterValue ("d50_up1_cutoff")->load() == 42,
               "full D-50 editor: editing a bound byte updates the host parameter too");
        int total = 0;
        for (int b = 0; b < 7; ++b) { fe.selectBlock (b); total += fe.getNumRows(); }
        check (total == 54 * 4 + 38 * 2 + 22, "full D-50 editor exposes " + juce::String (total) + " documented parameters (+ 3 name fields)");
    }
    juce::Image img (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
    juce::Graphics g (img);
    ed->paintEntireComponent (g, false);
    check (true, "editor painted off-screen without crashing");
    ed.reset();
    check (true, "editor destroyed cleanly");
}

int main (int argc, char** argv)
{
    juce::ignoreUnused (argc, argv);
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("Digital Combo automated tests\n");
    testDocumentVectors();
    testDspUnits();
    testProcessorSysEx();
    testModesAndAudio();
    testStateAndPresets();
    testD50Expression();
    testPerformance();
    testParameterBindings();
    testEditor();
    std::printf ("\nRESULT: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
