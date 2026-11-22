#include "../Source/Synth/ArpSeq.h"
#include "../Source/Presets/SoundRecipes.h"

/*
    Ratchets: a step of the arpeggiator or step sequencer repeats 2-4 times, evenly spaced within the step.
*/
namespace vsx
{
class ArpRatchetTests final : public juce::UnitTest
{
public:
    ArpRatchetTests() : juce::UnitTest ("Arp ratchets", "VICE SYNTH X") {}

    struct Ev { int64_t time; bool on; int note; };

    // Runs the arp / seq for blocks of 256 samples; key C4 held for holdBlocks, then released.
    static std::vector<Ev> run (int mode, const ArpPattern& ap, const SeqPattern& sp, int holdBlocks, int totalBlocks, double& stepSamples)
    {
        ArpSeq a;
        a.prepare (48000.0);
        ParamSnapshot p;
        p.value = Recipes::init ("Arp", "Test").values;
        p.value[(size_t) pid::ARP (pid::arp::Mode)] = (float) mode;
        p.value[(size_t) pid::ARP (pid::arp::Swing)] = 0.0f;
        p.value[(size_t) pid::SEQ (pid::seq::Swing)] = 0.0f;
        p.value[(size_t) pid::ARP (pid::arp::Probability)] = 1.0f;
        p.value[(size_t) pid::ARP (pid::arp::Pattern)] = 0.0f;
        p.value[(size_t) pid::ARP (pid::arp::Octaves)] = 1.0f;
        const int rate = mode == 1 ? p.geti (pid::ARP (pid::arp::Rate)) : p.geti (pid::SEQ (pid::seq::Rate));
        stepSamples = ParamRegistry::syncDivisionBeats (rate) * 48000.0 * 60.0 / 120.0;
        TransportInfo t; t.bpm = 120.0;
        std::vector<Ev> ev;
        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::MidiBuffer in, out;
            if (b == 0) in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (b == holdBlocks) in.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            a.process (in, out, 256, p, &ap, &sp, t);
            for (const auto m : out)
            {
                const auto msg = m.getMessage();
                if (msg.isNoteOn() || msg.isNoteOff()) ev.push_back ({ (int64_t) b * 256 + m.samplePosition, msg.isNoteOn(), msg.getNoteNumber() });
            }
        }
        return ev;
    }

    void runTest() override
    {
        beginTest ("Arpeggiator: ratchet 3 on step 1 gives three evenly spaced notes, step 2 one note");
        {
            ArpPattern ap;
            ap.length = 2;
            ap.steps[0].ratchet = 3;
            SeqPattern sp;
            double step = 0.0;
            const auto ev = run (1, ap, sp, 400, 420, step);
            std::vector<int64_t> ons;
            for (const auto& e : ev) if (e.on) ons.push_back (e.time);
            expect (ons.size() >= 8, "notes played");
            if (ons.size() >= 8)
            {
                const double sub = step / 3.0;
                double maxErr = 0.0;
                // pattern of 2 steps: on times t0, t0+sub, t0+2 sub, t0+step, t0+2 step, ...
                for (int cyc = 0; cyc < 2; ++cyc)
                {
                    const int64_t t0 = ons[(size_t) (cyc * 4)];
                    maxErr = std::max ({ maxErr, std::abs ((double) (ons[(size_t) (cyc * 4 + 1)] - t0) - sub),
                                         std::abs ((double) (ons[(size_t) (cyc * 4 + 2)] - t0) - 2.0 * sub),
                                         std::abs ((double) (ons[(size_t) (cyc * 4 + 3)] - t0) - step) });
                }
                logMessage ("MEASURE|arp|ratchet 3: largest timing error of the repeats (step " + juce::String (step, 0) + " samples)|" + juce::String (maxErr, 1) + "|samples");
                expect (maxErr <= 1.0, "repeats evenly spaced within one sample");
            }
            int balance = 0, minBalance = 0;
            for (const auto& e : ev) { balance += e.on ? 1 : -1; minBalance = std::min (minBalance, balance); }
            expectEquals (balance, 0, "every note-on has a note-off");
            expect (minBalance >= 0, "no note-off before its note-on");
        }

        beginTest ("Releasing the key during a ratchet: no repeat sounds afterwards");
        {
            ArpPattern ap;
            ap.length = 1;
            ap.steps[0].ratchet = 4;
            SeqPattern sp;
            double step = 0.0;
            const auto ev = run (1, ap, sp, 1, 200, step); // key released after 256 samples: inside the first step
            int64_t lastOn = -1; int balance = 0;
            for (const auto& e : ev) { if (e.on) lastOn = e.time; balance += e.on ? 1 : -1; }
            logMessage ("MEASURE|arp|key released at 256 samples (step " + juce::String (step, 0) + "): last note-on at|" + juce::String ((double) lastOn, 0) + "|samples");
            expect (lastOn < 256, "no repeat after the release");
            expectEquals (balance, 0, "all notes off");
        }

        beginTest ("Step sequencer: ratchet 4");
        {
            ArpPattern ap;
            SeqPattern sp;
            for (auto& s : sp.steps) s.on = false;
            sp.steps[0].on = true;
            sp.steps[0].ratchet = 4;
            double step = 0.0;
            const auto ev = run (2, ap, sp, 60, 80, step);
            std::vector<int64_t> ons;
            for (const auto& e : ev) if (e.on && e.time < (int64_t) step) ons.push_back (e.time);
            expectEquals ((int) ons.size(), 4, "four notes in the first step");
            if (ons.size() == 4)
                expectWithinAbsoluteError ((double) (ons[3] - ons[0]), 0.75 * step, 1.0, "evenly spaced");
        }

        beginTest ("Ratchets saved and loaded; patterns without ratchets are written as before");
        {
            auto spec = Recipes::init ("R", "Test");
            const auto before = spec.toJson();
            expect (! before.contains ("ratchet"), "no ratchet field without ratchets");
            spec.arp.steps[2].ratchet = 3;
            spec.seq.steps[5].ratchet = 2;
            PatchSpec back; juce::String err;
            expect (PatchSpec::fromPresetVar (spec.toPresetVar(), back, err), err);
            expectEquals (back.arp.steps[2].ratchet, 3);
            expectEquals (back.seq.steps[5].ratchet, 2);
            expectEquals (back.arp.steps[1].ratchet, 1);
            PatchState st;
            st.fromVar (spec.stateToVar(), false);
            expectEquals (st.getArpPattern().steps[2].ratchet, 3, "session state");
            expectEquals (st.getSeqPattern().steps[5].ratchet, 2, "session state");
        }
    }
};

static ArpRatchetTests arpRatchetTests;
} // namespace vsx
