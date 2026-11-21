#include "SoundMatcher.h"
#include "../Presets/SoundRecipes.h"

namespace vsx
{
OfflineRenderer::OfflineRenderer (double sampleRate) : sr (sampleRate)
{
    engine.prepare (sr, 512);
    rack.prepare (sr, 512);
}

juce::AudioBuffer<float> OfflineRenderer::render (const PatchSpec& spec, int note, int velocity, double noteSeconds, double tailSeconds,
                                                  int quality)
{
    state.fromVar (spec.stateToVar(), false);
    ParamSnapshot p;
    p.value = spec.values;
    p.value[(size_t) pid::G (pid::global::Quality)] = (float) quality;
    engine.reset();
    rack.reset();
    rack.updateConvolution (p, spec.userIr);

    const int total = (int) ((noteSeconds + tailSeconds) * sr);
    const int offAt = (int) (noteSeconds * sr);
    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> block (2, 512);
    TransportInfo t;
    t.bpm = 120.0;
    const float master = dbToGain (p.get (pid::kPresetVolume));
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = std::min (512, total - pos);
        block.setSize (2, n, false, false, true);
        juce::MidiBuffer midi;
        if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
        if (offAt >= pos && offAt < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, note), offAt - pos);
        p.value = spec.values;
        p.value[(size_t) pid::G (pid::global::Quality)] = (float) quality;
        engine.process (block, midi, p, t);
        FxContext fc { &p, &engine.getGlobalMods(), 120.0 };
        rack.process (block.getWritePointer (0), block.getWritePointer (1), n, fc, state);
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, pos, block, c, 0, n);
    }
    out.applyGain (master);
    return out;
}

// ---------------------------------------------------------------------------------
PatchSpec SoundMatcher::initialGuess (const AudioFeatures& ref, int& note)
{
    using namespace Recipes;
    PatchSpec s = init ("Matched Sound", "Digital");
    note = ref.f0 > 0.0f ? juce::jlimit (24, 96, (int) std::round (hzToMidi (ref.f0))) : 60;
    const float f0 = ref.f0 > 0.0f ? ref.f0 : midiToHz ((float) note);

    if (ref.f0 > 0.0f)
    {
        // harmonic amplitudes: measured for the first 32, spectral envelope beyond
        std::vector<float> amps;
        const float nyq = (float) ref.sampleRate * 0.5f;
        auto bandAt = [&ref] (float hz)
        {
            const float pos = std::log (std::max (40.0f, hz) / 40.0f) / std::log (18000.0f / 40.0f) * (float) AudioFeatures::kBands;
            const int i = juce::jlimit (0, AudioFeatures::kBands - 1, (int) pos);
            return ref.bandsDb[(size_t) i];
        };
        const float offset = ref.harmonicsDb[31] - bandAt (f0 * 32.0f);
        for (int k = 1; k <= 256 && f0 * (float) k < nyq; ++k)
        {
            const float db = k <= AudioFeatures::kHarmonics ? ref.harmonicsDb[(size_t) k - 1] : bandAt (f0 * (float) k) + offset;
            amps.push_back (db > -85.0f ? dbToGain (db) : 0.0f);
        }
        auto wt = Wavetable::fromHarmonics ({ amps }, "Matched Spectrum", "custom:Matched Spectrum#" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
        wavetable (s, 0, "Basic Shapes", 0.0f, 0.8f);
        s.customWavetable[0] = wt;
        s.wavetableId[0] = wt->sourceId;
        if (ref.flatness > 0.12f)
            noise (s, juce::jlimit (0.0f, 0.6f, ref.flatness * 1.5f), 1, ref.centroid > 3000.0f ? 0.3f : -0.3f);
        filter (s, 0, SVF, LP12, 20000.0f, 0.05f, 0.0f, 0.0f, 0.5f);
        if (ref.centroidStart > 0.0f && ref.centroidEnd < ref.centroidStart * 0.7f)
        {
            const float octaves = std::log2 (ref.centroidStart / std::max (100.0f, ref.centroidEnd));
            s.set ("flt1_cutoff", juce::jlimit (200.0f, 20000.0f, ref.centroidEnd * 3.0f));
            s.set ("flt1_env_amount", juce::jlimit (0.0f, 1.0f, octaves / 8.0f));
            env (s, 1, 0.002f, std::max (0.1f, ref.decay), 0.0f, 0.3f);
        }
    }
    else
    {
        s.set ("osc_a_on", 0.0f);
        noise (s, 0.8f, ref.flatness > 0.5f ? 0 : 1, 0.0f);
        filter (s, 0, SVF, BP, juce::jlimit (100.0f, 15000.0f, ref.centroid), 0.3f, 0.0f, 0.0f, 0.0f);
    }

    env (s, 0, juce::jlimit (0.001f, 5.0f, ref.attack), juce::jlimit (0.03f, 10.0f, std::max (0.05f, ref.decay)),
         juce::jlimit (0.0f, 1.0f, ref.sustain), juce::jlimit (0.02f, 10.0f, ref.release > 0.0f ? ref.release : 0.2f));

    if (ref.vibratoDepthCents > 5.0f && ref.vibratoRate > 2.0f)
    {
        lfo (s, 0, 0, ref.vibratoRate);
        s.addMod (modsrc::Lfo1, "osc_a_fine", juce::jlimit (0.0f, 0.5f, ref.vibratoDepthCents / 200.0f));
    }
    if (ref.tremoloDepth > 0.1f && ref.tremoloRate > 1.0f)
    {
        lfo (s, 1, 0, ref.tremoloRate);
        s.addMod (modsrc::Lfo2, "osc_a_level", -juce::jlimit (0.0f, 0.5f, ref.tremoloDepth * 0.4f));
    }
    if (ref.stereoWidth > 0.15f)
        unison (s, 0, 3, 0.06f, juce::jlimit (0.2f, 1.0f, ref.stereoWidth * 2.0f));
    s.meta.source = "ai";
    s.meta.description = "Generated from reference-audio analysis.";
    return s;
}

MatchResult SoundMatcher::optimise (const AudioFeatures& ref, const PatchSpec& start, int note, int iterations, OfflineRenderer& r,
                                    const Progress& progress)
{
    MatchResult res;
    const auto& reg = ParamRegistry::get();
    const double noteSec = juce::jlimit (0.3, 4.0, ref.duration * (ref.sustain > 0.2f ? 0.75 : 0.5));
    const double tailSec = juce::jlimit (0.2, 3.0, ref.duration - noteSec);
    auto evaluate = [&] (const PatchSpec& s)
    {
        auto audio = r.render (s, note, 100, noteSec, tailSec);
        ++res.renders;
        return AudioFeatures::similarity (ref, AudioFeatures::analyse (audio, r.getSampleRate()));
    };

    juce::StringArray ids { "flt1_cutoff", "flt1_resonance", "flt1_env_amount", "env1_attack", "env1_decay", "env1_sustain",
                            "env1_release", "env2_decay" };
    if (start.get ("noise_on") > 0.5f) ids.add ("noise_level");
    if (start.get ("osc_a_unison") > 1.5f) ids.add ("osc_a_detune");
    std::vector<int> idx;
    for (const auto& id : ids) idx.push_back (reg.indexOf (id));

    PatchSpec best = start;
    float bestScore = evaluate (best);
    res.initialSimilarity = bestScore;
    std::vector<float> x;
    for (int i : idx) x.push_back (reg[i].toNormalised (best.values[(size_t) i]));
    float sigma = 0.12f;
    juce::Random rnd (12345);

    for (int it = 0; it < iterations; ++it)
    {
        if (progress && ! progress ((float) it / (float) iterations, bestScore))
        {
            res.cancelled = true;
            break;
        }
        PatchSpec cand = best;
        std::vector<float> y = x;
        for (size_t k = 0; k < y.size(); ++k)
        {
            const float g = (float) (std::sqrt (-2.0 * std::log (std::max (1.0e-9, rnd.nextDouble()))) * std::cos (2.0 * kPiD * rnd.nextDouble()));
            y[k] = juce::jlimit (0.0f, 1.0f, y[k] + sigma * g);
            cand.values[(size_t) idx[k]] = reg[idx[k]].fromNormalised (y[k]);
        }
        const float score = evaluate (cand);
        if (score > bestScore)
        {
            bestScore = score;
            best = cand;
            x = y;
            sigma = std::min (0.4f, sigma * 1.4f);
        }
        else
            sigma = std::max (0.01f, sigma * 0.88f);
    }
    res.best = best;
    res.finalSimilarity = bestScore;
    res.report = "Similarity " + juce::String (res.initialSimilarity * 100.0f, 0) + "% -> " + juce::String (bestScore * 100.0f, 0)
               + "% after " + juce::String (res.renders) + " candidate renders.";
    return res;
}

PatchSpec SoundMatcher::refineDescriptors (const PatchSpec& before, const PatchSpec& after, const juce::StringArray& descriptors,
                                           OfflineRenderer& r, juce::String& log)
{
    auto measure = [&r] (const PatchSpec& s)
    {
        PatchSpec t = s;
        t.set ("arp_mode", 0.0f);
        return AudioFeatures::analyse (r.render (t, 60, 100, 1.2, 0.8), r.getSampleRate(), 1.2);
    };
    const auto fb = measure (before);
    PatchSpec spec = after;
    auto fa = measure (spec);
    if (! fb.valid || ! fa.valid) return spec;

    auto scaleCutoffs = [&spec] (float factor)
    {
        bool any = false;
        for (const char* f : { "flt1", "flt2" })
            if (spec.get (juce::String (f) + "_on") > 0.5f)
            {
                spec.set (juce::String (f) + "_cutoff", spec.get (juce::String (f) + "_cutoff") * factor);
                any = true;
            }
        return any;
    };

    for (const auto& dRaw : descriptors)
    {
        const auto d = dRaw.toLowerCase();
        if (d.contains ("dark"))
        {
            const float target = fb.centroid * 0.78f;
            for (int i = 0; i < 4 && fa.centroid > target; ++i)
            {
                if (! scaleCutoffs (0.7f))
                    Recipes::filter (spec, 0, Recipes::SVF, Recipes::LP12, std::max (300.0f, fa.centroid * 1.5f), 0.1f);
                fa = measure (spec);
            }
            log << "Darker: brightness centroid " << (int) fb.centroid << " Hz -> " << (int) fa.centroid << " Hz ("
                << juce::String ((fa.centroid / fb.centroid - 1.0f) * 100.0f, 0) << "%).\n";
        }
        else if (d.contains ("bright"))
        {
            const float target = fb.centroid * 1.25f;
            for (int i = 0; i < 4 && fa.centroid < target; ++i)
            {
                if (! scaleCutoffs (1.45f) || spec.get ("flt1_cutoff") >= 19000.0f)
                    Recipes::fx (spec, pid::FxEq, 1.0f, { { "high_gain", std::min (12.0f, spec.get ("fx_eq_high_gain") + 3.0f) } });
                fa = measure (spec);
            }
            log << "Brighter: brightness centroid " << (int) fb.centroid << " Hz -> " << (int) fa.centroid << " Hz.\n";
        }
        else if (d.contains ("wide"))
        {
            const float target = std::max (fb.stereoWidth + 0.15f, 0.25f);
            for (int i = 0; i < 3 && fa.stereoWidth < target; ++i)
            {
                for (int o = 0; o < kNumOscs; ++o)
                {
                    const auto id = Recipes::oscId (o);
                    if (spec.get (id + "_on") < 0.5f) continue;
                    if (spec.get (id + "_unison") < 2.0f) Recipes::unison (spec, o, 3, std::max (0.08f, spec.get (id + "_detune")), 0.8f);
                    else spec.set (id + "_spread", std::min (1.0f, spec.get (id + "_spread") + 0.25f));
                }
                if (i > 0) Recipes::fx (spec, pid::FxStereo, 1.0f, { { "width", std::min (2.0f, spec.get ("fx_stereo_width") + 0.3f) } });
                fa = measure (spec);
            }
            log << "Wider: stereo width " << juce::String (fb.stereoWidth, 2) << " -> " << juce::String (fa.stereoWidth, 2) << ".\n";
        }
        else if (d.contains ("soft") && d.contains ("attack"))
        {
            const float target = std::max (fb.attack * 2.0f, 0.06f);
            for (int i = 0; i < 4 && fa.attack < target; ++i)
            {
                spec.set ("env1_attack", std::max (0.03f, spec.get ("env1_attack") * 2.2f));
                fa = measure (spec);
            }
            log << "Softer attack: measured attack " << juce::String (fb.attack * 1000.0f, 0) << " ms -> " << juce::String (fa.attack * 1000.0f, 0) << " ms.\n";
        }
        else if (d.contains ("punch"))
        {
            for (int i = 0; i < 3 && fa.attack > std::max (0.004f, fb.attack * 0.6f); ++i)
            {
                spec.set ("env1_attack", spec.get ("env1_attack") * 0.4f);
                fa = measure (spec);
            }
            log << "Punchier: measured attack " << juce::String (fb.attack * 1000.0f, 0) << " ms -> " << juce::String (fa.attack * 1000.0f, 0) << " ms.\n";
        }
    }
    return spec;
}

juce::String SoundMatcher::limitationsFor (const AudioFeatures& ref)
{
    juce::StringArray l;
    l.add ("Matching approximates the timbre, envelope and modulation of a single note; it is not a resynthesis of the recording.");
    if (ref.f0 <= 0.0f) l.add ("The reference is unpitched or noisy, so it is approximated with filtered noise.");
    if (ref.onsets > 2) l.add ("The reference contains several notes/onsets; the analysis averages them.");
    if (ref.correlation < 0.3f) l.add ("Its complex stereo image is approximated with unison and stereo effects.");
    if (ref.duration > 6.0) l.add ("Only the overall character of this long recording can be matched.");
    return l.joinIntoString (" ");
}

} // namespace vsx
