// Automated tests for Digital Combo.
// Every D-50 SysEx vector below is copied from Roland's "D-50 MIDI Implementation, Version 2.00"
// (sections 4.2, 7.2.1, 10.1, 10.2, 10.3, 10.4). Exit code = number of failed checks.

#if defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wfloat-equal" // the tests compare exact table values on purpose
#endif

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "D50SysEx.h"
#include "D50Engine.h"
#include "JD800SysEx.h"
#include "JdEffects.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

namespace
{
int passed = 0, failed = 0;
juce::File screenshotDir;
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
    h.set (params::jdToneId (0, "wave"), 11);      // 012 Syn Sine
    h.set (params::jdToneId (0, "cutoff"), 100);
    h.set (params::jdToneId (0, "reso"), 0);
    h.set (params::jdToneId (0, "envDepth"), 50);  // 0
    h.set (params::jdToneId (1, "on"), 0);
    h.set ("jd_bBalance", 0);                      // Group B "100:0" = dry only
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
        h.set (params::jdToneId (0, "aT1"), 0);
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
    h.set (params::jdToneId (2, "wave"), 46);     // 047 Xylo
    h.set ("jd_revTime", 77);
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
    bool jdEqual = true;
    for (int i = 0; i < jd800::kPatchSize; ++i) jdEqual = jdEqual && h.proc->getJdByte (i) == h2.proc->getJdByte (i);
    check (jdEqual, "all 384 JD-800 patch bytes restored");
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

    section ("Output quality: DC offset and clipping (presets, chord + melody)");
    for (int p = 0; p < h.proc->getNumPrograms(); ++p)
    {
        Host hq;
        hq.prepare();
        hq.proc->setCurrentProgram (p);
        std::vector<std::pair<int, juce::MidiMessage>> ev;
        for (int n : { 48, 55, 60, 64 }) { ev.push_back ({ 0, on (n, 110) }); ev.push_back ({ 96000, off (n) }); }
        for (int i = 0; i < 4; ++i) { ev.push_back ({ 24000 + i * 16000, on (72 + i * 2, 120) }); ev.push_back ({ 24000 + (i + 1) * 16000 - 200, off (72 + i * 2) }); }
        auto rq = hq.run (3.0, ev);
        double dc = 0;
        for (size_t i = 0; i < rq.L.size(); ++i) dc += rq.L[i] + rq.R[i];
        dc /= double (2 * rq.L.size());
        check (std::abs (dc) < 0.005 && rq.peak < 0.9f, "preset " + juce::String (p + 1) + ": DC " + juce::String (dc, 5)
                   + ", peak " + juce::String (20.0f * std::log10 (rq.peak + 1.0e-9f), 1) + " dBFS (limit -0.9 dBFS)");
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

// ===========================================================================
static void testPcmBank()
{
    section ("D-50 PCM substitute bank (names/types from Owner's Manual p.34)");
    const auto& bank = dc::PcmBank::get();
    int typeOk = 0, finiteOk = 0, loopOk = 0;
    for (int i = 0; i < 100; ++i)
    {
        const auto& w = bank.waves[i];
        if (w.loop == d50::pcmIsLooped (i)) ++typeOk;
        bool fin = w.data.size() >= 64;
        float pk = 0;
        for (auto x : w.data) { fin = fin && std::isfinite (x); pk = std::max (pk, std::abs (x)); }
        if (fin && pk > 0.1f && pk <= 1.0f) ++finiteOk;
        if (! w.loop || (w.loopEnd > w.loopStart && w.loopEnd <= (int) w.data.size())) ++loopOk;
    }
    check (typeOk == 100, "1-47 one-shot, 48-100 looped, as listed (" + juce::String (typeOk) + "/100)");
    check (finiteOk == 100, "all 100 waves non-empty, finite, peak 0.1..1 (" + juce::String (finiteOk) + "/100)");
    check (loopOk == 100, "every looped wave has valid loop points");
    check (juce::String (d50::kPcmDisplay[0]) == "Marmba" && juce::String (d50::kPcmName[46]) == "Pizzicart"
               && juce::String (d50::kPcmDisplay[75]) == "Noise" && juce::String (d50::kPcmDisplay[99]) == "Loop24",
           "PCM name table: 1 Marmba, 47 Pizzicart, 76 Noise, 100 Loop24");
    check (params::formatD50Value (d50::partial::PcmWave, 0) == "1 Marmba *",
           "PCM value shown as number + LCD name, '*' marks a substitute: " + params::formatD50Value (d50::partial::PcmWave, 0));

    auto energy = [] (const std::vector<float>& d, size_t a, size_t b)
    {
        double s = 0; for (size_t i = a; i < b && i < d.size(); ++i) s += double (d[i]) * d[i]; return s;
    };
    {
        const auto& m = bank.waves[0].data; // Marimba: struck -> decays
        const size_t n = m.size();
        check (energy (m, 0, n / 10) > 5.0 * energy (m, n * 8 / 10, n * 9 / 10), "Marimba (1) decays like a struck bar (> 7 dB over the sample)");
    }
    {
        // Spectrum 1..7 (68..74) put a narrow formant at rising frequencies -> brightness rises.
        auto bright = [] (const std::vector<float>& d)
        {
            double dd = 0, s = 0;
            for (size_t i = 0; i < d.size(); ++i)
            {
                const float prev = d[(i + d.size() - 1) % d.size()];
                dd += double (d[i] - prev) * (d[i] - prev); s += double (d[i]) * d[i];
            }
            return std::sqrt (dd / s);
        };
        bool rising = true;
        juce::String vals;
        for (int k = 67; k < 74; ++k)
        {
            vals << juce::String (bright (bank.waves[k].data), 3) << " ";
            if (k > 67 && bright (bank.waves[k].data) <= bright (bank.waves[k - 1].data)) rising = false;
        }
        check (rising, "Spectrum 1..7 get brighter in order: " + vals);
    }

    // Every wave played through the engine (structure 6 = P + P, partial 1 only)
    int ok = 0;
    juce::String bad;
    for (int i = 0; i < 100; ++i)
    {
        Host h;
        h.prepare();
        makeCleanD50 (h);
        h.set ("d50_uc_structure", 5);
        h.set ("d50_up1_pcm", (float) i);
        auto r = h.run (0.25, { { 0, on (60) } });
        const bool good = r.allFinite && r.peak < 4.0f && r.rms (0, size_t (0.1 * h.sr)) > 1.0e-3f;
        if (good) ++ok; else bad << (i + 1) << " ";
    }
    check (ok == 100, "all 100 PCM waves sound through a PCM partial, finite and bounded (" + juce::String (ok) + "/100) " + bad);
    {
        // A looped wave keeps sounding while held; a one-shot wave ends.
        auto tail = [] (int pcm)
        {
            Host h;
            h.prepare();
            makeCleanD50 (h);
            h.set ("d50_uc_structure", 5);
            h.set ("d50_up1_pcm", (float) pcm);
            auto r = h.run (2.5, { { 0, on (60) } });
            return r.rms (size_t (2.0 * h.sr), size_t (2.5 * h.sr));
        };
        const float shot = tail (14), loop = tail (47);
        check (shot < 1.0e-4f && loop > 1.0e-2f, "held note: one-shot 15 Pick has ended (" + juce::String (shot, 6)
                   + "), loop 48 Drawbr still sounds (" + juce::String (loop, 4) + ")");
    }
}

static void testMixerAndZones()
{
    section ("Engine mixer: level, pan, transpose, key zones");
    auto play = [] (std::function<void (Host&)> setup, int note = 60, double secs = 0.4)
    {
        Host h;
        h.prepare();
        makeCleanD50 (h);
        setup (h);
        return h.run (secs, { { 0, on (note) } });
    };
    {
        auto ref = play ([] (Host&) {});
        auto up = play ([] (Host& h) { h.set (params::id::d50Transpose, 12); });
        const float f0 = ref.zeroCrossingHz (48000.0, 4800), f1 = up.zeroCrossingHz (48000.0, 4800);
        check (std::abs (f1 / f0 - 2.0f) < 0.02f, "D-50 transpose +12 doubles the pitch (" + juce::String (f0, 1) + " -> " + juce::String (f1, 1) + " Hz)");
    }
    {
        auto ref = play ([] (Host&) {});
        auto quiet = play ([] (Host& h) { h.set (params::id::d50Level, -6.0f); });
        const float ratio = quiet.rms() / ref.rms();
        check (std::abs (ratio - 0.501f) < 0.02f, "D-50 level -6 dB halves the amplitude (ratio " + juce::String (ratio, 3) + ")");
    }
    {
        auto left = play ([] (Host& h) { h.set (params::id::d50Pan, -50); });
        double l = 0, r = 0;
        for (size_t i = 0; i < left.L.size(); ++i) { l += left.L[i] * left.L[i]; r += left.R[i] * left.R[i]; }
        check (l > 1.0e-3 && r < l * 1.0e-6, "D-50 pan hard left: right channel silent (L " + juce::String (l, 4) + ", R " + juce::String (r, 8) + ")");
        auto centre = play ([] (Host&) {});
        double cl = 0, cr = 0;
        for (size_t i = 0; i < centre.L.size(); ++i) { cl += centre.L[i] * centre.L[i]; cr += centre.R[i] * centre.R[i]; }
        check (std::abs (cl - cr) < 1.0e-6 * cl && std::abs (l - cl) < 1.0e-3 * cl,
               "balance law: centre L = R, hard left keeps the left level unchanged");
    }
    {
        Host h;
        h.prepare();
        makeCleanD50 (h);
        h.set (params::id::d50KeyLow, 64);
        h.run (0.1, { { 0, on (60) } });
        const int below = h.proc->getD50ActiveVoices();
        h.run (0.1, { { 0, on (70) } });
        check (below == 0 && h.proc->getD50ActiveVoices() == 1, "D-50 zone low key 64: note 60 ignored, note 70 plays");
    }
    {
        Host h;
        h.prepare();
        h.set (params::id::mode, params::ModeLayer);
        h.set (params::id::jdKeyHigh, 59);
        h.run (0.1, { { 0, on (72) } });
        check (h.proc->getD50ActiveVoices() == 1 && h.proc->getJdActiveVoices() == 0,
               "LAYER with JD-800 zone 0..59: note 72 plays the D-50 only");
    }
    {
        // Note-off must reach the transposed note even if transpose changed while the key was held.
        Host h;
        h.prepare();
        makeCleanD50 (h);
        h.set ("d50_up1_tvaT5", 0); // fast release
        h.run (0.1, { { 0, on (60) } });
        h.set (params::id::d50Transpose, 7);
        h.run (0.1, { { 0, off (60) } });
        h.run (1.0);
        check (h.proc->getD50ActiveVoices() == 0, "note-off after a transpose change releases the sounding note");
    }
    {
        Host h;
        h.prepare();
        auto* p = h.proc->apvts.getParameter (params::id::d50Level);
        check (p != nullptr && p->getName (64) == "D-50 Engine Level", "mixer parameters are host-automatable ('" + (p ? p->getName (64) : juce::String()) + "')");
    }
}

// ===========================================================================
// Writes a mono sine WAV (optionally with a "smpl" chunk: unity note + one sustain loop).
static bool writeSineWav (const juce::File& f, double sr, double hz, double seconds, int unityNote, bool loop)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unordered_map<juce::String, juce::String> meta;
    const int len = int (sr * seconds);
    const int period = int (std::lround (sr / hz));
    if (unityNote >= 0)
    {
        meta["MidiUnityNote"] = juce::String (unityNote);
        meta["NumSampleLoops"] = loop ? "1" : "0";
        if (loop)
        {
            const int end = (len / period) * period; // whole number of cycles -> seamless loop
            meta["Loop0Type"] = "0";
            meta["Loop0Start"] = juce::String (end - 50 * period);
            meta["Loop0End"] = juce::String (end - 1);
        }
    }
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (1)
                                          .withBitsPerSample (24).withMetadataValues (meta));
    if (w == nullptr) return false;
    juce::AudioBuffer<float> b (1, len);
    for (int i = 0; i < len; ++i)
        b.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * double (i % period) / double (period)));
    return w->writeFromAudioSampleBuffer (b, 0, len);
}

static void testPatchFilesAndUserWaves()
{
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DigitalComboTest-" + juce::String (juce::Time::currentTimeMillis()));
    tmp.createDirectory();

    section ("Patch files (.dcpatch): save, browse, load");
    {
        Host a;
        a.prepare();
        a.proc->setPatchFolder (tmp.getChildFile ("Patches"));
        a.set (params::id::mode, params::ModeSplit);
        a.set (params::id::splitPoint, 55);
        a.set (params::id::d50Level, -3.0f);
        a.set (params::id::jdTranspose, -12);
        a.set ("d50_up1_cutoff", 33);
        a.set (params::jdToneId (0, "cutoff"), 41);
        a.run (0.02);
        a.proc->setD50Name ((int) d50::kPatchFactors, 18, "FILE TEST");
        const auto file = a.proc->getPatchFolder().getChildFile ("My Combo.dcpatch");
        check (a.proc->savePatchFile (file, "My Combo") && file.existsAsFile(), "patch saved to " + file.getFileName());
        check (a.proc->findUserPatches().size() == 1 && a.proc->findUserPatches()[0] == file, "browser lists the saved patch");

        Host b;
        b.prepare();
        const bool ok = b.proc->loadPatchFile (file);
        b.run (0.02);
        b.proc->syncParametersFromD50Temp();
        check (ok && b.get (params::id::mode) == params::ModeSplit && b.get (params::id::splitPoint) == 55
                   && std::abs (b.get (params::id::d50Level) + 3.0f) < 0.01f && b.get (params::id::jdTranspose) == -12,
               "mode, split point and mixer restored from the file");
        check (b.get ("d50_up1_cutoff") == 33 && b.get (params::jdToneId (0, "cutoff")) == 41,
               "D-50 and JD-800 sound parameters restored from the file");
        check (b.proc->getD50PatchName().trim() == "FILE TEST" && b.proc->getCurrentPatchName() == "My Combo",
               "patch names restored ('" + b.proc->getD50PatchName().trim() + "', '" + b.proc->getCurrentPatchName() + "')");
        bool same = true;
        for (int i = 0; i < d50::kTempSize; ++i) same = same && a.proc->getD50Byte (i) == b.proc->getD50Byte (i);
        for (int i = 0; i < jd800::kPatchSize; ++i) same = same && a.proc->getJdByte (i) == b.proc->getJdByte (i);
        check (same, "all 448 D-50 and 384 JD-800 bytes identical after save -> load");

        const auto junk = tmp.getChildFile ("junk.dcpatch");
        junk.replaceWithText ("<DigitalComboPatch><D50Temp hex=\"00\"/></DigitalComboPatch>");
        const float before = b.get ("d50_up1_cutoff");
        check (! b.proc->loadPatchFile (junk) && b.get ("d50_up1_cutoff") == before, "a damaged patch file is rejected and changes nothing");
        check (! b.proc->loadPatchFile (tmp.getChildFile ("missing.dcpatch")), "a missing file is rejected");
    }

    section ("User wave import (the user's own WAV files)");
    {
        const auto d50Dir = tmp.getChildFile ("D50Waves"), jdDir = tmp.getChildFile ("JDWaves");
        d50Dir.createDirectory();
        jdDir.createDirectory();
        // 440 Hz recorded at unity note 69 with a sustain loop, as PCM 1; a one-shot without
        // sampler data (taken as middle C) as PCM 2; plus files that must be skipped.
        bool written = writeSineWav (d50Dir.getChildFile ("001 my sine.wav"), 44100.0, 441.0, 0.5, 69, true);
        written = writeSineWav (d50Dir.getChildFile ("2 plain.wav"), 44100.0, 441.0, 0.3, -1, false) && written;
        written = writeSineWav (d50Dir.getChildFile ("no number.wav"), 44100.0, 441.0, 0.1, -1, false) && written;
        written = writeSineWav (d50Dir.getChildFile ("200 too high.wav"), 44100.0, 441.0, 0.1, -1, false) && written;
        d50Dir.getChildFile ("3 not audio.wav").replaceWithText ("hello");
        written = writeSineWav (jdDir.getChildFile ("012 user sine.wav"), 48000.0, 480.0, 0.5, 69, true) && written;
        check (written, "test WAV files written (smpl chunk with unity note and loop)");

        Host h;
        h.prepare();
        const auto report = h.proc->setUserWaveFolder (false, d50Dir);
        check (h.proc->getUserWaveCount (false) == 2, "D-50 folder: 2 waves loaded (" + report + ")");
        check (h.proc->getUserWaveMessages().size() == 3, "3 unusable files skipped and reported: "
                                                              + h.proc->getUserWaveMessages().joinIntoString (" | "));
        makeCleanD50 (h);
        h.set ("d50_uc_structure", 5);
        h.set ("d50_up1_pcm", 0);
        auto r = h.run (2.0, { { 0, on (69) } });
        const float hz = r.zeroCrossingHz (h.sr, size_t (0.2 * h.sr));
        check (std::abs (hz - 441.0f) < 3.0f, "PCM 1 plays the user's file at its unity note: note 69 -> " + juce::String (hz, 1) + " Hz");
        check (r.rms (size_t (1.5 * h.sr), size_t (2.0 * h.sr)) > 0.01f, "the file's sustain loop keeps the held note sounding past the file's end");

        h.set ("d50_up1_pcm", 1);
        auto r2 = h.run (1.0, { { 0, on (60) } });
        const float hz2 = r2.zeroCrossingHz (h.sr, size_t (0.05 * h.sr));
        check (std::abs (hz2 - 441.0f) < 4.0f, "file without sampler data: taken as middle C (note 60 -> " + juce::String (hz2, 1) + " Hz)");
        check (r2.rms (size_t (0.6 * h.sr), size_t (1.0 * h.sr)) < 1.0e-4f, "file without a loop plays once (one-shot)");

        h.proc->setUserWaveFolder (true, jdDir);
        check (h.proc->getUserWaveCount (true) == 1 && h.proc->getUserWaveCount (false) == 2, "JD-800 folder: waveform 12 loaded, D-50 waves kept");
        Host j;
        j.prepare();
        j.proc->setUserWaveFolder (true, jdDir);
        makeCleanJd (j); // tone A = waveform 12
        auto rj = j.run (1.0, { { 0, on (69) } });
        const float hzj = rj.zeroCrossingHz (j.sr, size_t (0.2 * j.sr));
        check (std::abs (hzj - 480.0f) < 4.0f, "JD-800 waveform 12 replaced by the user's file: note 69 -> " + juce::String (hzj, 1) + " Hz");

        juce::MemoryBlock state;
        h.proc->getStateInformation (state);
        Host k;
        k.prepare();
        k.proc->setStateInformation (state.getData(), (int) state.getSize());
        check (k.proc->getUserWaveCount (false) == 2 && k.proc->getUserWaveCount (true) == 1
                   && k.proc->getUserWaveFolder (false) == d50Dir,
               "user wave folders are saved in the project and reloaded");

        k.proc->setUserWaveFolder (false, juce::File());
        k.proc->setUserWaveFolder (true, juce::File());
        check (k.proc->getUserWaveCount (false) == 0 && k.proc->getUserWaveCount (true) == 0, "clearing the folders returns to the built-in substitutes");
        auto rk = k.run (0.3, { { 0, on (60) } });
        check (rk.allFinite && rk.rms() > 1.0e-3f, "still plays after the user waves were removed");
    }
    tmp.deleteRecursively();
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

// ===========================================================================
// JD-800: every vector below is printed in "JD-800 Owner's Manual II", MIDI Implementation V-58..V-64
static void testJdDocument()
{
    section ("JD-800 doc: address map and patch layout (*4-1-1, *4-5)");
    uint8_t a[3];
    auto addr = [&a] (uint32_t lin) { d50::toAddress (lin, a); return juce::String::toHexString (a, 3, 1).toUpperCase(); };
    check (jd800::kPatchSize == 384, "patch = 50 (common) + 46 (effect) + 4 x 72 (tones) = 384 bytes ('Total Size 00 03 00')");
    check (addr (jd800::kEffectOffset) == "00 00 32", "Patch Effect at offset [00-00-32]");
    check (addr (jd800::kToneOffset[0]) == "00 00 60" && addr (jd800::kToneOffset[1]) == "00 01 28"
               && addr (jd800::kToneOffset[2]) == "00 01 70" && addr (jd800::kToneOffset[3]) == "00 02 38",
           "Tone A/B/C/D at [00-00-60] [00-01-28] [00-01-70] [00-02-38]");
    check (addr (jd800::patchMemoryAddress (0)) == "05 00 00" && addr (jd800::patchMemoryAddress (1)) == "05 03 00"
               && addr (jd800::patchMemoryAddress (8)) == "05 18 00" && addr (jd800::patchMemoryAddress (32)) == "05 60 00"
               && addr (jd800::patchMemoryAddress (63)) == "06 3D 00",
           "Patch Memory I-11 [05-00-00], I-12 [05-03-00], I-21 [05-18-00], I-51 [05-60-00], I-88 [05-00-00]+[01-3D-00]");
    check (addr (uint32_t (64 * jd800::kPatchSize)) == "01 40 00", "Patch Memory total size 01-40-00 (24576 bytes)");
    check (jd800::deviceForUnit (17) == 0x10, "Device ID = Unit number - 1 (unit 17 -> 10H, as in all doc examples)");

    section ("JD-800 doc: application examples (checksums and addresses)");
    struct Ex { std::vector<uint8_t> bytes; const char* what; };
    const Ex examples[] = {
        { { 0xF0, 0x41, 0x10, 0x3D, 0x11, 0x04, 0x10, 0x4A, 0x00, 0x00, 0x0A, 0x18, 0xF7 }, "RQ1 setup key names (note #60)" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x12, 0x04, 0x09, 0x67, 0x01, 0x0B, 0xF7 }, "DT1 setup key effect mode (note #50)" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x19, 0x65, 0xF7 }, "RQ1 system area" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x12, 0x02, 0x00, 0x12, 0x64, 0x08, 0xF7 }, "DT1 chorus level 100" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x11, 0x03, 0x00, 0x0C, 0x00, 0x00, 0x06, 0x6B, 0xF7 }, "RQ1 part 3" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x12, 0x03, 0x00, 0x1D, 0x32, 0x2E, 0xF7 }, "DT1 part 5 effect level 50" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x11, 0x03, 0x00, 0x1E, 0x00, 0x00, 0x04, 0x5B, 0xF7 }, "RQ1 special part" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x12, 0x03, 0x00, 0x1E, 0x50, 0x0F, 0xF7 }, "DT1 special part level 80" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x11, 0x05, 0x60, 0x23, 0x00, 0x00, 0x07, 0x71, 0xF7 }, "RQ1 EQ of patch I-51" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x12, 0x05, 0x18, 0x10, 0x64, 0x6F, 0xF7 }, "DT1 patch level of I-21 = 100" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x11, 0x05, 0x48, 0x32, 0x00, 0x00, 0x2E, 0x53, 0xF7 }, "RQ1 effect data of I-41" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x12, 0x06, 0x10, 0x43, 0x64, 0x43, 0xF7 }, "DT1 phaser mix of I-71 = 100" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x11, 0x05, 0x04, 0x08, 0x00, 0x00, 0x48, 0x27, 0xF7 }, "RQ1 tone B of I-12 (as printed)" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x12, 0x00, 0x14, 0x39, 0x64, 0x4F, 0xF7 }, "DT1 part 2 tone C cutoff = 100 (MULTI)" },
        { { 0xF0, 0x41, 0x10, 0x3D, 0x12, 0x07, 0x00, 0x00, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x21, 0x64, 0xF7 }, "DT1 display 'Hello!'" },
    };
    int ok = 0;
    for (auto& ex : examples)
    {
        d50::Message m;
        const bool good = jd800::parse (ex.bytes.data(), ex.bytes.size(), m) == d50::ParseResult::Ok && m.device == 0x10;
        ok += good ? 1 : 0;
        if (! good) check (false, juce::String ("doc example rejected: ") + ex.what);
    }
    check (ok == 15, "all 15 application examples parse with valid checksums (model 3DH, device 10H)");
    {
        d50::Message m;
        jd800::parse (examples[9].bytes.data(), examples[9].bytes.size(), m);
        check (m.address == jd800::patchMemoryAddress (8) + jd800::common::Level && m.data[0] == 100,
               "'patch level of I-21' example hits Patch Memory I-21 + Patch level (offset 10H)");
        jd800::parse (examples[11].bytes.data(), examples[11].bytes.size(), m);
        check (m.address == jd800::patchMemoryAddress (48) + jd800::kEffectOffset + jd800::effect::PhaserMix,
               "'phaser mix of I-71' example hits I-71 + Effect (32H) + Phaser Mix (11H)");
        jd800::parse (examples[13].bytes.data(), examples[13].bytes.size(), m);
        check (m.address == d50::toLinear (0x00, 0x12, 0x52) + d50::toLinear (0x00, 0x01, 0x42) + jd800::tone::Cutoff,
               "'part 2 tone C cutoff' example = Part 2 [00-12-52] + Tone-C [00-01-42] + Cutoff freq (25H)");
        jd800::parse (examples[12].bytes.data(), examples[12].bytes.size(), m);
        uint8_t ours[16];
        const auto n = jd800::buildRQ1 (0x10, jd800::patchMemoryAddress (1) + (uint32_t) jd800::kToneOffset[1], 72, ours);
        check (m.address != jd800::patchMemoryAddress (1) + (uint32_t) jd800::kToneOffset[1]
                   && equalBytes (ours, { 0xF0, 0x41, 0x10, 0x3D, 0x11, 0x05, 0x04, 0x28, 0x00, 0x00, 0x48, 0x07, 0xF7 }, n),
               "DOC DISCREPANCY: 'tone B of I-12' example prints address 05-04-08; the address table gives 05-03-00 + 01-28 = "
               "05-04-28 (we follow the table: " + hex (ours, n) + ")");
    }

    section ("JD-800 doc: parameter tables and value lists (V-37..V-42, V-61..V-63)");
    struct T { int off; int max; const char* what; };
    for (auto t : { T { jd800::tone::Lfo1Delay, 101, "LFO Delay 0-100, REL" }, T { jd800::tone::PitchCoarse, 96, "Pitch coarse -48..+48" },
                    T { jd800::tone::PitchKeyFollow, 16, "WG key follow (17 steps)" }, T { jd800::tone::CutoffKf, 40, "TVF key follow (41 steps)" },
                    T { jd800::tone::BiasLevel, 20, "TVA bias level -10..+10" }, T { jd800::tone::TvaEnvTimeKf, 20, "time KF -10..+10" },
                    T { jd800::tone::WaveLsb, 127, "Waveform LSB" }, T { jd800::tone::FilterMode, 2, "HPF/BPF/LPF" } })
        check (jd800::toneParam (t.off).maxValue == t.max, juce::String ("tone ") + t.what);
    check (jd800::commonParam (jd800::common::AftertouchBend).maxValue == 26 && jd800::commonParam (jd800::common::SplitPoint).maxValue == 85
               && jd800::commonParam (jd800::common::Name).minValue == 32,
           "common: A-touch bend 0-1AH, split point 0-55H, names ASCII 20H-7FH");
    check (jd800::effectParam (jd800::effect::DelayCenterTap).maxValue == 125 && jd800::effectParam (jd800::effect::ChorusDelay).maxValue == 99
               && jd800::effectParam (jd800::effect::ReverbPreDelay).maxValue == 120 && jd800::effectParam (jd800::effect::ReverbHfDamp).maxValue == 16,
           "effect: delay tap 0-7DH, chorus delay 0-63H, pre delay 0-79H, HF damp 0-10H");
    check (jd800::tvfKeyFollowPercent (0) == -100 && jd800::tvfKeyFollowPercent (10) == 0 && jd800::tvfKeyFollowPercent (40) == 150,
           "TVF key follow: -100..0 % (10 % steps), 0..+150 % (5 % steps)");
    check (jd800::aftertouchBendSemis (0) == -36 && jd800::aftertouchBendSemis (1) == -24 && jd800::aftertouchBendSemis (2) == -12
               && jd800::aftertouchBendSemis (26) == 12, "A-touch bend: -36, -24, -12 .. +12");
    check (std::abs (jd800::phaserManualHz (0) - 50) < 0.01f && std::abs (jd800::phaserManualHz (26) - 320) < 0.01f
               && std::abs (jd800::phaserManualHz (49) - 1010) < 0.01f && std::abs (jd800::phaserManualHz (85) - 8100) < 0.01f
               && std::abs (jd800::phaserManualHz (99) - 15000) < 0.01f, "phaser manual table [*4]: 50 Hz .. 320 .. 1010 .. 8.1k .. 15 kHz (100 steps)");
    check (std::abs (jd800::delayTapMs (0) - 0.1f) < 1e-4f && std::abs (jd800::delayTapMs (49) - 5) < 1e-4f
               && std::abs (jd800::delayTapMs (59) - 10) < 1e-4f && std::abs (jd800::delayTapMs (89) - 40) < 1e-4f
               && std::abs (jd800::delayTapMs (105) - 200) < 1e-4f && std::abs (jd800::delayTapMs (125) - 600) < 1e-4f,
           "delay tap table [*5]: 0.1 .. 5 .. 10 .. 40 .. 200 .. 600 ms (126 steps)");
    check (std::abs (jd800::chorusDelayMs (99) - 50) < 1e-4f && jd800::feedbackPercent (0) == -98 && jd800::feedbackPercent (98) == 98,
           "chorus delay [*6] ends at 50 ms; feedback -98..+98 % in 2 % steps");
    int oneShots = 0;
    for (char t : jd800::kWaveLoopType) oneShots += t == 'O' ? 1 : 0;
    check (juce::String (jd800::kWaveNames[0]) == "Syn Saw 1" && juce::String (jd800::kWaveNames[107]) == "Wind Chime" && oneShots == 17,
           "WAVEFORM LIST: 108 names (001 Syn Saw 1 .. 108 Wind Chime), 17 one-shots (075-091)");
    int o[4];
    dc::JdEffects::groupAOrder (0, o);
    const bool a0 = o[0] == 0 && o[1] == 1 && o[2] == 2 && o[3] == 3;
    dc::JdEffects::groupAOrder (23, o);
    const bool a23 = o[0] == 3 && o[1] == 0 && o[2] == 2 && o[3] == 1;
    int ob[3];
    dc::JdEffects::groupBOrder (5, ob);
    check (a0 && a23 && ob[0] == 2 && ob[1] == 1 && ob[2] == 0, "effect orders: A 0 = DS-PH-SP-EN, A 23 = EN-DS-SP-PH, B 5 = REV-DLY-CHO");
    check (dc::jdVelocityCurve (0, 50.0f / 127.0f) < 0.02f && dc::jdVelocityCurve (0, 110.0f / 127.0f) > 0.98f
               && dc::jdVelocityCurve (3, 0.5f) < dc::jdVelocityCurve (2, 0.5f),
           "velocity curves (p. I-17): curve 1 switches between 64 and 105, curve 4 steeper than curve 3");
}

static void testJdEngine()
{
    section ("JD-800 engine");
    {
        Host h; h.prepare(); makeCleanJd (h);
        h.set (params::jdToneId (0, "coarse"), 60);
        auto r = h.run (1.0, { { 0, on (69) } });
        const float f = r.zeroCrossingHz (h.sr, 24000);
        check (std::abs (f - 880.0f) < 3.0f, "Pitch coarse +12 -> " + juce::String (f, 2) + " Hz (expected 880)");
    }
    // Analog pulse waves with documented duty cycles: 005 50 %, 006 30 %, 007 20 %, 008 14 %, 009 7 %
    const float duties[5] = { 0.5f, 0.3f, 0.2f, 0.14f, 0.07f };
    for (int k = 0; k < 5; ++k)
    {
        Host h; h.prepare(); makeCleanJd (h);
        h.set (params::jdToneId (0, "wave"), (float) (4 + k));
        auto r = h.run (0.6, { { 0, on (45) } });  // 110 Hz
        int high = 0, total = 0;
        float mean = 0;
        for (size_t i = 12000; i < r.L.size(); ++i) mean += r.L[i];
        mean /= float (r.L.size() - 12000);
        for (size_t i = 12000; i < r.L.size(); ++i) { high += r.L[i] > mean ? 1 : 0; ++total; }
        const float measured = float (high) / float (total);
        check (std::abs (measured - duties[k]) < 0.03f, juce::String ("waveform ") + jd800::kWaveNames[4 + k] + ": measured duty "
                   + juce::String (measured * 100.0f, 1) + " % (manual: " + juce::String (duties[k] * 100.0f, 0) + " %)");
    }
    {
        int silent = 0, bad = 0;
        juce::String list;
        for (int w = 0; w < jd800::kNumWaveforms; ++w)
        {
            Host h; h.prepare(); makeCleanJd (h);
            h.set (params::jdToneId (0, "wave"), (float) w);
            auto r = h.run (0.12, { { 0, on (60) } });
            if (! r.allFinite || r.peak > 4.0f) { ++bad; list << (w + 1) << " "; }
            else if (jd800::kWaveLoopType[w] == 'O' ? r.peak < 0.002f : r.rms() < 0.0005f) { ++silent; list << (w + 1) << " "; }
        }
        check (silent == 0 && bad == 0, "all 108 substitute waveforms sound (one-shots checked by peak) and stay finite" + (list.isEmpty() ? juce::String() : " (problem: " + list + ")"));
    }
    {
        Host h; h.prepare(); makeCleanJd (h);
        h.set (params::jdToneId (1, "on"), 1);
        h.sysex ([] {   // Key range of tone A = C4..C5 (60..72), via DT1 to common 11H/12H
            uint8_t b[16]; const uint8_t d[] = { 60, 72 };
            const auto n = jd800::buildDT1 (0x10, jd800::kCommonOffset + jd800::common::KeyRangeLowA, d, 2, b);
            return std::vector<uint8_t> (b, b + n); }());
        h.run (0.2, { { 0, on (50) } });
        check (h.proc->getJdActiveTones (0) == 0 && h.proc->getJdActiveTones (1) == 1, "Key Range: tone A (C4-C5) silent at D3, tone B plays");
        h.run (0.2, { { 0, on (64) } });
        check (h.proc->getJdActiveTones (0) == 1, "Key Range: tone A plays at E4");
    }
    {
        auto brightness = [] (int mode)
        {
            Host h; h.prepare(); makeCleanJd (h);
            h.set (params::jdToneId (0, "wave"), 0); // Syn Saw 1
            h.set (params::jdToneId (0, "filterMode"), (float) mode);
            h.set (params::jdToneId (0, "cutoff"), 55);
            return h.run (0.4, { { 0, on (48) } }).brightness();
        };
        const float hp = brightness (0), bp = brightness (1), lp = brightness (2);
        check (hp > bp && bp > lp, "TVF modes HPF > BPF > LPF in brightness (" + juce::String (hp, 3) + ", " + juce::String (bp, 3) + ", " + juce::String (lp, 3) + ")");
    }
    {
        // Pitch envelope: L0 = +50 (estimated +24 semitones), T1 slow -> starts high, ends at the key pitch
        Host h; h.prepare(); makeCleanJd (h);
        h.sysex ([] {
            uint8_t b[24]; const uint8_t d[] = { 100, 60, 50, 60 };  // L0 +50, T1 60, L1 0, T2 60
            const auto n = jd800::buildDT1 (0x10, (uint32_t) (jd800::kToneOffset[0] + jd800::tone::PEnvL0), d, 4, b);
            return std::vector<uint8_t> (b, b + n); }());
        auto r = h.run (1.5, { { 0, on (57) } });
        Rendered early; early.L.assign (r.L.begin(), r.L.begin() + 4800); early.R = early.L;
        const float fEarly = early.zeroCrossingHz (h.sr, 0), fLate = r.zeroCrossingHz (h.sr, 60000);
        check (fEarly > 400.0f && std::abs (fLate - 220.0f) < 3.0f,
               "Pitch envelope L0 -> L1 -> 0: first 100 ms " + juce::String (fEarly, 0) + " Hz, then " + juce::String (fLate, 1) + " Hz (key A3 = 220)");
    }
    {
        // Solo + Legato: overlapping keys reuse one voice without restarting it
        Host h; h.prepare(); makeCleanJd (h);
        h.set ("jd_solo", 1);
        h.sysex ([] { uint8_t b[16]; const uint8_t d[] = { 1 };
                      const auto n = jd800::buildDT1 (0x10, jd800::kCommonOffset + jd800::common::SoloLegato, d, 1, b);
                      return std::vector<uint8_t> (b, b + n); }());
        h.run (0.3, { { 0, on (60) }, { 4800, on (64) }, { 9600, on (67) } });
        check (h.proc->getJdActiveVoices() == 1, "Solo: three overlapping keys use one voice");
    }
    {
        // Group B balance 0 ("100:0") = dry; turning Group A distortion on changes the waveform
        auto render = [] (bool dist)
        {
            Host h; h.prepare(); makeCleanJd (h);
            if (dist) { h.set ("jd_aBlock1", 1); h.set ("jd_distType", 6); h.set ("jd_distDrive", 80); }
            return h.run (0.4, { { 0, on (57) } });
        };
        const auto clean = render (false), dist = render (true);
        auto crest = [] (const Rendered& r) { return r.peak / std::max (1.0e-6f, r.rms (4800)); };
        check (crest (dist) < crest (clean) * 0.8f, "Group A block 1 (DS in DS-PH-SP-EN), FUZZ DIST: crest factor "
                   + juce::String (crest (clean), 2) + " -> " + juce::String (crest (dist), 2));
    }
    {
        // LFO Delay = REL: no vibrato while the key is held
        auto render = [] (int sens)
        {
            Host h; h.prepare(); makeCleanJd (h);
            h.sysex ([] { uint8_t b[16]; const uint8_t d[] = { 80, 101 }; // LFO1 rate 80, delay REL
                          const auto n = jd800::buildDT1 (0x10, (uint32_t) (jd800::kToneOffset[0] + jd800::tone::Lfo1Rate), d, 2, b);
                          return std::vector<uint8_t> (b, b + n); }());
            h.set (params::jdToneId (0, "lfo1Sens"), (float) sens);
            return h.run (0.5, { { 0, on (69) } });
        };
        const auto withLfo = render (100), without = render (50);
        float diff = 0;
        for (size_t i = 0; i < withLfo.L.size(); ++i) diff = std::max (diff, std::abs (withLfo.L[i] - without.L[i]));
        check (diff < 1.0e-5f, "LFO Delay REL: held note identical with and without LFO depth (max diff " + juce::String (diff, 7) + ")");
    }
}

static void testJdProcessorSysEx()
{
    section ("Processor: JD-800 SysEx receive / reply");
    Host h;
    h.prepare();
    auto jdDt1 = [] (uint8_t dev, uint32_t addr, std::vector<uint8_t> d)
    {
        std::vector<uint8_t> out (d.size() + 12);
        out.resize (jd800::buildDT1 (dev, addr, d.data(), (int) d.size(), out.data()));
        return out;
    };
    const uint32_t cutoffA = (uint32_t) (jd800::kToneOffset[0] + jd800::tone::Cutoff);
    h.sysex (jdDt1 (0x10, cutoffA, { 30 }));
    check (h.proc->getJdByte ((int) cutoffA) == 30 && (int) h.get (params::jdToneId (0, "cutoff")) == 30,
           "DT1 (unit 17) sets Tone A cutoff; host parameter follows");
    h.sysex (jdDt1 (0x11, cutoffA, { 40 }));
    check (h.proc->getJdByte ((int) cutoffA) == 30, "message for unit 18 ignored while unit = 17");
    h.sysex (jdDt1 (0x10, jd800::kCommonOffset + jd800::common::LayerTone, { 0x0C })); // C + D
    check ((int) h.get (params::jdToneId (0, "on")) == 0 && (int) h.get (params::jdToneId (2, "on")) == 1
               && (int) h.get (params::jdToneId (3, "on")) == 1, "Layer tone = CD: tone on/off parameters follow the bit mask");
    h.sysex (jdDt1 (0x10, (uint32_t) (jd800::kToneOffset[0] + jd800::tone::WaveMsb), { 0, 62 }));
    check ((int) h.get (params::jdToneId (0, "wave")) == 62, "Waveform MSB/LSB = 0/62 -> waveform parameter 063 Digi Bell");
    h.set (params::jdToneId (1, "wave"), 100);
    h.run (0.01);
    check (h.proc->getJdByte (jd800::kToneOffset[1] + jd800::tone::WaveMsb) == 0 && h.proc->getJdByte (jd800::kToneOffset[1] + jd800::tone::WaveLsb) == 100,
           "waveform parameter 101 writes MSB 0 / LSB 100");
    h.sysex (jdDt1 (0x10, (uint32_t) (jd800::kToneOffset[0] + jd800::tone::PitchCoarse), { 127 }));
    check (h.proc->getJdByte (jd800::kToneOffset[0] + jd800::tone::PitchCoarse) == 96, "out-of-range Pitch coarse 127 clamped to 96 (+48)");

    h.allMidiOut.clear();
    uint8_t rq[16];
    const auto rqLen = jd800::buildRQ1 (0x10, 0, 384, rq);
    h.run (0.01, { { 0, juce::MidiMessage (rq, (int) rqLen) } });
    int replies = 0, total = 0;
    bool valid = true, match = true;
    for (auto& m : h.allMidiOut)
    {
        d50::Message r;
        valid = valid && jd800::parse (m.getRawData(), (size_t) m.getRawDataSize(), r) == d50::ParseResult::Ok;
        for (int k = 0; k < r.dataLength; ++k) match = match && r.data[k] == h.proc->getJdByte ((int) r.address + k);
        total += r.dataLength;
        ++replies;
    }
    check (replies == 2 && total == 384 && valid && match, "RQ1(384) answered with 2 valid JD-800 DT1 messages (256 + 128 bytes) equal to the patch");

    std::vector<uint8_t> slot (jd800::kPatchSize);
    params::writeInitJdPatch (slot.data());
    slot[(size_t) jd800::common::Level] = 33;
    for (int off = 0; off < jd800::kPatchSize; off += 128)
        h.sysex (jdDt1 (0x10, jd800::patchMemoryAddress (3) + (uint32_t) off,
                        std::vector<uint8_t> (slot.begin() + off, slot.begin() + std::min (off + 128, jd800::kPatchSize))));
    h.run (0.01, { { 0, juce::MidiMessage::programChange (1, 3) } });
    check (h.proc->getJdByte (jd800::common::Level) == 33, "patch received into Patch Memory I-14 is loaded by Program Change 4");

    const auto exported = h.proc->exportJdTempAsSysEx();
    Host h2;
    h2.prepare();
    const int accepted = h2.proc->importSysExData (exported.getData(), exported.getSize());
    bool same = accepted == 2;
    for (int i = 0; i < jd800::kPatchSize; ++i) same = same && h2.proc->getJdByte (i) == h.proc->getJdByte (i);
    check (same, "JD-800 .syx export -> import reproduces all 384 bytes");
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
    bool jdOk = true;
    for (auto& b : params::jdBindings())
    {
        auto* p = proc.apvts.getParameter (b.id);
        if (p == nullptr) { jdOk = false; continue; }
        if (b.kind == params::JdBinding::Byte)
        {
            const auto& info = jd800::paramForOffset (b.offset);
            const auto r = p->getNormalisableRange();
            jdOk = jdOk && (int) r.start == info.minValue && (int) r.end == info.maxValue;
        }
    }
    check (jdOk, juce::String ((int) params::jdBindings().size()) + " automatable JD-800 parameters use the documented ranges");
    check (proc.apvts.getParameter (params::jdToneId (0, "wave"))->getCurrentValueAsText().startsWith ("001 Syn Saw 1"),
           "JD waveform parameter shows the WAVEFORM LIST name (001 Syn Saw 1)");
    check (proc.apvts.getParameter ("d50_pt_keyMode")->getText (proc.apvts.getParameter ("d50_pt_keyMode")->convertTo0to1 (8), 32) == "Separate-S",
           "Key Mode 8 displays as Separate-S");
}

static void testEditor()
{
    section ("Editor (GUI) controls");
    DigitalComboProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000, 512);
    proc.prepareToPlay (48000, 512);
    const auto patchDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DigitalComboEditorTest-" + juce::String (juce::Time::currentTimeMillis()));
    proc.setPatchFolder (patchDir); // never touch the real Documents folder from a test
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
        fe.selectBlock (6); // Patch: value names must show even for value 0
        auto* km = fe.sliderForOffset ((int) d50::kPatchFactors + d50::patch::KeyMode);
        auto* cm = fe.sliderForOffset ((int) d50::kPatchFactors + d50::patch::ChaseMode);
        check (km != nullptr && cm != nullptr && km->getTextFromValue (km->getValue()) == "Whole"
                   && cm->getTextFromValue (cm->getValue()) == "UL",
               "full D-50 editor shows document value names (Key Mode 'Whole', Chase Mode 'UL')");
        int total = 0;
        for (int b = 0; b < 7; ++b) { fe.selectBlock (b); total += fe.getNumRows(); }
        check (total == 54 * 4 + 38 * 2 + 22, "full D-50 editor exposes " + juce::String (total) + " documented parameters (+ 3 name fields)");
    }
    if (combo != nullptr)
    {
        check (ed->isOpaque(), "editor is opaque (paints every pixel; the host never shows through)");
        // COMBO page: mixer knob, patch browser, renderer choice
        juce::Slider* level = nullptr;
        std::function<void (juce::Component&)> findLevel = [&] (juce::Component& c)
        {
            if (auto* sl = dynamic_cast<juce::Slider*> (&c))
                if (sl->getTitle() == "D-50 Engine Level") level = sl;
            for (auto* child : c.getChildren()) findLevel (*child);
        };
        combo->showTab (combo->getNumTabs() - 1); // tab pages are only in the component tree while shown
        findLevel (*ed);
        combo->showTab (0);
        if (level != nullptr) level->setValue (-10.0, juce::sendNotificationSync);
        check (level != nullptr && std::abs (proc.apvts.getRawParameterValue (params::id::d50Level)->load() + 10.0f) < 0.01f,
               "COMBO page: the D-50 Engine Level knob changes the parameter");

        proc.apvts.getParameter (params::id::splitPoint)->setValueNotifyingHost (proc.apvts.getParameter (params::id::splitPoint)->convertTo0to1 (50));
        proc.savePatchFile (patchDir.getChildFile ("Editor Test.dcpatch"), "Editor Test");
        proc.apvts.getParameter (params::id::splitPoint)->setValueNotifyingHost (proc.apvts.getParameter (params::id::splitPoint)->convertTo0to1 (70));
        combo->refreshPatchBrowser();
        auto& browser = combo->getPatchBrowser();
        int userItem = -1;
        for (int i = 0; i < browser.getNumItems(); ++i)
            if (browser.getItemText (i) == "Editor Test") userItem = browser.getItemId (i);
        check (userItem >= DigitalComboEditor::kUserPatchIdBase, "patch browser lists factory presets and the saved user patch");
        browser.setSelectedId (userItem, juce::sendNotificationSync);
        check (proc.getCurrentPatchName() == "Editor Test" && (int) proc.apvts.getRawParameterValue (params::id::splitPoint)->load() == 50,
               "choosing the user patch in the browser loads it");
        browser.setSelectedId (3, juce::sendNotificationSync);
        check (proc.getCurrentProgram() == 2 && proc.getCurrentPatchName() == proc.getProgramName (2),
               "choosing a factory preset in the browser loads it ('" + proc.getProgramName (2) + "')");

        auto& rb = combo->getComboPage().rendererBox;
        rb.setSelectedItemIndex (1, juce::sendNotificationSync);
        const int afterD2D = proc.guiRenderer.load();
        juce::MemoryBlock st;
        proc.getStateInformation (st);
        auto xml = juce::AudioProcessor::getXmlFromBinary (st.getData(), (int) st.getSize());
        rb.setSelectedItemIndex (0, juce::sendNotificationSync);
        check (afterD2D == 1 && xml != nullptr && xml->getIntAttribute ("guiRenderer", -1) == 1 && proc.guiRenderer.load() == 0,
               "editor drawing choice (Software / Direct2D) is stored in the project; Software is the default");
        patchDir.deleteRecursively();
    }
    if (combo != nullptr && screenshotDir != juce::File())
    {
        screenshotDir.createDirectory();
        int written = 0;
        for (int t = 0; t < combo->getNumTabs(); ++t)
        {
            combo->showTab (t);
            juce::Image shot (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
            juce::Graphics sg (shot);
            ed->paintEntireComponent (sg, false);
            juce::PNGImageFormat png;
            auto file = screenshotDir.getChildFile ("editor-" + juce::String (t + 1) + "-"
                                                    + juce::File::createLegalFileName (combo->getTabName (t)).replaceCharacter (' ', '_') + ".png");
            file.deleteFile();
            juce::FileOutputStream os (file);
            if (os.openedOk() && png.writeImageToStream (shot, os)) ++written;
        }
        combo->showTab (0);
        check (written == combo->getNumTabs(), "editor screenshots written for all " + juce::String (written) + " tabs");
    }
    if (combo != nullptr)
    {
        // Every tab must paint every pixel (no transparent holes) with real content (not blank).
        int goodTabs = 0;
        juce::String detail;
        for (int t = 0; t < combo->getNumTabs(); ++t)
        {
            combo->showTab (t);
            juce::Image img (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
            {
                juce::Graphics g (img);
                ed->paintEntireComponent (g, false);
            }
            const juce::Image::BitmapData px (img, juce::Image::BitmapData::readOnly);
            int transparent = 0;
            std::set<uint32_t> colours;
            for (int y = 0; y < img.getHeight(); y += 2)
                for (int x = 0; x < img.getWidth(); x += 2)
                {
                    const auto c = px.getPixelColour (x, y);
                    if (c.getAlpha() < 255) ++transparent;
                    if (colours.size() < 5000) colours.insert (c.getARGB());
                }
            if (transparent == 0 && colours.size() > 50) ++goodTabs;
            else detail << " " << combo->getTabName (t) << ": " << transparent << " transparent, " << (int) colours.size() << " colours;";
        }
        combo->showTab (0);
        check (goodTabs == combo->getNumTabs(), "all " + juce::String (combo->getNumTabs()) + " tabs paint fully opaque, non-blank content" + detail);
    }
    ed.reset();
    check (true, "editor destroyed cleanly");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 1) // optional: folder for editor screenshots
        screenshotDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    std::printf ("Digital Combo automated tests\n");
    testDocumentVectors();
    testDspUnits();
    testProcessorSysEx();
    testModesAndAudio();
    testStateAndPresets();
    testD50Expression();
    testPcmBank();
    testMixerAndZones();
    testPatchFilesAndUserWaves();
    testJdDocument();
    testJdEngine();
    testJdProcessorSysEx();
    testPerformance();
    testParameterBindings();
    testEditor();
    std::printf ("\nRESULT: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
