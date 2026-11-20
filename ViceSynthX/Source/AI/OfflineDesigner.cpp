#include "OfflineDesigner.h"
#include "../Analysis/SoundMatcher.h"
#include "../Presets/SoundRecipes.h"

namespace vsx
{
using namespace Recipes;
using A = Archetype;

namespace
{
    struct Text
    {
        juce::String s;
        explicit Text (const juce::String& raw)
        {
            auto t = raw.toLowerCase();
            juce::String clean;
            for (auto c : t)
                clean += (juce::CharacterFunctions::isLetterOrDigit (c) || c == '-') ? juce::String::charToString (c) : juce::String (" ");
            s = " " + clean.replace ("-", " ").replace ("  ", " ") + " ";
            while (s.contains ("  ")) s = s.replace ("  ", " ");
        }
        bool has (const char* w) const { return s.contains (" " + juce::String (w) + " "); }
        bool any (std::initializer_list<const char*> ws) const { for (auto* w : ws) if (has (w)) return true; return false; }
        bool phrase (const char* p) const { return s.contains (juce::String (p)); }
        float intensity() const
        {
            if (any ({ "much", "very", "lot", "way", "really", "extremely", "super", "massively", "huge" })) return 1.7f;
            if (any ({ "slightly", "bit", "little", "subtle", "subtly", "touch", "tad", "gently" })) return 0.5f;
            return 1.0f;
        }
    };

    int countWord (const Text& t)
    {
        static const char* words[] = { "zero", "one", "two", "three", "four", "five", "six", "seven", "eight" };
        const auto toks = juce::StringArray::fromTokens (t.s, " ", "");
        for (int i = 0; i < toks.size(); ++i)
            if (toks[i].startsWith ("variation") || toks[i].startsWith ("version") || toks[i].startsWith ("alternative"))
            {
                for (int k = i - 1; k >= std::max (0, i - 3); --k)
                {
                    if (toks[k].containsOnly ("0123456789") && toks[k].isNotEmpty()) return juce::jlimit (1, 8, toks[k].getIntValue());
                    for (int w = 1; w < 9; ++w) if (toks[k] == words[w]) return w;
                }
                return 3;
            }
        return 0;
    }

    int freeOsc (const PatchSpec& s)
    {
        for (int o = 1; o < kNumOscs; ++o)
            if (s.get (oscId (o) + "_on") < 0.5f) return o;
        return -1;
    }

    juce::String titleCase (const juce::String& s)
    {
        juce::StringArray w = juce::StringArray::fromTokens (s, " ", "");
        for (auto& x : w) if (x.isNotEmpty()) x = x.substring (0, 1).toUpperCase() + x.substring (1);
        return w.joinIntoString (" ");
    }

    struct Ctx
    {
        PatchSpec& s;
        juce::StringArray& log;
        juce::StringArray& descriptors;
        juce::StringArray& nameWords;
    };

    bool isFilterOn (const PatchSpec& s, int f) { return s.get ("flt" + juce::String (f + 1) + "_on") > 0.5f; }

    void scaleCutoff (PatchSpec& s, float factor)
    {
        for (int f = 0; f < 2; ++f)
            if (isFilterOn (s, f))
            {
                const auto id = "flt" + juce::String (f + 1) + "_cutoff";
                s.set (id, juce::jlimit (40.0f, 20000.0f, s.get (id) * factor));
            }
        for (int o = 0; o < kNumOscs; ++o)
            if ((int) s.get (oscId (o) + "_mode") == SPECTRAL)
                s.set (oscId (o) + "_spec_tilt", juce::jlimit (-1.0f, 1.0f, s.get (oscId (o) + "_spec_tilt") + (factor > 1.0f ? 0.25f : -0.25f)));
    }

    void ensureLowpass (PatchSpec& s, float cutoff)
    {
        if (! isFilterOn (s, 0) && ! isFilterOn (s, 1))
            filter (s, 0, SVF, LP12, cutoff, 0.1f);
    }

    void addLfoMovement (PatchSpec& s, float k)
    {
        const int l = 6; // LFO 7 reserved for designer movement
        lfo (s, l, 6, 0.12f * k + 0.05f);
        bool any = false;
        for (int o = 0; o < kNumOscs; ++o)
        {
            const int mode = (int) s.get (oscId (o) + "_mode");
            if (s.get (oscId (o) + "_on") > 0.5f && (mode == WT || mode == GRAIN || mode == SPECTRAL))
            {
                s.addMod (modsrc::Lfo1 + l, oscId (o) + "_position", 0.25f * k);
                any = true;
            }
        }
        if (isFilterOn (s, 0)) { s.addMod (modsrc::Lfo1 + l, "flt1_cutoff", 0.08f * k); any = true; }
        if (! any) s.addMod (modsrc::Lfo1 + l, "osc_a_pan", 0.3f * k);
    }

    /** Applies adjective / comparative modifiers. Returns true if anything was recognised. */
    bool applyModifiers (const Text& t, Ctx c)
    {
        auto& s = c.s;
        const float k = t.intensity();
        bool any = false;
        auto note = [&] (const juce::String& m) { c.log.add (m); any = true; };

        const bool clarity = t.any ({ "clarity", "clear", "definition", "presence", "articulate" });

        if (t.any ({ "dark", "darker", "darken", "mellow", "mellower", "duller", "dull", "muffled" }) || t.phrase ("less bright"))
        {
            ensureLowpass (s, 3000.0f);
            scaleCutoff (s, std::pow (0.6f, k));
            c.descriptors.add ("darker"); c.nameWords.add ("Dark");
            note ("Darker: lowered the filter cutoff(s)" + juce::String (k > 1.2f ? " substantially" : "") + ".");
        }
        if (t.any ({ "bright", "brighter", "brighten", "crisp", "crisper", "sparkle", "sparkly", "shiny", "shimmer", "shimmering" }) || t.phrase ("more air"))
        {
            scaleCutoff (s, std::pow (1.6f, k));
            if (k > 1.2f || ! isFilterOn (s, 0)) fx (s, pid::FxEq, 1.0f, { { "high_gain", 3.0f * k }, { "high_freq", 7000.0f } });
            c.descriptors.add ("brighter"); c.nameWords.add ("Bright");
            note ("Brighter: opened the filter and lifted the top end.");
        }
        if (t.any ({ "warm", "warmer", "warmth", "analog", "analogue" }))
        {
            fx (s, pid::FxTube, 0.6f + 0.2f * std::min (1.0f, k), { { "drive", 4.0f + 2.0f * k }, { "bias", 0.35f } });
            s.set ("osc_drift", juce::jlimit (0.0f, 1.0f, s.get ("osc_drift") + 0.15f * k));
            if (clarity)
            {
                fx (s, pid::FxEq, 1.0f, { { "low_gain", 1.5f * k }, { "low_freq", 180.0f }, { "mid2_freq", 3200.0f }, { "mid2_gain", 1.5f }, { "mid2_q", 0.7f } });
                note ("Analog warmth without losing clarity: tube saturation, analog drift and a low-shelf lift, plus a gentle 3.2 kHz presence boost instead of cutting highs.");
            }
            else
            {
                fx (s, pid::FxEq, 1.0f, { { "low_gain", 2.0f * k }, { "low_freq", 160.0f }, { "high_cut", 14000.0f } });
                note ("Warmer: tube saturation, analog drift, low-shelf lift and a soft top-end roll-off.");
            }
            c.nameWords.add ("Warm");
        }
        if (t.any ({ "wide", "wider", "widen", "stereo", "spacious", "panoramic" }) && ! t.phrase ("stereo movement"))
        {
            for (int o = 0; o < kNumOscs; ++o)
                if (s.get (oscId (o) + "_on") > 0.5f)
                {
                    if (s.get (oscId (o) + "_unison") < 2.0f) unison (s, o, 3, std::max (0.08f, s.get (oscId (o) + "_detune")), 0.8f);
                    s.set (oscId (o) + "_spread", std::min (1.0f, s.get (oscId (o) + "_spread") + 0.3f * k));
                }
            fx (s, pid::FxStereo, 1.0f, { { "width", std::min (2.0f, 1.2f + 0.2f * k) }, { "bass_mono", 120.0f } });
            c.descriptors.add ("wider"); c.nameWords.add ("Wide");
            note ("Wider: more unison spread and a mid/side enhancer (bass kept mono below 120 Hz).");
        }
        if (t.any ({ "narrow", "narrower", "focused", "centred", "centered" }) || t.phrase ("mono compatible"))
        {
            for (int o = 0; o < kNumOscs; ++o) s.set (oscId (o) + "_spread", s.get (oscId (o) + "_spread") * 0.4f);
            fx (s, pid::FxStereo, 1.0f, { { "width", 0.8f } });
            note ("Narrower: reduced unison spread and stereo width.");
        }
        if (t.phrase ("stereo movement") || t.phrase ("movement in stereo") || t.any ({ "autopan", "pan" }))
        {
            lfo (s, 7, 0, 0.35f * k + 0.15f);
            int sign = 1;
            for (int o = 0; o < kNumOscs; ++o)
                if (s.get (oscId (o) + "_on") > 0.5f) { s.addMod (modsrc::Lfo8, oscId (o) + "_pan", 0.35f * (float) sign * std::min (1.5f, k)); sign = -sign; }
            fx (s, pid::FxChorus, 0.4f, { { "rate", 0.8f }, { "width", 1.0f } });
            c.descriptors.add ("more-movement");
            note ("More stereo movement: an auto-pan LFO moves the oscillators in opposite directions, plus a wide chorus.");
        }
        const bool softAttack = t.phrase ("soft attack") || t.phrase ("softer attack") || t.phrase ("slow attack") || t.phrase ("slower attack")
                                || t.any ({ "swell", "swelling", "fade" }) || t.phrase ("gentle attack")
                                || (t.has ("attack") && t.any ({ "softer", "soft", "slower", "slow", "gentler", "gentle", "smoother", "longer" }));
        if (softAttack)
        {
            for (int e = 0; e < 2; ++e)
            {
                const auto id = "env" + juce::String (e + 1) + "_attack";
                s.set (id, juce::jlimit (0.05f, 8.0f, std::max (0.12f, s.get (id) * std::pow (3.0f, k))));
            }
            s.set ("env1_attack_curve", -0.3f);
            c.descriptors.add ("softer-attack");
            note ("Softer attack: longer amp and filter attack with a rounded curve.");
        }
        if (! softAttack && (t.any ({ "punchy", "punchier", "snappy", "snappier", "tight", "tighter", "percussive" }) || t.phrase ("fast attack") || t.phrase ("faster attack") || t.phrase ("harder attack")))
        {
            s.set ("env1_attack", 0.002f);
            s.set ("env1_decay", std::max (0.08f, s.get ("env1_decay") * 0.6f));
            if (t.has ("percussive")) s.set ("env1_sustain", 0.0f);
            if (isFilterOn (s, 0) && s.get ("flt1_env_amount") < 0.2f) { s.set ("flt1_env_amount", 0.3f); s.set ("env2_attack", 0.001f); s.set ("env2_decay", 0.25f); }
            c.descriptors.add ("punchier"); c.nameWords.add ("Punchy");
            note ("Punchier: instant attack, shorter decay and a snappy filter envelope.");
        }
        if (t.phrase ("long release") || t.phrase ("longer release") || t.any ({ "tail", "lingering" }) || t.phrase ("longer tail"))
        {
            s.set ("env1_release", juce::jlimit (0.05f, 15.0f, s.get ("env1_release") * 2.0f * k + 0.3f));
            note ("Longer release.");
        }
        if (t.phrase ("short release") || t.phrase ("shorter release") || t.phrase ("less tail"))
        {
            s.set ("env1_release", std::max (0.02f, s.get ("env1_release") * 0.4f));
            note ("Shorter release.");
        }
        if (t.any ({ "reverb", "reverberant", "spacey", "spacy", "huge", "ambient", "hall", "cathedral", "lush", "wet", "wetter", "roomy" }) && ! t.phrase ("less reverb") && ! t.phrase ("no reverb"))
        {
            const float mix = s.get ("fx_reverb_on") > 0.5f ? s.get ("fx_reverb_mix") : 0.15f;
            fx (s, pid::FxReverb, juce::jlimit (0.1f, 0.65f, mix + 0.15f * k),
                { { "size", juce::jlimit (0.3f, 1.0f, s.get ("fx_reverb_size") + 0.15f * k) }, { "decay", juce::jlimit (1.5f, 12.0f, s.get ("fx_reverb_decay") * (1.0f + 0.4f * k)) } });
            c.descriptors.add ("more-space");
            note ("More space: larger, wetter reverb.");
        }
        if (t.any ({ "dry", "drier", "dryer", "intimate" }) || t.phrase ("less reverb") || t.phrase ("no reverb"))
        {
            s.set ("fx_reverb_mix", s.get ("fx_reverb_mix") * 0.35f);
            s.set ("fx_stereodelay_mix", s.get ("fx_stereodelay_mix") * 0.5f);
            c.descriptors.add ("drier");
            note ("Drier: reduced reverb and delay.");
        }
        if (t.any ({ "delay", "echo", "echoes", "echoing" }) && ! t.phrase ("no delay"))
        {
            if (t.any ({ "tape", "dub" })) fx (s, pid::FxTapeDelay, 0.3f);
            else fx (s, pid::FxStereoDelay, 0.25f, { { "pingpong", 1.0f } });
            note ("Added a tempo-synced delay.");
        }
        if (t.any ({ "distorted", "distortion", "aggressive", "gritty", "grit", "dirty", "dirtier", "crunchy", "crunch", "angry", "nasty", "growl", "growling" }))
        {
            fx (s, pid::FxDistortion, 0.5f + 0.2f * std::min (1.0f, k), { { "drive", 10.0f + 6.0f * k }, { "output", -6.0f - 2.0f * k } });
            for (int f = 0; f < 2; ++f) if (isFilterOn (s, f)) s.set ("flt" + juce::String (f + 1) + "_drive", juce::jlimit (0.0f, 1.0f, s.get ("flt" + juce::String (f + 1) + "_drive") + 0.25f * k));
            c.nameWords.add ("Gritty");
            note ("More aggression: distortion stage and hotter filter drive.");
        }
        if (t.any ({ "clean", "cleaner", "pure", "purer", "smooth", "smoother" }))
        {
            s.set ("fx_distortion_on", 0.0f);
            for (int f = 0; f < 2; ++f) s.set ("flt" + juce::String (f + 1) + "_drive", s.get ("flt" + juce::String (f + 1) + "_drive") * 0.3f);
            note ("Cleaner: removed distortion and reduced filter drive.");
        }
        if (t.any ({ "thick", "thicker", "fat", "fatter", "massive", "bigger" }))
        {
            for (int o = 0; o < kNumOscs; ++o)
                if (s.get (oscId (o) + "_on") > 0.5f && (int) s.get (oscId (o) + "_mode") <= SAMPLE)
                    unison (s, o, std::min (8, (int) s.get (oscId (o) + "_unison") + 2), std::min (0.6f, s.get (oscId (o) + "_detune") + 0.08f * k), std::max (0.6f, s.get (oscId (o) + "_spread")));
            if (s.get ("sub_on") < 0.5f) sub (s, 0.35f, -1, 0);
            c.nameWords.add ("Fat");
            note ("Thicker: more unison voices, more detune and a sine sub oscillator.");
        }
        if (t.any ({ "thin", "thinner", "smaller", "lighter" }))
        {
            for (int o = 0; o < kNumOscs; ++o) s.set (oscId (o) + "_unison", 1.0f);
            s.set ("sub_on", 0.0f);
            fx (s, pid::FxEq, 1.0f, { { "low_cut", 220.0f } });
            note ("Thinner: single voices, no sub, low cut at 220 Hz.");
        }
        if (t.any ({ "detuned", "detune", "chorused", "chorus" }))
        {
            for (int o = 0; o < kNumOscs; ++o)
                if (s.get (oscId (o) + "_on") > 0.5f) { if (s.get (oscId (o) + "_unison") < 2.0f) unison (s, o, 3, 0.15f, 0.7f); s.set (oscId (o) + "_detune", std::min (0.7f, s.get (oscId (o) + "_detune") + 0.12f * k)); }
            fx (s, pid::FxChorus, 0.45f);
            note ("More detune and chorus.");
        }
        if (t.any ({ "evolving", "evolve", "moving", "movement", "animated", "alive", "morphing", "shifting" }) && ! t.phrase ("stereo movement"))
        {
            addLfoMovement (s, k);
            c.descriptors.add ("more-movement"); c.nameWords.add ("Evolving");
            note ("Evolving: a slow smooth-random LFO morphs the wavetable position and filter.");
        }
        if (t.any ({ "static", "stable", "steady" }))
        {
            for (int l = 0; l < kNumLfos; ++l)
                for (int i = (int) s.mods.size() - 1; i >= 0; --i)
                    if (s.mods[(size_t) i].source == modsrc::Lfo1 + l && s.mods[(size_t) i].aux == 0) s.removeMod (s.mods[(size_t) i].source, s.mods[(size_t) i].dest);
            note ("Static: removed free-running LFO modulation.");
        }
        if (t.any ({ "vibrato", "wobble", "wobbly" }) && ! t.phrase ("no vibrato"))
        {
            lfo (s, 5, 0, 5.5f);
            for (int o = 0; o < kNumOscs; ++o) if (s.get (oscId (o) + "_on") > 0.5f) s.addMod (modsrc::Lfo6, oscId (o) + "_fine", 0.06f * k);
            note ("Added vibrato (LFO 6, 5.5 Hz).");
        }
        if (t.phrase ("octave up") || t.phrase ("higher octave") || t.phrase ("up an octave") || t.has ("higher"))
        {
            for (int o = 0; o < kNumOscs; ++o) s.set (oscId (o) + "_octave", std::min (4.0f, s.get (oscId (o) + "_octave") + 1.0f));
            note ("Transposed up one octave.");
        }
        if (t.phrase ("octave down") || t.phrase ("lower octave") || t.phrase ("down an octave") || t.has ("lower"))
        {
            for (int o = 0; o < kNumOscs; ++o) s.set (oscId (o) + "_octave", std::max (-4.0f, s.get (oscId (o) + "_octave") - 1.0f));
            note ("Transposed down one octave.");
        }
        if (t.any ({ "glide", "portamento", "slide", "sliding" }))
        {
            voice (s, 2, 0.12f * k, 1);
            note ("Legato with glide.");
        }
        if (t.any ({ "mono", "monophonic" }) && ! t.phrase ("mono compatible")) { s.set ("voice_mode", 1.0f); s.set ("polyphony", 1.0f); note ("Monophonic voice mode."); }
        if (t.any ({ "poly", "polyphonic", "chords", "chordal" })) { s.set ("voice_mode", 0.0f); s.set ("polyphony", 16.0f); note ("Polyphonic voice mode."); }
        if (t.any ({ "airy", "breathy", "breath", "air" }))
        {
            noise (s, 0.06f * k + 0.03f, 1, 0.6f);
            fx (s, pid::FxEq, 1.0f, { { "high_gain", 2.0f }, { "high_freq", 9000.0f } });
            note ("Airier: a soft pink-noise breath layer and a high-shelf lift.");
        }
        if (t.any ({ "metallic", "metal", "inharmonic", "clangy", "clang" }))
        {
            const int o = freeOsc (s);
            if (o > 0) { analog (s, o, 0.0f, 0.0f); pitch (s, o, 0, 7); }
            xmod (s, 0, 1, o > 0 ? o : 1, 0.25f * k);
            fx (s, pid::FxRingMod, 0.15f * k, { { "freq", 640.0f } });
            note ("Metallic: through-zero FM and a touch of ring modulation.");
        }
        if (t.any ({ "hollow", "woody", "clarinet" }))
        {
            if ((int) s.get ("osc_a_mode") == VA) s.set ("osc_a_position", 1.0f);
            else { spectral (s, 0, "Soft Pad", false, s.get ("osc_a_level")); s.set ("osc_a_spec_odd_even", -0.8f); }
            note ("Hollow: odd-harmonic emphasis.");
        }
        if (t.any ({ "lofi", "lo", "vintage", "retro", "tape", "worn", "nostalgic" }))
        {
            fx (s, pid::FxTape, 0.8f, { { "drive", 6.0f }, { "tone", 0.45f } });
            s.set ("osc_drift", juce::jlimit (0.0f, 1.0f, s.get ("osc_drift") + 0.25f));
            if (t.any ({ "lofi", "lo" })) fx (s, pid::FxDistortion, 0.35f, { { "type", 4.0f }, { "drive", 20.0f } });
            c.nameWords.add ("Vintage");
            note ("Vintage character: tape saturation, HF loss and pitch drift.");
        }
        if (t.any ({ "glassy", "glass", "crystal", "crystalline" }))
        {
            const int o = freeOsc (s);
            if (o > 0) { wavetable (s, o, "Glass", 0.5f, 0.3f); pitch (s, o, 1); }
            note ("Glass layer: Glass wavetable an octave up.");
        }
        if (t.any ({ "subby", "bassier", "deeper", "deep" }) || t.phrase ("more bass") || t.phrase ("more low end"))
        {
            sub (s, 0.5f, -1, 0);
            fx (s, pid::FxEq, 1.0f, { { "low_gain", 3.0f }, { "low_freq", 110.0f } });
            note ("More low end: sub oscillator and a low-shelf boost.");
        }
        if (t.any ({ "quieter" }) || t.phrase ("lower volume")) { s.set ("master_volume", s.get ("master_volume") - 3.0f); note ("3 dB quieter."); }
        if (t.any ({ "louder" }) || t.phrase ("more volume")) { s.set ("master_volume", std::min (3.0f, s.get ("master_volume") + 3.0f)); note ("3 dB louder."); }
        if (t.any ({ "plucky" }))
        {
            s.set ("env1_sustain", 0.0f); s.set ("env1_decay", 0.5f); s.set ("env1_attack", 0.002f);
            note ("Plucky envelope.");
        }
        if (t.any ({ "sustained", "sustaining", "held" }))
        {
            s.set ("env1_sustain", std::max (0.8f, s.get ("env1_sustain")));
            note ("Sustained envelope.");
        }
        if (t.phrase ("faster") && ! softAttack)
        {
            for (int l = 0; l < kNumLfos; ++l) s.set ("lfo" + juce::String (l + 1) + "_rate", std::min (40.0f, s.get ("lfo" + juce::String (l + 1) + "_rate") * 2.0f));
            note ("Faster modulation.");
        }
        if (t.has ("slower") && ! softAttack)
        {
            for (int l = 0; l < kNumLfos; ++l) s.set ("lfo" + juce::String (l + 1) + "_rate", std::max (0.01f, s.get ("lfo" + juce::String (l + 1) + "_rate") * 0.5f));
            note ("Slower modulation.");
        }
        return any;
    }

    /** Hardware, artist and era references. */
    bool applyReferences (const Text& t, Ctx c, A arche)
    {
        auto& s = c.s;
        bool any = false;
        auto note = [&] (const juce::String& m) { c.log.add (m); any = true; };
        const bool bassOrLead = arche == A::Bass || arche == A::Lead;

        if (t.any ({ "moog", "minimoog", "ladder" }))
        {
            if (isFilterOn (s, 0) || bassOrLead) { s.set ("flt1_on", 1.0f); s.set ("flt1_model", (float) MG); }
            if (bassOrLead) voice (s, 2, std::max (0.03f, s.get ("glide_time")), 1);
            c.nameWords.add ("Ladder");
            note ("Moog-style character: transistor-ladder filter (MG Ladder model)" + juce::String (bassOrLead ? ", monophonic legato with glide." : "."));
        }
        if (t.any ({ "juno" })) { s.set ("flt1_on", 1.0f); s.set ("flt1_model", (float) JN); fx (s, pid::FxDimension, 1.0f, { { "mode", 1.0f } }); note ("Juno-style: JN OTA filter and dimension chorus."); }
        if (t.any ({ "jupiter" })) { s.set ("flt1_on", 1.0f); s.set ("flt1_model", (float) JP); note ("Jupiter-style: JP OTA 4-pole filter."); }
        if (t.any ({ "prophet", "sequential" })) { s.set ("flt1_on", 1.0f); s.set ("flt1_model", (float) P5); note ("Prophet-style: P5 cascade filter."); }
        if (t.any ({ "ms20", "korg" }) || t.phrase ("ms 20")) { s.set ("flt1_on", 1.0f); s.set ("flt1_model", (float) MS); s.set ("flt1_resonance", std::max (0.5f, s.get ("flt1_resonance"))); note ("MS-20-style: screaming Sallen-Key filter."); }
        if (t.any ({ "oberheim", "sem", "obx" })) { s.set ("flt1_on", 1.0f); s.set ("flt1_model", (float) SEM); note ("Oberheim-style: SEM state-variable filter."); }
        if (t.any ({ "303", "acid", "squelch", "squelchy" }))
        {
            s.set ("flt1_on", 1.0f); s.set ("flt1_model", (float) ACID); s.set ("flt1_resonance", std::max (0.75f, s.get ("flt1_resonance")));
            s.set ("flt1_env_amount", std::max (0.45f, s.get ("flt1_env_amount")));
            c.nameWords.add ("Acid");
            note ("303-style: Acid Diode filter with high resonance and envelope.");
        }
        if (t.any ({ "dx7", "dx", "fm", "yamaha" }))
        {
            if ((int) s.get ("osc_a_mode") != MULTI) { wavetable (s, 0, arche == A::Keys ? "Electric Tine" : "Digital FM", 0.3f, s.get ("osc_a_level")); }
            note ("FM character: FM wavetable on OSC A.");
        }
        if (t.any ({ "jd800", "jd", "d50", "wavestation", "m1" }) || t.phrase ("jd 800") || t.phrase ("d 50"))
        {
            const int o = freeOsc (s);
            if (o > 0) { wavetable (s, o, "Glass", 0.55f, 0.32f); pitch (s, o, 1); unison (s, o, 3, 0.1f, 0.9f); }
            if (t.any ({ "d50" }) || t.phrase ("d 50")) sample (s, 2, "Breath Pad", 0.25f, 60, 1);
            fx (s, pid::FxChorus, 0.45f, { { "rate", 0.5f } });
            fx (s, pid::FxReverb, 0.4f, { { "size", 0.85f }, { "decay", 4.5f } });
            scaleCutoff (s, 1.4f);
            c.nameWords.add ("Digital");
            note ("Inspired by early-90s digital workstations: bright glassy upper layer, chorus and a large hall.");
        }
        if (t.any ({ "vangelis", "cs80", "blade" }) || t.phrase ("cs 80"))
        {
            s.addMod (modsrc::Aftertouch, "flt1_cutoff", 0.3f);
            s.addMod (modsrc::Aftertouch, "osc_a_fine", 0.05f);
            fx (s, pid::FxReverb, 0.5f, { { "size", 0.95f }, { "decay", 7.0f } });
            note ("Expressive poly-brass character: aftertouch opens the filter and adds pitch movement; long hall.");
        }
        if (t.any ({ "trance", "supersaw" }))
        {
            wavetable (s, 0, "Basic Shapes", 0.66f, 0.7f); unison (s, 0, 7, 0.35f, 1.0f, 0.8f);
            note ("Trance supersaw: seven detuned saws.");
        }
        if (t.any ({ "synthwave", "retrowave", "outrun", "miami", "vice" }))
        {
            fx (s, pid::FxChorus, 0.35f);
            fx (s, pid::FxReverb, 0.35f, { { "size", 0.7f }, { "decay", 2.5f } });
            c.nameWords.add ("Neon");
            note ("Synthwave sheen: chorus and a gated-style medium plate.");
        }

        // eras
        const bool eighties = t.any ({ "80s", "1980s", "eighties", "80" });
        const bool nineties = t.any ({ "90s", "1990s", "nineties", "90" });
        const bool seventies = t.any ({ "70s", "1970s", "seventies" });
        const bool mj = t.phrase ("michael jackson") || t.has ("jackson");
        if (mj)
            note ("Michael Jackson reference interpreted as the polished late-80s / early-90s pop production style: bright digital layers, "
                  "tight chorus and glossy reverb. This is an original sound inspired by that era, not a reproduction of any recording.");
        if (nineties || mj)
        {
            const int o = freeOsc (s);
            if (o > 0 && arche == A::Pad) { wavetable (s, o, "Harmonic Sweep", 0.55f, 0.28f); pitch (s, o, 1); }
            fx (s, pid::FxChorus, 0.35f, { { "rate", 0.45f }, { "depth", 0.35f } });
            fx (s, pid::FxReverb, 0.35f, { { "size", 0.8f }, { "decay", 3.5f }, { "damping", 0.35f } });
            fx (s, pid::FxStereoDelay, 0.12f);
            fx (s, pid::FxEq, 1.0f, { { "mid2_freq", 4000.0f }, { "mid2_gain", 1.5f }, { "low_cut", 60.0f } });
            c.nameWords.add (mj ? "90s Pop" : "90s");
            note ("Early-90s pop production profile: digital upper layer, chorus, glossy hall, subtle delay and presence EQ.");
        }
        if (eighties)
        {
            fx (s, pid::FxChorus, 0.4f);
            fx (s, pid::FxReverb, 0.3f, { { "size", 0.5f }, { "decay", 2.2f } });
            scaleCutoff (s, 1.25f);
            c.nameWords.add ("80s");
            note ("1980s profile: bright, chorused, with a plate-like reverb.");
        }
        if (seventies)
        {
            s.set ("osc_drift", 0.35f);
            fx (s, pid::FxTape, 0.7f);
            fx (s, pid::FxPhaser, 0.3f);
            c.nameWords.add ("70s");
            note ("1970s profile: analog drift, tape saturation and phaser.");
        }
        if (t.any ({ "cinematic", "epic", "film", "movie", "trailer" }))
        {
            fx (s, pid::FxReverb, 0.45f, { { "size", 0.95f }, { "decay", 7.0f } });
            if (s.get ("env1_attack") < 0.3f && arche != A::Sfx && arche != A::Bass && arche != A::Pluck) s.set ("env1_attack", 0.6f);
            addLfoMovement (s, 0.8f);
            c.nameWords.add ("Cinematic");
            note ("Cinematic scale: long hall, slow attack and slow evolving modulation.");
        }
        return any;
    }

    struct ArcheGuess { bool found = false; A a = A::Pad; int variant = 0; juce::String label; };

    ArcheGuess detectArchetype (const Text& t)
    {
        ArcheGuess g;
        auto set = [&] (A a, int v, const char* label) { g.found = true; g.a = a; g.variant = v; g.label = label; };
        if (t.any ({ "riser", "rise", "uplifter", "build" })) set (A::Sfx, 0, "Riser");
        else if (t.any ({ "laser", "zap", "pew" })) set (A::Sfx, 1, "Laser");
        else if (t.any ({ "impact", "boom", "hit", "explosion" })) set (A::Sfx, 2, "Impact");
        else if (t.any ({ "siren", "alarm" })) set (A::Sfx, 3, "Siren");
        else if (t.any ({ "sequence", "sequenced", "sequencer" })) set (A::Sequence, 0, "Sequence");
        else if (t.any ({ "arp", "arpeggio", "arpeggiated", "arpeggiator" })) set (A::Arp, 0, "Arp");
        else if (t.any ({ "bass", "bassline", "808", "reese" }) || t.phrase (" sub bass")) set (A::Bass, 0, "Bass");
        else if (t.any ({ "choir", "choirs", "vocal", "vocals", "voice", "voices", "vox", "aah", "ooh" })) set (A::Choir, 0, "Choir");
        else if (t.any ({ "pad", "pads", "atmosphere", "atmospheric", "wash", "bed" })) set (A::Pad, 0, "Pad");
        else if (t.any ({ "string", "strings", "violin", "violins", "cello", "cellos", "orchestra", "orchestral", "ensemble" })) set (A::Strings, 0, "Strings");
        else if (t.any ({ "brass", "horn", "horns", "trumpet", "trumpets", "trombone", "stab" })) set (A::Brass, 0, "Brass");
        else if (t.any ({ "lead", "solo", "melody" })) set (A::Lead, 0, "Lead");
        else if (t.any ({ "pluck", "plucks", "plucked", "pizz", "pizzicato", "guitar", "kalimba" })) set (A::Pluck, 0, "Pluck");
        else if (t.any ({ "organ", "hammond", "drawbar" })) set (A::Organ, 0, "Organ");
        else if (t.any ({ "piano", "keys", "keyboard", "rhodes", "ep", "wurli", "clav", "clavinet" }) || t.phrase ("electric piano")) set (A::Keys, 0, "Keys");
        else if (t.any ({ "bell", "bells", "chime", "chimes", "mallet", "mallets", "glockenspiel", "vibraphone", "marimba" }) || t.phrase ("music box")) set (A::Bell, 0, "Bell");
        else if (t.any ({ "drone", "drones" })) set (A::Drone, 0, "Drone");
        else if (t.any ({ "texture", "textures", "soundscape", "granular", "ambience" })) set (A::Texture, 0, "Texture");
        else if (t.any ({ "fx", "sfx", "effect", "whoosh", "sweep" })) set (A::Sfx, 0, "Sound Effect");
        if (! g.found) return g;

        // style -> variant
        switch (g.a)
        {
            case A::Bass:
                if (t.any ({ "acid", "303", "squelch" })) g.variant = 1;
                else if (t.any ({ "reese", "dnb", "neuro" }) || t.phrase ("drum and bass")) g.variant = 2;
                else if (t.any ({ "fm", "digital", "dx", "dx7" })) g.variant = 3;
                else if (t.any ({ "sub", "808", "deep" })) g.variant = 4;
                else if (t.any ({ "wobble", "dubstep", "growl", "growling" })) g.variant = 5;
                break;
            case A::Pad:
                if (t.any ({ "string", "strings" })) g.variant = 5;
                else if (t.any ({ "choir", "vocal" })) g.variant = 4;
                else if (t.any ({ "glass", "glassy", "crystal" })) g.variant = 3;
                else if (t.any ({ "evolving", "cinematic", "moving", "vowel", "formant" })) g.variant = 2;
                else if (t.any ({ "digital", "bright", "shimmer", "shimmering", "90s", "1990s", "jd800", "jd", "d50", "wavetable" }) || t.phrase ("jd 800")) g.variant = 1;
                break;
            case A::Lead:
                if (t.any ({ "square", "pwm", "pulse" })) g.variant = 1;
                else if (t.any ({ "sync" })) g.variant = 2;
                else if (t.any ({ "fm", "whistle", "digital" })) g.variant = 3;
                else if (t.any ({ "scream", "screaming", "ms20", "aggressive" })) g.variant = 4;
                break;
            case A::Pluck:
                if (t.any ({ "digital", "fm", "bell" })) g.variant = 1;
                else if (t.any ({ "kalimba", "thumb" })) g.variant = 2;
                else if (t.any ({ "guitar", "nylon", "acoustic" })) g.variant = 3;
                break;
            case A::Keys:
                if (t.any ({ "piano", "felt", "grand", "upright", "acoustic" }) && ! t.phrase ("electric piano")) g.variant = 1;
                else if (t.any ({ "fm", "dx", "dx7", "digital" })) g.variant = 2;
                else if (t.any ({ "clav", "clavinet", "funky", "funk" })) g.variant = 3;
                break;
            case A::Organ: g.variant = t.any ({ "house", "bass", "stab" }) ? 1 : 0; break;
            case A::Strings:
                if (t.any ({ "analog", "machine", "solina", "synth" })) g.variant = 1;
                else if (t.any ({ "spectral", "additive" })) g.variant = 2;
                break;
            case A::Choir:
                if (t.any ({ "ooh", "soft" })) g.variant = 1;
                else if (t.any ({ "synth", "robot", "vowel", "digital" })) g.variant = 2;
                break;
            case A::Brass:
                if (t.any ({ "synth", "analog", "stab", "80s", "1980s", "jupiter", "prophet", "oberheim", "vangelis", "cs80" })) g.variant = 1;
                else if (t.any ({ "wavetable", "digital" })) g.variant = 2;
                break;
            case A::Bell:
                if (t.any ({ "mallet", "marimba", "vibraphone", "glockenspiel", "music" })) g.variant = 1;
                else if (t.any ({ "bowl", "singing", "meditation", "tibetan" })) g.variant = 2;
                break;
            case A::Texture:
                if (t.any ({ "breath", "breathy", "air" })) g.variant = 1;
                else if (t.any ({ "spectral", "scan" })) g.variant = 2;
                else if (t.any ({ "vinyl", "lofi", "nostalgic" })) g.variant = 3;
                break;
            default: break;
        }
        if (g.a == A::Pad && t.any ({ "cinematic", "evolving" }) && t.any ({ "texture" })) { g.a = A::Texture; g.variant = 0; g.label = "Texture"; }
        return g;
    }

    juce::String categoryFor (A a)
    {
        switch (a)
        {
            case A::Bass: return "Bass";
            case A::Pad: return "Pads";
            case A::Lead: return "Leads";
            case A::Pluck: return "Plucks";
            case A::Keys: case A::Organ: return "Keys";
            case A::Strings: return "Strings";
            case A::Choir: return "Choirs";
            case A::Brass: return "Brass";
            case A::Bell: return "Bells";
            case A::Texture: return "Textures";
            case A::Drone: return "Cinematic";
            case A::Sfx: return "Sound Effects";
            case A::Sequence: case A::Arp: return "Sequences";
        }
        return "Digital";
    }

    juce::String realismNote (const Text& t, A a)
    {
        if ((a == A::Strings || a == A::Choir || a == A::Brass || a == A::Keys) && t.any ({ "realistic", "real", "orchestral", "authentic", "acoustic" }))
            return "Limitation: VICE SYNTH X ships original synthesised instrument multisamples, not recordings of real orchestras, choirs or pianos. "
                   "The result is an expressive synthetic approximation; load your own licensed samples (WAV/AIFF/SFZ) for full realism.";
        return {};
    }
}

// ---------------------------------------------------------------------------------
std::vector<PatchSpec> OfflineDesigner::makeVariations (const PatchSpec& base, int count, uint32_t seed, juce::StringArray& notes)
{
    std::vector<PatchSpec> out;
    Rng rng (seed ? seed : 1234u);
    static const int models[] = { MG, JP, JN, P5, SEM, SVF, DIODE };
    for (int i = 0; i < count; ++i)
    {
        PatchSpec v = base;
        juce::StringArray changes;
        const int numChanges = 2 + (int) (rng.next() % 3);
        for (int c = 0; c < numChanges; ++c)
        {
            switch (rng.next() % 9)
            {
                case 0: { const float f = std::exp2 (rng.bi() * 0.9f); scaleCutoff (v, f); changes.add (f > 1 ? "brighter filter" : "darker filter"); break; }
                case 1: v.set ("flt1_resonance", juce::jlimit (0.0f, 0.85f, v.get ("flt1_resonance") + rng.bi() * 0.2f)); changes.add ("resonance"); break;
                case 2:
                    for (int o = 0; o < kNumOscs; ++o) v.set (oscId (o) + "_position", juce::jlimit (0.0f, 1.0f, v.get (oscId (o) + "_position") + rng.bi() * 0.3f));
                    changes.add ("wavetable/shape position"); break;
                case 3:
                    for (int o = 0; o < kNumOscs; ++o) if (v.get (oscId (o) + "_on") > 0.5f) unison (v, o, juce::jlimit (1, 9, (int) v.get (oscId (o) + "_unison") + (int) (rng.next() % 5) - 2), juce::jlimit (0.0f, 0.6f, v.get (oscId (o) + "_detune") + rng.bi() * 0.1f), v.get (oscId (o) + "_spread"));
                    changes.add ("unison"); break;
                case 4: v.set ("env1_attack", juce::jlimit (0.001f, 4.0f, v.get ("env1_attack") * std::exp2 (rng.bi() * 1.5f))); v.set ("env1_release", juce::jlimit (0.02f, 8.0f, v.get ("env1_release") * std::exp2 (rng.bi()))); changes.add ("envelope"); break;
                case 5: if (v.get ("flt1_on") > 0.5f && (int) v.get ("flt1_model") != ACID) { const int m = models[rng.next() % 7]; v.set ("flt1_model", (float) m); changes.add ("filter model " + choices::filterModels[m]); } break;
                case 6: fx (v, pid::FxChorus, 0.2f + 0.3f * rng.uni()); changes.add ("chorus"); break;
                case 7: fx (v, pid::FxReverb, juce::jlimit (0.1f, 0.6f, v.get ("fx_reverb_mix") + rng.bi() * 0.15f)); changes.add ("reverb"); break;
                default:
                {
                    const int o = 1 + (int) (rng.next() % 2);
                    if (v.get (oscId (o) + "_on") > 0.5f) { v.set (oscId (o) + "_octave", juce::jlimit (-2.0f, 2.0f, v.get (oscId (o) + "_octave") + (rng.uni() > 0.5f ? 1.0f : -1.0f))); changes.add ("layer octave"); }
                    else { addLfoMovement (v, 0.6f); changes.add ("slow movement"); }
                    break;
                }
            }
        }
        v.meta.name = base.meta.name + " Var " + juce::String (i + 1);
        v.meta.uuid = juce::Uuid().toString();
        v.meta.description = "Variation of " + base.meta.name + ": " + changes.joinIntoString (", ");
        notes.add (v.meta.name + ": " + changes.joinIntoString (", "));
        out.push_back (v);
    }
    return out;
}

DesignResult OfflineDesigner::design (const juce::String& prompt, const PatchSpec& current, const AudioFeatures* reference) const
{
    DesignResult r;
    r.engine = "Offline designer";
    const Text t (prompt);
    juce::StringArray log, nameWords;

    // reference-audio requests
    if (reference != nullptr && reference->valid && t.any ({ "reference", "match", "similar", "like", "sample", "loaded", "audio" }))
    {
        int note = 60;
        r.patch = SoundMatcher::initialGuess (*reference, note);
        r.patch.meta.name = "Reference Match";
        r.patch.meta.prompt = prompt;
        r.hasPatch = true;
        r.descriptors.add ("match-reference");
        r.reply = "Built a first approximation from the reference analysis (harmonic spectrum -> custom wavetable, measured envelope, "
                  "brightness, vibrato/tremolo and width). Use 'Match' to refine it by rendering and comparing candidates. "
                  + SoundMatcher::limitationsFor (*reference);
        return r;
    }

    const int variations = countWord (t);
    const auto guess = detectArchetype (t);
    const bool refersCurrent = t.any ({ "this", "it", "current", "existing" }) || t.phrase ("the sound") || t.phrase ("the preset") || t.phrase ("the patch");
    const bool creationVerb = t.any ({ "create", "make", "design", "build", "give", "generate", "want", "need", "new", "craft", "program" });
    const bool newPatch = guess.found && (creationVerb || ! refersCurrent);

    PatchSpec spec = current;
    if (newPatch)
    {
        spec = PatchOps::initPatch();
        archetype (spec, guess.a, guess.variant);
        log.add ("Base: " + archetypeName (guess.a) + " (" + juce::String (guess.variant + 1) + ") built from the recipe library.");
    }

    Ctx c { spec, log, r.descriptors, nameWords };
    const bool refs = applyReferences (t, c, newPatch ? guess.a : A::Pad);
    const bool mods = applyModifiers (t, c);
    const bool eraSuit = t.any ({ "suitable", "fit", "fits", "production", "mix", "track" });

    if (! newPatch && ! refs && ! mods && variations == 0)
    {
        r.question = "What kind of sound would you like? For example: a warm analog pad, a punchy acid bass, a glassy digital bell, "
                     "or a change to the current preset such as 'make it darker and wider'.";
        r.reply = r.question;
        return r;
    }

    if (newPatch)
    {
        juce::StringArray name;
        for (const auto& w : nameWords) name.addIfNotAlreadyThere (w);
        while (name.size() > 2) name.remove (0);
        name.add (guess.label);
        spec.meta.name = titleCase (name.joinIntoString (" "));
        spec.meta.category = categoryFor (guess.a);
        spec.meta.tags = nameWords;
        spec.meta.tags.add (archetypeName (guess.a).toLowerCase());
    }
    else if (refs || mods || eraSuit)
    {
        if (! spec.meta.name.endsWith ("(edit)")) spec.meta.name = spec.meta.name + " (edit)";
    }

    spec.meta.source = "ai";
    spec.meta.author = "AI Sound Designer (offline)";
    spec.meta.prompt = prompt;
    spec.meta.uuid = juce::Uuid().toString();
    spec.meta.description = log.joinIntoString (" ");
    r.patch = spec;
    r.hasPatch = newPatch || refs || mods;

    if (variations > 0)
    {
        juce::StringArray varNotes;
        r.variations = makeVariations (r.hasPatch ? spec : current, variations, (uint32_t) prompt.hashCode(), varNotes);
        log.add ("Created " + juce::String (variations) + " variations: " + varNotes.joinIntoString ("; ") + ".");
        if (! r.hasPatch) r.patch = current;
    }

    juce::String reply;
    if (newPatch) reply << "Created \"" << spec.meta.name << "\". ";
    else if (r.hasPatch) reply << "Updated the current sound. ";
    reply << log.joinIntoString (" ");
    const auto real = realismNote (t, newPatch ? guess.a : A::Pad);
    if (real.isNotEmpty()) reply << " " << real;
    r.reply = reply.trim();
    return r;
}

} // namespace vsx
