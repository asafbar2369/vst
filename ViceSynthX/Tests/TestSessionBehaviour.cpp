#include "../Source/PluginProcessor.h"
#include "../Source/Presets/SoundRecipes.h"
#include "../Source/Analysis/SoundMatcher.h"
#include <cmath>

/*
    Preset switching silence, PANIC, MASTER VOL, GLOBAL FX BYPASS and the CLIP indicator, on the real processor.
*/
namespace vsx
{
class SessionBehaviourTests final : public juce::UnitTest
{
public:
    SessionBehaviourTests() : juce::UnitTest ("Session behaviour", "VICE SYNTH X") {}

    struct Rig
    {
        std::unique_ptr<ViceSynthProcessor> proc = std::make_unique<ViceSynthProcessor>();
        juce::AudioBuffer<float> buf { 2, 256 };
        Rig() { proc->setRateAndBufferSizeDetails (48000.0, 256); proc->prepareToPlay (48000.0, 256); }
        ~Rig() { proc->releaseResources(); }
        float block (juce::MidiBuffer midi = {})
        {
            buf.clear();
            proc->processBlock (buf, midi);
            return buf.getMagnitude (0, 256);
        }
        float blocks (int n) { float p = 0.0f; for (int i = 0; i < n; ++i) p = std::max (p, block()); return p; }
    };

    static PatchSpec tailPatch()
    {
        // long release, reverb, delay and arpeggiator: every kind of tail that must stop on a preset change
        auto s = Recipes::init ("Tails", "Test");
        Recipes::env (s, 0, 0.001f, 0.5f, 1.0f, 8.0f);
        Recipes::fx (s, pid::FxReverb, 0.6f, { { "decay", 10.0f } });
        Recipes::fx (s, pid::FxStereoDelay, 0.5f, { { "feedback", 0.9f } });
        s.p ("arp_mode", 1.0f); // arpeggiator
        return s;
    }

    void runTest() override
    {
        beginTest ("Preset change: complete silence until a new note, held notes do not retrigger");
        {
            Rig rig;
            rig.proc->applyPatch (tailPatch(), "test");
            rig.block();
            juce::MidiBuffer on;
            for (int n : { 48, 55, 60, 64, 67 }) on.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 127), 0);
            on.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0); // sustain pedal down
            rig.block (on);
            expect (rig.blocks (40) > 0.01f, "the held chord sounds");
            rig.proc->applyPatch (Recipes::init ("Next", "Test"), "switch");
            rig.block(); // the switch block fades the last sample to zero over 64 samples
            const float after = rig.blocks (200);
            expect (after == 0.0f, "no sound after a preset change (voices, sustain, arp, reverb, delay): " + juce::String (after, 9));
            juce::MidiBuffer note;
            note.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            rig.block (note);
            expect (rig.blocks (5) > 0.01f, "a new note plays the new preset");
        }

        beginTest ("Rapid preset switching under load stays finite and silent between notes");
        {
            Rig rig;
            juce::Random rng (4);
            bool finite = true;
            for (int k = 0; k < 120; ++k)
            {
                rig.proc->applyPatch (k % 2 == 0 ? tailPatch() : Recipes::init ("B", "Test"), "rapid");
                juce::MidiBuffer m;
                if (rng.nextBool()) m.addEvent (juce::MidiMessage::noteOn (1, 36 + rng.nextInt (60), (juce::uint8) 127), rng.nextInt (255));
                rig.block (m);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i) finite &= std::isfinite (rig.buf.getSample (ch, i));
            }
            expect (finite);
            rig.proc->applyPatch (Recipes::init ("End", "Test"), "end");
            rig.block();
            expect (rig.blocks (100) == 0.0f, "silent after the last switch");
        }

        beginTest ("PANIC stops every voice and tail immediately");
        {
            Rig rig;
            rig.proc->applyPatch (tailPatch(), "test");
            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
            rig.block (on);
            expect (rig.blocks (40) > 0.01f);
            rig.proc->panic();
            rig.block();
            expect (rig.blocks (100) == 0.0f, "PANIC leaves no tail");
        }

        beginTest ("MASTER VOL: final output, silence at minimum, never changed by presets, recalled, not saved in presets");
        {
            Rig rig;
            const int mv = pid::G (pid::global::MasterOut);
            auto steady = Recipes::init ("A", "Test");
            steady.set ("osc_drift", 0.0f); // steady pitch: compare levels, not drift
            steady.set ("osc_a_phase_rand", 0.0f);
            steady.set ("master_volume", -12.0f); // keep the reference well below the output safety stage
            rig.proc->applyPatch (steady, "a");
            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            rig.block (on);
            rig.blocks (20);
            const float ref = rig.blocks (10);
            rig.proc->setParam (mv, -12.0f);
            rig.blocks (20);
            const float quieter = rig.blocks (10);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (quieter / ref), -12.0f, 0.5f);
            rig.proc->setParam (mv, -70.0f);
            rig.blocks (20);
            expect (rig.blocks (10) == 0.0f, "complete silence at minimum");
            rig.proc->setParam (mv, -6.0f);
            auto other = Recipes::init ("B", "Test");
            other.values[(size_t) mv] = 6.0f; // a preset can never carry MASTER VOL
            rig.proc->applyPatch (other, "b");
            expectWithinAbsoluteError (rig.proc->getParam (mv), -6.0f, 0.01f);
            expect (! rig.proc->getCurrentPatch().toJson().contains ("master_out"), "MASTER VOL is not written to presets");
            expect (! rig.proc->getCurrentPatch().toJson().contains ("fx_bypass"), "GLOBAL FX BYPASS is not written to presets");
            juce::MemoryBlock state;
            rig.proc->getStateInformation (state);
            Rig other2;
            other2.proc->setStateInformation (state.getData(), (int) state.getSize());
            expectWithinAbsoluteError (other2.proc->getParam (mv), -6.0f, 0.01f, "project recall");
        }

        beginTest ("GLOBAL FX BYPASS removes every effect tail; not changed by presets");
        {
            Rig rig;
            rig.proc->applyPatch (tailPatch(), "t");
            rig.proc->setParam (pid::G (pid::global::FxBypass), 1.0f);
            juce::MidiBuffer on, off;
            on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            rig.proc->setParam (pid::E (0, pid::env::Release), 0.01f);
            rig.block (on);
            rig.blocks (20);
            rig.block (off);
            rig.blocks (40);
            expect (rig.blocks (20) < 1.0e-4f, "no reverb / delay tail while bypassed");
            rig.proc->applyPatch (tailPatch(), "t2");
            expect (rig.proc->getParam (pid::G (pid::global::FxBypass)) > 0.5f, "a preset load keeps the bypass");
        }

        beginTest ("INIT patch has polyphonic headroom (full-velocity six-note chord below -1.5 dBFS)");
        {
            auto r = std::make_unique<OfflineRenderer> (48000.0);
            const auto init = Recipes::init();
            const auto single = r->render (init, 48, 127, 1.5, 0.3, 1);
            const auto chord = r->renderNotes (init, { 48, 52, 55, 59, 62, 67 }, 127, 1.5, 0.3, 1);
            const float sp = juce::Decibels::gainToDecibels (single.getMagnitude (0, single.getNumSamples()));
            const float cp = juce::Decibels::gainToDecibels (chord.getMagnitude (0, chord.getNumSamples()));
            logMessage ("MEASURE|gain|INIT single note peak|" + juce::String (sp, 2) + "|dBFS");
            logMessage ("MEASURE|gain|INIT six-note chord peak|" + juce::String (cp, 2) + "|dBFS");
            expect (cp < -1.5f, "INIT chord peak " + juce::String (cp, 2));
        }

        beginTest ("CLIP indicator latches only when the output safety stage engages");
        {
            Rig rig;
            rig.proc->applyPatch (Recipes::init ("A", "Test"), "a");
            rig.proc->resetClipLatch();
            juce::MidiBuffer on;
            on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            rig.block (on);
            rig.blocks (20);
            expect (! rig.proc->getClipLatched(), "INIT at default level does not clip");
            rig.proc->setParam (pid::G (pid::global::MasterVolume), 12.0f);
            rig.proc->setParam (pid::G (pid::global::MasterOut), 6.0f);
            rig.blocks (30);
            expect (rig.proc->getClipLatched(), "+18 dB drives the output into the safety stage");
        }
    }
};

static SessionBehaviourTests sessionBehaviourTests;
} // namespace vsx
