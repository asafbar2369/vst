#include "../Source/PluginProcessor.h"
#include "../Source/Synth/FmVoice.h"
#include <cmath>

namespace vsx
{
class FmTests final : public juce::UnitTest
{
public:
    FmTests() : juce::UnitTest ("Six-operator FM", "VICE SYNTH X") {}

    static fm::Patch randomPatch (juce::Random& r)
    {
        fm::Patch p;
        for (auto& o : p.op)
        {
            for (auto& x : o.rate) x = (uint8_t) r.nextInt (100);
            for (auto& x : o.level) x = (uint8_t) r.nextInt (100);
            o.breakPoint = (uint8_t) r.nextInt (100); o.leftDepth = (uint8_t) r.nextInt (100); o.rightDepth = (uint8_t) r.nextInt (100);
            o.leftCurve = (uint8_t) r.nextInt (4); o.rightCurve = (uint8_t) r.nextInt (4); o.rateScale = (uint8_t) r.nextInt (8);
            o.ampModSens = (uint8_t) r.nextInt (4); o.velSens = (uint8_t) r.nextInt (8); o.outputLevel = (uint8_t) r.nextInt (100);
            o.fixedMode = (uint8_t) r.nextInt (2); o.coarse = (uint8_t) r.nextInt (32); o.fine = (uint8_t) r.nextInt (100); o.detune = (uint8_t) r.nextInt (15);
        }
        for (auto& x : p.pitchRate) x = (uint8_t) r.nextInt (100);
        for (auto& x : p.pitchLevel) x = (uint8_t) r.nextInt (100);
        p.algorithm = (uint8_t) r.nextInt (32); p.feedback = (uint8_t) r.nextInt (8); p.oscSync = (uint8_t) r.nextInt (2);
        p.lfoSpeed = (uint8_t) r.nextInt (100); p.lfoDelay = (uint8_t) r.nextInt (100); p.lfoPmd = (uint8_t) r.nextInt (100);
        p.lfoAmd = (uint8_t) r.nextInt (100); p.lfoSync = (uint8_t) r.nextInt (2); p.lfoWave = (uint8_t) r.nextInt (6);
        p.lfoPms = (uint8_t) r.nextInt (8); p.transpose = (uint8_t) r.nextInt (49);
        p.setName ("RND " + juce::String (r.nextInt (1000)));
        return p;
    }

    static std::vector<float> renderVoice (const fm::Patch& p, int note, int vel, double sr, double seconds, double releaseAt = -1.0)
    {
        fm::FmRenderer r;
        r.prepare (sr);
        r.setPatch (&p);
        r.noteOn (p, note, vel, (float) note, 1);
        std::vector<float> out ((size_t) (seconds * sr));
        for (size_t i = 0; i < out.size(); ++i)
        {
            if (releaseAt >= 0.0 && i == (size_t) (releaseAt * sr)) r.noteOff();
            out[i] = r.render();
        }
        return out;
    }

    static double frequency (const std::vector<float>& x, double sr, size_t from)
    {
        int crossings = 0; size_t first = 0, last = 0;
        for (size_t i = from + 1; i < x.size(); ++i)
            if (x[i - 1] <= 0.0f && x[i] > 0.0f)
            {
                const double t = (double) (i - 1) + x[i - 1] / (double) (x[i - 1] - x[i]);
                if (crossings == 0) first = (size_t) t;
                last = (size_t) t;
                ++crossings;
            }
        return crossings > 1 ? (crossings - 1) * sr / (double) (last - first) : 0.0;
    }

    static float rms (const std::vector<float>& x, size_t a, size_t b)
    {
        double s = 0.0;
        for (size_t i = a; i < b && i < x.size(); ++i) s += (double) x[i] * x[i];
        return (float) std::sqrt (s / std::max<size_t> (1, b - a));
    }

    void runTest() override
    {
        beginTest ("32 algorithms: carriers, topology and feedback match the published chart");
        {
            const int carriers[32] = { 2, 2, 2, 2, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 3, 3, 4, 4, 4, 5, 5, 3, 3, 3, 4, 4, 5, 6 };
            for (int a = 0; a < 32; ++a)
            {
                const auto& al = fm::algorithm (a);
                expectEquals (al.numCarriers(), carriers[a], "algorithm " + juce::String (a + 1));
                for (int i = 0; i < 6; ++i)
                {
                    // modulators always have a higher operator number (render order 6 -> 1)
                    expect ((al.modulators[(size_t) i] & ((1u << (i + 1)) - 1u)) == 0, "algorithm " + juce::String (a + 1));
                    const bool carrier = (al.carriers >> i) & 1;
                    bool modulates = false;
                    for (int j = 0; j < 6; ++j) modulates = modulates || ((al.modulators[(size_t) j] >> i) & 1);
                    expect (carrier != modulates, "algorithm " + juce::String (a + 1) + " OP" + juce::String (i + 1) + " is either a carrier or a modulator");
                }
            }
            expectEquals (fm::algorithm (3).feedbackSource, 3); expectEquals (fm::algorithm (3).feedbackTarget, 5);
            expectEquals (fm::algorithm (5).feedbackSource, 4); expectEquals (fm::algorithm (5).feedbackTarget, 5);
            expectEquals (fm::algorithm (31).feedbackSource, 5);
        }

        beginTest ("VCED, packed and hex round trips");
        {
            juce::Random r (11);
            for (int k = 0; k < 200; ++k)
            {
                auto p = randomPatch (r);
                expectEquals (p.clamp(), 0);
                const auto vced = p.toVced();
                expect (fm::Patch::fromVced (vced.data()) == p, "VCED round trip");
                const auto packed = p.toPacked();
                for (auto b : packed) expect (b < 128);
                expect (fm::Patch::fromPacked (packed.data()) == p, "packed round trip");
                p.opEnabled = (uint8_t) r.nextInt (64);
                fm::Patch h;
                expect (fm::Patch::fromHex (p.toHex(), h) && h == p, "hex round trip");
            }
            fm::Patch bad;
            expect (! fm::Patch::fromHex ("zz", bad));
            // out-of-range VCED bytes are clamped, never trusted
            std::array<uint8_t, fm::kVcedSize> junk {};
            junk.fill (127);
            const auto c = fm::Patch::fromVced (junk.data());
            expect (c.algorithm == 31 && c.feedback == 7 && c.op[0].coarse == 31 && c.op[0].detune == 14 && c.transpose == 48);
        }

        beginTest ("SysEx bank and single voice import, checksum validation, malformed files");
        {
            const auto& factory = fm::factoryVoices();
            expect (factory.size() >= 12);
            auto bank = fm::makeBankSysex (factory);
            expectEquals ((int) bank.getSize(), 4104);
            auto res = fm::parseSysex (bank);
            expect (res.ok && res.isBank && res.checksumOk && res.voices.size() == 32, res.error + res.warning);
            for (size_t i = 0; i < factory.size() && i < res.voices.size(); ++i) expect (res.voices[i] == factory[i], factory[i].getName());
            expectEquals (res.voices[0].getName(), juce::String ("TINE PIANO"));

            // corrupt one data byte: checksum mismatch is reported, voices still offered
            auto corrupt = bank;
            static_cast<uint8_t*> (corrupt.getData())[100] ^= 0x01;
            res = fm::parseSysex (corrupt);
            expect (res.ok && ! res.checksumOk && res.warning.contains ("checksum"));

            const auto single = fm::makeVoiceSysex (factory[3]);
            expectEquals ((int) single.getSize(), 163);
            res = fm::parseSysex (single);
            expect (res.ok && ! res.isBank && res.voices.size() == 1 && res.voices[0] == factory[3]);

            // two messages in one file
            juce::MemoryBlock both (bank);
            both.append (single.getData(), single.getSize());
            res = fm::parseSysex (both);
            expect (res.ok && res.voices.size() == 33);

            // raw 4096-byte bank without header
            juce::MemoryBlock raw (static_cast<const uint8_t*> (bank.getData()) + 6, 4096);
            res = fm::parseSysex (raw);
            expect (res.ok && res.voices.size() == 32 && res.warning.contains ("raw"));

            // truncated, empty, garbage, wrong manufacturer
            res = fm::parseSysex (juce::MemoryBlock (bank.getData(), 2000));
            expect (! res.ok);
            res = fm::parseSysex (juce::MemoryBlock());
            expect (! res.ok);
            juce::MemoryBlock garbage;
            juce::Random r (5);
            for (int i = 0; i < 5000; ++i) { const auto b = (char) r.nextInt (256); garbage.append (&b, 1); }
            res = fm::parseSysex (garbage);
            expect (! res.ok || res.voices.size() > 0); // never crashes; random data is rejected or clamped
            auto other = bank;
            static_cast<uint8_t*> (other.getData())[1] = 0x41;
            expect (! fm::parseSysex (other).ok, "non-Yamaha-format message rejected");

            const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("vsx-fm-bank", ".syx");
            tmp.replaceWithData (bank.getData(), bank.getSize());
            res = fm::loadSysexFile (tmp);
            expect (res.ok && res.voices.size() == 32);
            tmp.deleteFile();
            expect (! fm::loadSysexFile (tmp).ok);
        }

        beginTest ("Renderer: pitch, ratio, fixed frequency, transpose");
        {
            const double sr = 48000.0;
            auto p = fm::Patch::init();
            auto x = renderVoice (p, 69, 127, sr, 1.0);
            expectWithinAbsoluteError (frequency (x, sr, 4800), 440.0, 0.5);
            p.op[0].coarse = 2;
            x = renderVoice (p, 69, 127, sr, 1.0);
            expectWithinAbsoluteError (frequency (x, sr, 4800), 880.0, 1.0);
            p.op[0].coarse = 0; // ratio 0.5
            x = renderVoice (p, 69, 127, sr, 1.0);
            expectWithinAbsoluteError (frequency (x, sr, 4800), 220.0, 0.5);
            p.op[0].fixedMode = 1; p.op[0].coarse = 2; p.op[0].fine = 0; // fixed 100 Hz regardless of key
            for (int n : { 36, 60, 96 })
            {
                x = renderVoice (p, n, 127, sr, 1.0);
                expectWithinAbsoluteError (frequency (x, sr, 4800), 100.0, 0.5);
            }
            p = fm::Patch::init();
            p.transpose = 36; // +12
            x = renderVoice (p, 57, 127, sr, 1.0);
            expectWithinAbsoluteError (frequency (x, sr, 4800), 440.0, 0.5);
        }

        beginTest ("Renderer: envelopes, release, velocity, mute, feedback, modulation index");
        {
            const double sr = 48000.0;
            auto p = fm::Patch::init();
            p.op[0].rate = { 99, 50, 50, 60 };
            p.op[0].level = { 99, 99, 99, 0 };
            auto x = renderVoice (p, 60, 127, sr, 3.0, 1.0);
            expect (rms (x, 24000, 48000) > 0.3f, "sustains while held");
            expect (rms (x, (size_t) (2.6 * sr), (size_t) (3.0 * sr)) < 1.0e-3f, "decays after note-off");

            p.op[0].velSens = 7;
            const float loud = rms (renderVoice (p, 60, 127, sr, 0.5), 4800, 24000);
            const float soft = rms (renderVoice (p, 60, 30, sr, 0.5), 4800, 24000);
            expect (soft < loud * 0.25f, "velocity sensitivity: " + juce::String (soft) + " vs " + juce::String (loud));

            p = fm::Patch::init();
            p.opEnabled = 0x3E; // OP1 muted
            expect (rms (renderVoice (p, 60, 127, sr, 0.5), 4800, 24000) < 1.0e-6f, "muted carrier is silent");

            // a modulator adds harmonics; FM amount 0 removes them
            p = fm::Patch::init();
            p.op[1].outputLevel = 90; // OP2 modulates OP1 in algorithm 1
            auto spectralSpread = [&] (const std::vector<float>& y)
            {
                double d = 0.0, e = 0.0;
                for (size_t i = 1; i < y.size(); ++i) { d += (double) (y[i] - y[i - 1]) * (y[i] - y[i - 1]); e += (double) y[i] * y[i]; }
                return d / std::max (1.0e-12, e);
            };
            const auto mod = renderVoice (p, 60, 127, sr, 0.5);
            const auto pure = renderVoice (fm::Patch::init(), 60, 127, sr, 0.5);
            expect (spectralSpread (mod) > 4.0 * spectralSpread (pure), "modulation brightens");
            {
                fm::FmRenderer r;
                r.prepare (sr);
                r.setPatch (&p);
                r.noteOn (p, 60, 127, 60.0f, 1);
                r.setPitch (60.0f, 0.0f);
                std::vector<float> y (24000);
                for (auto& v : y) v = r.render();
                expect (spectralSpread (y) < 1.2 * spectralSpread (pure), "FM Amount 0 leaves the carrier sine");
            }
            p = fm::Patch::init();
            p.feedback = 7; // OP6 self-feedback is not routed to the carrier in algorithm 1 unless OP6 sounds
            p.algorithm = 31; p.op[5].outputLevel = 99; p.op[0].outputLevel = 0;
            const auto fb = renderVoice (p, 60, 127, sr, 0.5);
            p.feedback = 0;
            const auto noFb = renderVoice (p, 60, 127, sr, 0.5);
            expect (spectralSpread (fb) > 3.0 * spectralSpread (noFb), "feedback adds harmonics");
        }

        beginTest ("Every algorithm and factory voice renders finite, bounded audio at all notes");
        {
            const double sr = 96000.0;
            juce::Random r (2);
            for (int a = 0; a < 32; ++a)
            {
                auto p = randomPatch (r);
                p.algorithm = (uint8_t) a;
                for (auto& o : p.op) { o.level[0] = 99; o.rate[0] = 99; o.outputLevel = (uint8_t) (60 + r.nextInt (40)); }
                p.feedback = 7;
                for (int n : { 0, 36, 60, 96, 127 })
                {
                    const auto x = renderVoice (p, n, 127, sr, 0.2, 0.1);
                    float peak = 0.0f; bool finite = true;
                    for (auto v : x) { finite &= std::isfinite (v); peak = std::max (peak, std::abs (v)); }
                    expect (finite && peak < 4.0f, "algorithm " + juce::String (a + 1) + " note " + juce::String (n) + (finite ? " peak " + juce::String (peak) : juce::String (" non-finite")));
                }
            }
            for (const auto& v : fm::factoryVoices())
            {
                expect (v.getName().length() <= 10 && v.getName().isNotEmpty());
                const auto x = renderVoice (v, 60, 100, 48000.0, 1.5, 1.0);
                float peak = 0.0f;
                for (auto s : x) peak = std::max (peak, std::abs (s));
                expect (peak > 0.05f && peak < 2.5f, v.getName() + " peak " + juce::String (peak));
            }
        }

        beginTest ("Processor: FM oscillator mode renders, recalls and survives a voice change while playing");
        {
            auto proc = std::make_unique<ViceSynthProcessor>();
            proc->setRateAndBufferSizeDetails (48000.0, 256);
            proc->prepareToPlay (48000.0, 256);
            proc->getPatch().setFmVoice (0, fm::factoryVoices()[0]);
            proc->setParam (pid::O (0, pid::osc::Mode), (float) (int) OscMode::FM);
            proc->setParam (pid::O (1, pid::osc::On), 0.0f);
            proc->setParam (pid::O (2, pid::osc::On), 0.0f);
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            float peak = 0.0f; bool finite = true;
            for (int b = 0; b < 200; ++b)
            {
                buf.clear();
                if (b == 100) proc->getPatch().setFmVoice (0, fm::factoryVoices()[5]); // edit while the note sounds
                proc->processBlock (buf, midi);
                midi.clear();
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i) { finite &= std::isfinite (buf.getSample (ch, i)); peak = std::max (peak, std::abs (buf.getSample (ch, i))); }
            }
            expect (finite && peak > 0.01f, "FM oscillator output " + juce::String (peak));

            juce::MemoryBlock state;
            proc->getStateInformation (state);
            auto restored = std::make_unique<ViceSynthProcessor>();
            restored->setStateInformation (state.getData(), (int) state.getSize());
            expect (restored->getPatch().getFmVoice (0) == fm::factoryVoices()[5], "FM voice recalled from the project");
            expectEquals ((int) restored->getParam (pid::O (0, pid::osc::Mode)), (int) OscMode::FM);
            const auto spec = proc->getCurrentPatch();
            expect (spec.fmVoice[0] == fm::factoryVoices()[5].toHex(), "FM voice travels in presets");
            proc->releaseResources();
        }
    }
};

static FmTests fmTests;
} // namespace vsx
