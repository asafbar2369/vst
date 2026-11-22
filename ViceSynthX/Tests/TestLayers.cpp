#include "../Source/PluginProcessor.h"
#include "../Source/Presets/SoundRecipes.h"
#include <cmath>

/*
    Layers: keyboard splits, velocity layers, transpose without stuck notes, level, silence, project recall, EDIT swap.
    Layer A plays a sine at the key; layer B plays a sine a fifth higher (+7 st), so each layer is identified by pitch.
*/
namespace vsx
{
class LayerTests final : public juce::UnitTest
{
public:
    LayerTests() : juce::UnitTest ("Layers", "VICE SYNTH X") {}

    static PatchSpec sine (int semi)
    {
        auto s = Recipes::init ("Sine " + juce::String (semi), "Test");
        s.set ("osc_drift", 0.0f);
        s.set ("osc_a_position", 0.0f);
        s.set ("osc_a_unison", 1.0f);
        s.set ("osc_a_phase_rand", 0.0f);
        s.set ("osc_a_semi", (float) semi);
        s.set ("flt1_on", 0.0f);
        s.set ("master_volume", -12.0f);
        s.set ("env1_release", 0.02f);
        return s;
    }

    struct Rig
    {
        ViceSynthProcessor proc;
        juce::AudioBuffer<float> block { 2, 256 };
        Rig() { proc.prepareToPlay (48000.0, 256); }
        ~Rig() { proc.releaseResources(); }
        std::vector<float> play (juce::MidiBuffer first, int blocks)
        {
            std::vector<float> x;
            juce::MidiBuffer none;
            for (int b = 0; b < blocks; ++b)
            {
                block.clear();
                proc.processBlock (block, b == 0 ? first : none);
                for (int i = 0; i < 256; ++i) x.push_back (block.getSample (0, i));
            }
            return x;
        }
        void silence() { proc.panic(); play ({}, 6); }
    };

    // windowed Goertzel level (dB) of MIDI note m in x (skipping the first 2048 samples)
    static double level (const std::vector<float>& x, double note)
    {
        const double f = 440.0 * std::pow (2.0, (note - 69.0) / 12.0), sr = 48000.0;
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        const size_t a = 2048, n = x.size() - a;
        for (size_t i = 0; i < n; ++i)
        {
            const double t = 2.0 * juce::MathConstants<double>::pi * (double) i / (double) (n - 1);
            const double win = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
            const double s0 = x[a + i] * win + c * s1 - s2; s2 = s1; s1 = s0;
        }
        return 10.0 * std::log10 ((s1 * s1 + s2 * s2 - c * s1 * s2) / ((double) n * n) + 1e-30);
    }

    static juce::MidiBuffer on (int key, int vel)
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, key, (juce::uint8) vel), 0);
        return m;
    }

    void runTest() override
    {
        beginTest ("Keyboard split: B below C4, A from C4 up");
        {
            Rig rig;
            rig.proc.applyPatch (sine (0), "a");
            auto& b = rig.proc.getLayer (0);
            b.setSound (sine (7));
            b.enabled.store (true);
            b.routing.keyHi.store (59);
            rig.proc.getLayerARouting().keyLo.store (60);
            rig.play ({}, 2);
            const auto low = rig.play (on (48, 100), 60);
            logMessage ("MEASURE|layers|key 48: layer B (55) / layer A (48)|" + juce::String (level (low, 55), 1) + " / " + juce::String (level (low, 48), 1) + "|dB");
            expect (level (low, 55) > -40.0 && level (low, 48) < level (low, 55) - 60.0, "key 48 plays only layer B");
            rig.silence();
            const auto high = rig.play (on (72, 100), 60);
            logMessage ("MEASURE|layers|key 72: layer A (72) / layer B (79)|" + juce::String (level (high, 72), 1) + " / " + juce::String (level (high, 79), 1) + "|dB");
            expect (level (high, 72) > -40.0 && level (high, 79) < level (high, 72) - 60.0, "key 72 plays only layer A");
        }

        beginTest ("Velocity layers: A below 100, B from 100");
        {
            Rig rig;
            rig.proc.applyPatch (sine (0), "a");
            auto& b = rig.proc.getLayer (0);
            b.setSound (sine (7));
            b.enabled.store (true);
            b.routing.velLo.store (100);
            rig.proc.getLayerARouting().velHi.store (99);
            rig.play ({}, 2);
            const auto soft = rig.play (on (60, 64), 60);
            expect (level (soft, 60) > -45.0 && level (soft, 67) < level (soft, 60) - 60.0, "velocity 64 plays layer A only");
            rig.silence();
            const auto hard = rig.play (on (60, 120), 60);
            expect (level (hard, 67) > -45.0 && level (hard, 60) < level (hard, 67) - 60.0, "velocity 120 plays layer B only");
        }

        beginTest ("Transpose changed while a key is held: the note-off still releases it (no stuck note)");
        {
            Rig rig;
            rig.proc.applyPatch (sine (0), "a");
            rig.proc.getLayerARouting().keyLo.store (100); // layer A plays nothing below key 100
            auto& b = rig.proc.getLayer (0);
            b.setSound (sine (0));
            b.enabled.store (true);
            b.routing.transpose.store (12);
            rig.play ({}, 2);
            rig.play (on (60, 100), 20);
            b.routing.transpose.store (0); // changed while held
            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            rig.play (off, 40);
            const auto after = rig.play ({}, 20);
            float peak = 0.0f; for (float v : after) peak = std::max (peak, std::abs (v));
            logMessage ("MEASURE|layers|after note-off with a changed transpose, peak|" + juce::String (peak, 9) + "|linear");
            expect (peak < 1.0e-5f, "released");
        }

        beginTest ("Layer level, PANIC, preset switch silence, project recall, EDIT swap");
        {
            Rig rig;
            rig.proc.applyPatch (sine (0), "a");
            rig.proc.getLayerARouting().keyLo.store (100);
            auto& b = rig.proc.getLayer (0);
            b.setSound (sine (7));
            b.enabled.store (true);
            rig.play ({}, 2);
            const double full = level (rig.play (on (60, 100), 60), 67);
            rig.silence();
            b.levelDb.store (-6.0f);
            const double quieter = level (rig.play (on (60, 100), 60), 67);
            logMessage ("MEASURE|layers|layer level -6 dB measured|" + juce::String (quieter - full, 2) + "|dB");
            expectWithinAbsoluteError (quieter - full, -6.0, 0.2, "layer level");

            rig.play (on (64, 100), 10);
            rig.proc.panic();
            rig.play ({}, 2);
            float peak = 0.0f; for (float v : rig.play ({}, 20)) peak = std::max (peak, std::abs (v));
            expect (peak == 0.0f, "PANIC silences every layer");

            rig.play (on (64, 100), 10);
            rig.proc.applyPatch (sine (3), "switch");
            rig.play ({}, 2);
            peak = 0.0f; for (float v : rig.play ({}, 20)) peak = std::max (peak, std::abs (v));
            expect (peak == 0.0f, "a preset switch silences every layer");

            b.routing.keyLo.store (36); b.routing.keyHi.store (84); b.routing.velLo.store (20); b.routing.transpose.store (-5);
            b.pan.store (0.4f);
            juce::MemoryBlock state;
            rig.proc.getStateInformation (state);
            Rig other;
            other.proc.setStateInformation (state.getData(), (int) state.getSize());
            auto& ob = other.proc.getLayer (0);
            expect (ob.hasSound() && ob.enabled.load() && ob.getSound().meta.name == "Sine 7", "layer sound and state recalled");
            expect (ob.routing.keyLo.load() == 36 && ob.routing.keyHi.load() == 84 && ob.routing.velLo.load() == 20 && ob.routing.transpose.load() == -5, "routing recalled");
            expectWithinAbsoluteError (ob.levelDb.load(), -6.0f, 1.0e-4f);
            expectWithinAbsoluteError (ob.pan.load(), 0.4f, 1.0e-4f);
            expect (other.proc.getLayerARouting().keyLo.load() == 100, "layer A routing recalled");
            expect (! other.proc.getLayer (1).hasSound() && ! other.proc.getLayer (1).enabled.load(), "empty layers stay empty");
            expect (! rig.proc.getCurrentPatch().toJson().contains ("\"layers\""), "layers are not written into presets");

            rig.proc.swapLayerWithMain (0);
            expect (rig.proc.getCurrentPatch().meta.name == "Sine 7" && rig.proc.getLayer (0).getSound().meta.name == "Sine 3", "EDIT swaps the sounds");
        }

        beginTest ("Preset morphing: switches at 50 %, continuous values in the normalised range, full morph = target, recall");
        {
            const int morph = pid::G (pid::global::Morph), vol = pid::G (pid::global::MasterVolume);
            auto a = sine (0);                       // Preset Volume -12 dB
            auto t = sine (7); t.set ("master_volume", -24.0f); t.meta.name = "Target";
            auto render = [&] (Rig& rig) { rig.silence(); return rig.play (on (60, 100), 60); };
            Rig rig;
            rig.proc.applyPatch (a, "a");
            rig.proc.setMorphTarget (t);
            rig.play ({}, 2);

            rig.proc.setParam (morph, 0.0f);
            const auto m0 = render (rig);
            rig.proc.setParam (morph, 0.49f);
            const auto m49 = render (rig);
            rig.proc.setParam (morph, 0.51f);
            const auto m51 = render (rig);
            expect (level (m0, 60) > -45.0 && level (m0, 67) < level (m0, 60) - 60.0, "morph 0 plays the patch");
            expect (level (m49, 60) > level (m49, 67) + 60.0, "morph 49 %: the semitone switch is still the patch's");
            expect (level (m51, 67) > level (m51, 60) + 60.0, "morph 51 %: the semitone switch is the target's");

            // continuous: Preset Volume moves in its normalised (skewed) range
            const auto& d = ParamRegistry::get()[vol];
            const float expected = d.fromNormalised (0.5f * (d.toNormalised (-12.0f) + d.toNormalised (-24.0f)));
            rig.proc.setParam (morph, 0.5f);
            const auto m50 = render (rig);
            logMessage ("MEASURE|morph|Preset Volume -12 -> -24 dB at 50 %: level change vs morph 0 (expected " + juce::String (expected + 12.0f, 2) + ")|"
                        + juce::String (level (m50, 67) - level (m0, 60), 2) + "|dB");
            expectWithinAbsoluteError (level (m50, 67) - level (m0, 60), (double) expected + 12.0, 0.1, "Preset Volume at 50 %");

            rig.proc.setParam (morph, 1.0f);
            const auto m100 = render (rig);
            Rig ref;
            ref.proc.applyPatch (t, "t");
            ref.play ({}, 2);
            const auto direct = render (ref);
            {
                const auto again = render (ref);
                double e2 = 0, s2 = 0;
                for (size_t i = 0; i < again.size(); ++i) { e2 += (again[i] - direct[i]) * (again[i] - direct[i]); s2 += direct[i] * direct[i]; }
                logMessage ("MEASURE|morph|same patch rendered twice, sample difference (free-running phase)|" + juce::String (10.0 * std::log10 (e2 / s2 + 1e-30), 1) + "|dB");
            }
            // oscillator phase runs free (the same patch rendered twice differs sample by sample), so compare what is heard:
            // the level at the target's pitch and the absence of the patch's pitch
            logMessage ("MEASURE|morph|morph 100 % vs the target played directly, level at 67|" + juce::String (level (m100, 67), 2) + " / " + juce::String (level (direct, 67), 2) + "|dB");
            expectWithinAbsoluteError (level (m100, 67), level (direct, 67), 0.05, "full morph: same level as the target");
            expect (level (m100, 60) < level (m100, 67) - 60.0, "full morph: none of the patch's pitch");
            expect (! rig.proc.getCurrentPatch().toJson().contains ("\"morph\""), "Morph is not stored in presets");
            juce::MemoryBlock state;
            rig.proc.getStateInformation (state);
            Rig other;
            other.proc.setStateInformation (state.getData(), (int) state.getSize());
            expect (other.proc.hasMorphTarget() && other.proc.getMorphTargetName() == "Target", "morph target recalled");
            expectWithinAbsoluteError (other.proc.getParam (morph), 1.0f, 1.0e-4f);
            rig.proc.clearMorphTarget();
            const auto cleared = render (rig);
            expect (level (cleared, 60) > level (cleared, 67) + 60.0, "no target: the patch plays unchanged");
        }
    }
};

static LayerTests layerTests;
} // namespace vsx
