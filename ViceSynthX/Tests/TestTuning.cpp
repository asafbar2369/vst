#include "../Source/Core/Tuning.h"
#include "../Source/PluginProcessor.h"
#include "../Source/AI/PatchOps.h"
#include <cmath>

/*
    Scala microtuning: .scl / .kbm parsing, the tuning table, and pitch through the real synth.
*/
namespace vsx
{
class TuningTests final : public juce::UnitTest
{
public:
    TuningTests() : juce::UnitTest ("Microtuning (Scala)", "VICE SYNTH X") {}

    static juce::String edo (int n)
    {
        juce::String s = "! " + juce::String (n) + "-edo.scl\n!\n" + juce::String (n) + " equal divisions of the octave\n " + juce::String (n) + "\n!\n";
        for (int k = 1; k <= n; ++k) s << " " << juce::String (1200.0 * k / n, 5) << "\n";
        return s;
    }

    // period (samples) by autocorrelation around the expected value, parabolic interpolation
    static double period (const juce::AudioBuffer<float>& b, int start, int len, double expected)
    {
        const float* x = b.getReadPointer (0);
        const int lo = (int) (expected * 0.9), hi = (int) (expected * 1.1) + 2;
        std::vector<double> c ((size_t) hi + 2, 0.0);
        int best = lo;
        for (int lag = lo - 1; lag <= hi + 1; ++lag)
        {
            double num = 0, e0 = 0, e1 = 0;
            for (int i = start; i < start + len; ++i) { num += x[i] * x[i + lag]; e0 += x[i] * x[i]; e1 += x[i + lag] * x[i + lag]; }
            c[(size_t) lag] = num / std::sqrt (e0 * e1 + 1e-30);
            if (lag >= lo && lag <= hi && c[(size_t) lag] > c[(size_t) best]) best = lag;
        }
        const double a = c[(size_t) best - 1], m = c[(size_t) best], d = c[(size_t) best + 1];
        return best + 0.5 * (a - d) / (a - 2 * m + d);
    }

    void runTest() override
    {
        beginTest ("Parsing: 12-TET identity, just intonation, 19-EDO, keyboard mapping, unmapped keys, errors");
        {
            tuning::Scale s;
            juce::String err;
            expect (tuning::parseScl (edo (12), s, err), err);
            const auto t = tuning::build (s, nullptr);
            float worst = 0.0f;
            for (int k = 0; k < 128; ++k) worst = std::max (worst, std::abs (t[(size_t) k] - (float) k));
            logMessage ("MEASURE|tuning|12-EDO scale vs 12-TET, largest deviation|" + juce::String (worst * 100.0f, 5) + "|cents");
            expect (worst < 1.0e-3f, "12-EDO .scl reproduces 12-TET");

            const juce::String just = "! just.scl\nJust major (5-limit)\n7\n9/8\n5/4\n4/3\n3/2\n5/3\n15/8\n2/1\n";
            expect (tuning::parseScl (just, s, err), err);
            expectEquals (s.description, juce::String ("Just major (5-limit)"));
            // without a .kbm the scale degrees follow the keys from C4 (60) upwards: key 61 = 9/8, key 62 = 5/4 ...
            const auto tj = tuning::build (s, nullptr);
            expectWithinAbsoluteError (tj[61], 60.0f + (float) (12.0 * std::log2 (9.0 / 8.0)), 1.0e-4f);
            expectWithinAbsoluteError (tj[64], 60.0f + (float) (12.0 * std::log2 (3.0 / 2.0)), 1.0e-4f);
            expectWithinAbsoluteError (tj[67], 72.0f, 1.0e-4f); // one period above
            expectWithinAbsoluteError (tj[60], 60.0f, 1.0e-4f);

            expect (tuning::parseScl (edo (19), s, err), err);
            const auto t19 = tuning::build (s, nullptr);
            expectWithinAbsoluteError (t19[61], 60.0f + 12.0f / 19.0f, 1.0e-4f);
            expectWithinAbsoluteError (t19[79], 72.0f, 1.0e-4f);
            expectWithinAbsoluteError (t19[41], 48.0f, 1.0e-4f);

            // .kbm: 12-key mapping, A4 = 432 Hz, keys C# and D# unmapped
            const juce::String kbm = "! test.kbm\n12\n0\n127\n60\n69\n432.0\n12\n0\nx\n2\nx\n4\n5\n6\n7\n8\n9\n10\n11\n";
            tuning::KeyMap m;
            expect (tuning::parseKbm (kbm, m, err), err);
            expect (tuning::parseScl (edo (12), s, err), err);
            const auto tk = tuning::build (s, &m);
            expectWithinAbsoluteError (tk[69], 69.0f + (float) (12.0 * std::log2 (432.0 / 440.0)), 1.0e-4f);
            expect (tk[61] <= tuning::kUnmapped && tk[63] <= tuning::kUnmapped && tk[73] <= tuning::kUnmapped, "x keys unmapped");
            expectWithinAbsoluteError (tk[62] - tk[60], 2.0f, 1.0e-4f);

            for (const char* bad : { "", "desc\n3\n100.0\n200.0\n", "desc\nabc\n", "desc\n2\n100.0\n-3/2\n", "desc\n1\n0.0\n" })
                expect (! tuning::parseScl (bad, s, err), juce::String ("rejected: ") + bad);
            expect (! tuning::parseKbm ("12\n0\n127\n", m, err), "short .kbm rejected");
        }

        beginTest ("Synth: 19-EDO pitch through the engine, unmapped keys silent, recalled with the project, not in presets");
        {
            ViceSynthProcessor proc;
            proc.prepareToPlay (48000.0, 256);
            auto s = PatchOps::initPatch();
            s.set ("osc_drift", 0.0f);
            s.set ("osc_a_position", 0.0f); // sine
            s.set ("osc_a_unison", 1.0f);
            s.set ("flt1_on", 0.0f);
            proc.applyPatch (s, "test");
            juce::String err;
            expect (proc.setTuning (edo (19), {}, err), err);
            auto play = [&] (int key)
            {
                juce::AudioBuffer<float> out (2, 48000), block (2, 256);
                juce::MidiBuffer on;
                on.addEvent (juce::MidiMessage::noteOn (1, key, (juce::uint8) 100), 0);
                juce::MidiBuffer none;
                for (int b = 0; b < 48000 / 256; ++b)
                {
                    block.clear();
                    proc.processBlock (block, b == 0 ? on : none);
                    for (int c = 0; c < 2; ++c) out.copyFrom (c, b * 256, block, c, 0, 256);
                }
                juce::MidiBuffer off;
                off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                for (int b = 0; b < 40; ++b) { block.clear(); none.clear(); proc.processBlock (block, b == 0 ? off : none); }
                return out;
            };
            for (int key : { 60, 63, 79, 98 })
            {
                const auto out = play (key);
                const double want = 440.0 * std::pow (2.0, (proc.getTuningTable()[(size_t) key] - 69.0) / 12.0);
                const double got = 48000.0 / period (out, 12000, 12000, 48000.0 / want);
                const double cents = 1200.0 * std::log2 (got / want);
                logMessage ("MEASURE|tuning|19-EDO key " + juce::String (key) + " (" + juce::String (want, 2) + " Hz) error|" + juce::String (cents, 3) + "|cents");
                expect (std::abs (cents) < 2.0, "key " + juce::String (key) + ": " + juce::String (cents, 3) + " cents");
            }

            const juce::String kbm = "12\n0\n127\n60\n60\n261.6255653\n12\n0\nx\n2\nx\n4\n5\n6\n7\n8\n9\n10\n11\n";
            expect (proc.setTuning (edo (12), kbm, err), err);
            expect (play (61).getMagnitude (0, 48000) == 0.0f, "an unmapped key plays nothing");
            expect (play (62).getMagnitude (0, 48000) > 0.01f, "a mapped key plays");

            expect (proc.setTuning (edo (19), {}, err), err);
            juce::MemoryBlock state;
            proc.getStateInformation (state);
            ViceSynthProcessor other;
            other.setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (other.getTuningTable()[61], 60.0f + 12.0f / 19.0f, 1.0e-4f, "tuning recalled with the project");
            expect (other.getTuningName().contains ("19"), other.getTuningName());
            expect (! proc.getCurrentPatch().toJson().contains ("tuning"), "tuning is not written to presets");
            expect (! proc.setTuning ("broken\nnot a number\n", {}, err) && err.isNotEmpty(), "a broken file is reported");
            expectWithinAbsoluteError (proc.getTuningTable()[61], 60.0f + 12.0f / 19.0f, 1.0e-4f, "a failed load keeps the current tuning");
            proc.releaseResources();
        }
    }
};

static TuningTests tuningTests;
} // namespace vsx
