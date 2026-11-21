#include "../Source/FX/Effects.h"
#include "../Source/Presets/SoundRecipes.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

// Original synthetic fixtures; never reads an owner's sample library.
// Raw WAVs preserve gain. Listening copies use stereo RMS, not LUFS.
namespace vsx
{
class AudioAcceptanceTests final : public juce::UnitTest
{
public:
    AudioAcceptanceTests() : juce::UnitTest ("Audio acceptance evidence", "VICE SYNTH X") {}

    void writeWav (const juce::String& name, const juce::AudioBuffer<float>& b, double rate)
    {
        const auto path = juce::SystemStats::getEnvironmentVariable ("VSX_AUDIO_DIR", {});
        if (path.isEmpty()) return;
        juce::File dir (path);
        expect (dir.createDirectory().wasOk(), "Audio evidence directory");
        auto file = dir.getChildFile (name + ".wav");
        auto stream = file.createOutputStream();
        if (! stream) { expect (false, "Cannot write " + name); return; }
        stream->setPosition (0);
        stream->truncate();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (
            stream.get(), rate, 2, 24, {}, 0));
        if (! writer) { expect (false, "WAV writer " + name); return; }
        stream.release();
        expect (writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()), "WAV " + name);
    }

    double energy (const juce::AudioBuffer<float>& b, int start, int end)
    {
        double sum = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = start; i < end; ++i) sum += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return sum;
    }

    void report (const juce::String& label, const juce::AudioBuffer<float>& b, double rate)
    {
        double cross = 0.0, left = 0.0, right = 0.0, mean = 0.0;
        float peak = 0.0f; bool finite = true;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float l = b.getSample (0, i), r = b.getSample (1, i);
            finite &= std::isfinite (l) && std::isfinite (r);
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
            left += (double) l*l; right += (double) r*r; cross += (double) l*r;
            mean += l + r;
        }
        expect (finite, label + " finite");
        const double total = left + right;
        logMessage ("AUDIO|" + label + "|peak=" + juce::String (peak, 8)
            + "|energy=" + juce::String (total, 8)
            + "|rms_dBFS=" + juce::String (10.0*std::log10 (std::max (1.0e-30, total/(2*b.getNumSamples()))), 3)
            + "|correlation=" + juce::String (cross/std::sqrt (std::max (1.0e-30, left*right)), 6)
            + "|DC=" + juce::String (mean/(2*b.getNumSamples()), 8)
            + "|last_second_energy=" + juce::String (energy (b, b.getNumSamples()-(int) rate, b.getNumSamples()), 10));
    }

    juce::AudioBuffer<float> render (int type, const juce::AudioBuffer<float>& input,
                                     double rate, ParamSnapshot& params)
    {
        auto out = input;
        auto effect = createEffect (type);
        effect->prepare (rate, 256);
        ModOffsets mods;
        mods.clearAll();
        FxContext context { &params, &mods, 120.0 };
        for (int start = 0; start < out.getNumSamples(); start += 256)
            effect->process (out.getWritePointer (0, start), out.getWritePointer (1, start),
                             std::min (256, out.getNumSamples()-start), context);
        return out;
    }

    void runTest() override
    {
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
        {
            const auto suffix = "_" + juce::String ((int) rate);
            ParamSnapshot p;
            p.value = Recipes::init().values;
            p.value[(size_t) pid::FX (pid::FxReverb, pid::reverb::Decay)] = 2.0f;
            p.value[(size_t) pid::FX (pid::FxReverb, pid::reverb::Modulation)] = 0.0f;
            beginTest ("Reverb raw impulse evidence" + suffix);
            juce::AudioBuffer<float> impulse (2, (int) (8*rate));
            impulse.clear();
            impulse.setSample (0, 0, 0.25f); impulse.setSample (1, 0, 0.25f);
            auto tail = render (pid::FxReverb, impulse, rate, p);
            report ("reverb_impulse" + suffix, tail, rate);
            writeWav ("reverb_impulse_raw" + suffix, tail, rate);
            expect (energy (tail, 0, tail.getNumSamples()) > 1.0e-8, "Non-silent reverb tail");

            beginTest ("Stereo reverb predelay evidence" + suffix);
            p.value[(size_t) pid::FX (pid::FxReverb, pid::reverb::Predelay)] = 200.0f;
            impulse.setSample (1, 0, -0.25f);
            auto sideTail = render (pid::FxReverb, impulse, rate, p);
            logMessage ("AUDIO|reverb_side" + suffix + "|energy_before_predelay="
                + juce::String (energy (sideTail, 0, (int) (0.19*rate)), 10));
            writeWav ("reverb_side_predelay_raw" + suffix, sideTail, rate);

            beginTest ("Isolated effects, raw and stereo-RMS-matched listening copies" + suffix);
            juce::AudioBuffer<float> dry (2, (int) (4*rate));
            dry.clear();
            // Three original sine partials with repeated 250 ms decays; no sample/ROM content.
            for (int i = 0; i < (int) (2*rate); ++i)
            {
                const double t = i/rate, envelope = std::exp (-12.0*std::fmod (t, 0.5));
                const float x = (float) (0.08*envelope*(std::sin (kTwoPi*130.8128*t)
                    + 0.5*std::sin (kTwoPi*261.6256*t) + 0.25*std::sin (kTwoPi*523.2512*t)));
                dry.setSample (0, i, x); dry.setSample (1, i, x);
            }
            const double dryEnergy = energy (dry, 0, dry.getNumSamples());
            writeWav ("fixture_dry" + suffix, dry, rate);
            for (int type = 0; type < pid::NumFx; ++type)
            {
                p.value = Recipes::init().values;
                auto wet = render (type, dry, rate, p);
                const auto label = choices::fxTypeIds[type] + suffix;
                report (label, wet, rate);
                writeWav (label + "_raw", wet, rate);
                const double wetEnergy = energy (wet, 0, wet.getNumSamples());
                if (wetEnergy > 1.0e-20)
                {
                    const float gain = (float) std::sqrt (dryEnergy/wetEnergy);
                    auto matched = wet; matched.applyGain (gain);
                    float peak = 0.0f;
                    for (int ch = 0; ch < 2; ++ch)
                        peak = std::max (peak, matched.getMagnitude (ch, 0, matched.getNumSamples()));
                    // Never clip or limit a comparison; report a rejected file explicitly.
                    if (peak < 0.99f) writeWav (label + "_rms_matched", matched, rate);
                    else logMessage ("AUDIO|" + label + "|matched_copy_skipped_peak=" + juce::String (peak));
                    logMessage ("AUDIO|" + label + "|listening_gain_dB=" + juce::String (20*std::log10 (gain), 4));
                }
            }
        }
    }
};
static AudioAcceptanceTests audioAcceptanceTests;
}
