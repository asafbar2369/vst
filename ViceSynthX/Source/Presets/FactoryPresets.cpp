// VICE SYNTH X factory library: original sound design built from code.
#include "PresetManager.h"
#include "SoundRecipes.h"

namespace vsx::FactoryPresets
{
using namespace Recipes;
using pid::FxDistortion; using pid::FxTube; using pid::FxTape; using pid::FxEq; using pid::FxMbComp; using pid::FxFreqShift;
using pid::FxRingMod; using pid::FxChorus; using pid::FxDimension; using pid::FxFlanger; using pid::FxPhaser; using pid::FxStereoDelay;
using pid::FxTapeDelay; using pid::FxDigitalDelay; using pid::FxReverb; using pid::FxConvolution; using pid::FxStereo; using pid::FxLimiter;
using A = Archetype;

juce::StringArray categories()
{
    return { "Analog", "Digital", "Bass", "Pads", "Leads", "Keys", "Plucks", "Strings", "Choirs", "Brass", "Bells", "Textures",
             "Cinematic", "Sound Effects", "Sequences", "Vintage 1980s", "Vintage 1990s" };
}

namespace
{
    float levelTrim (const juce::String& name);

    struct Lib
    {
        std::vector<PatchSpec> v;
        template <typename Fn>
        void add (const char* name, const char* category, std::initializer_list<const char*> tags, const char* desc, Fn&& fn)
        {
            PatchSpec s = init (name, category);
            fn (s);
            s.meta.name = name;
            s.meta.category = category;
            s.meta.description = desc;
            s.meta.source = "factory";
            s.meta.author = "VICE Factory";
            s.meta.tags.clear();
            for (auto* t : tags) s.meta.tags.add (t);
            s.set ("master_volume", levelTrim (name));
            v.push_back (std::move (s));
        }
    };

    /** Loudness calibration: master volume per preset so the loudest 400 ms window sits near
        -16 dBFS RMS with peaks <= -4 dBFS (measured offline by the calibration test, see Tests/TestPresets.cpp). */
    float levelTrim (const juce::String& name)
    {
        static const std::map<juce::String, float> trims {
        { "Ocean Drive Poly", -9.1f },
        { "Ladder Saw Stack", -2.7f },
        { "PWM Velvet", -13.6f },
        { "Five Voice Brass", -4.1f },
        { "Diode Growler", -1.5f },
        { "Glass Harmonics", -17.5f },
        { "FM Tine Machine", -6.5f },
        { "Spectral Bloom", -12.4f },
        { "Bitcrush Arcade", 2.0f },
        { "Comb Morph Keys", -2.5f },
        { "Ladder Mono Bass", -1.1f },
        { "Acid Line 303", -5.6f },
        { "Reese Tide", -3.5f },
        { "FM Pluck Bass", -1.2f },
        { "Neon Wobble", -0.2f },
        { "Sunset Boulevard Pad", -10.5f },
        { "Crystal Sky", -6.2f },
        { "Vowel Drift", -21.9f },
        { "Angel Choir Pad", -8.4f },
        { "Velvet Strings Pad", -5.3f },
        { "Miami Hero Lead", -6.3f },
        { "Square Runner", -5.4f },
        { "Sync Screamer", -10.4f },
        { "FM Whistle", -11.8f },
        { "Korg Scream", 5.4f },
        { "Tine Electric Piano", -3.1f },
        { "Felt Upright", 1.8f },
        { "Digital EP", -2.6f },
        { "Funky Clav", -2.5f },
        { "Club Organ", -5.6f },
        { "Palm Pluck", 1.6f },
        { "Tine Drops", 0.3f },
        { "Kalimba Sun", 2.3f },
        { "Nylon Guitar", 5.4f },
        { "Glass Pizz", -0.1f },
        { "Ensemble Section", -2.6f },
        { "Solina Night", -7.7f },
        { "Spectral Violins", -12.8f },
        { "Staccato Strings", 6.0f },
        { "Cinema Swell", -10.6f },
        { "Cathedral Aah", -11.4f },
        { "Soft Ooh", -2.7f },
        { "Synth Vox", -8.6f },
        { "Breath Choir", -11.4f },
        { "Dark Monks", -5.3f },
        { "Brass Section", 1.9f },
        { "Synth Brass Stab", -2.2f },
        { "Wavetable Horns", 0.3f },
        { "Vangelis Blade", -5.5f },
        { "Horn Swell", 1.6f },
        { "FM Glass Bells", -7.2f },
        { "Crystal Mallets", -1.8f },
        { "Singing Bowl", -4.1f },
        { "Tubular Night", -8.1f },
        { "Music Box", -4.0f },
        { "Granular Fog", -1.7f },
        { "Breathing Room", -3.7f },
        { "Spectral Tides", -16.2f },
        { "Lo-Fi Memory", -11.3f },
        { "Neon Rain", -1.0f },
        { "Deep Space Drone", -17.9f },
        { "Trailer Impact", -16.2f },
        { "Tension Riser", -9.5f },
        { "Heroic Swell", -11.7f },
        { "Vice City Dusk", -13.5f },
        { "Laser Zap", 6.0f },
        { "Police Siren", -4.5f },
        { "White Noise Sweep", 6.0f },
        { "Robot Talk", -2.8f },
        { "Arcade Coin", 4.3f },
        { "Night Drive Sequence", -3.9f },
        { "Arp Lights", -5.8f },
        { "Bassline Engine", -3.5f },
        { "Kalimba Arp", -3.9f },
        { "Gated Chord Seq", -11.2f },
        { "Eighties Poly Brass", -3.5f },
        { "Crystal EP 85", -5.1f },
        { "Synthwave Lead", -8.9f },
        { "Pop Synth Pad 84", -10.7f },
        { "Gated Drum Synth", -2.9f },
        { "Nineties Digital Pad", -4.0f },
        { "Rave Hoover", -7.7f },
        { "House Organ Bass", -2.7f },
        { "Trance Supersaw", -12.3f },
        { "Pop Bell Pad 92", -7.5f },
        };
        auto it = trims.find (name);
        return it != trims.end() ? it->second : -3.0f;
    }

    void reverb (PatchSpec& s, float mix, float size, float decay) { fx (s, FxReverb, mix, { { "size", size }, { "decay", decay } }); }

    std::vector<PatchSpec> build()
    {
        Lib L;

        // ---------------- Analog -------------------------------------------------------
        L.add ("Ocean Drive Poly", "Analog", { "poly", "warm", "juno" }, "Classic warm poly: detuned saws through the JN OTA filter with dimension chorus.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 0); env (s, 0, 0.02f, 1.2f, 0.75f, 0.6f); s.p ("flt1_cutoff", 3200.0f).p ("flt1_env_amount", 0.18f); });
        L.add ("Ladder Saw Stack", "Analog", { "saw", "ladder", "fat" }, "Three detuned analog saws into a driven transistor ladder.", [] (PatchSpec& s)
        { analog (s, 0, 0.67f, 0.7f); unison (s, 0, 3, 0.2f, 0.6f); analog (s, 1, 0.67f, 0.5f); pitch (s, 1, -1); filter (s, 0, MG, LP24, 1500.0f, 0.3f, 0.5f, 0.3f); env (s, 1, 0.005f, 0.8f, 0.4f, 0.4f); env (s, 0, 0.005f, 0.5f, 0.8f, 0.3f); });
        L.add ("PWM Velvet", "Analog", { "pwm", "soft", "pad" }, "Slow PWM with a smooth SEM low-pass and long release.", [] (PatchSpec& s)
        { analog (s, 0, 1.0f, 0.75f, 0.4f); unison (s, 0, 2, 0.08f, 0.7f); lfo (s, 0, 1, 0.35f); s.addMod (modsrc::Lfo1, "osc_a_pulse_width", 0.25f); filter (s, 0, SEM, LP24, 2400.0f, 0.2f); env (s, 0, 0.3f, 1.0f, 0.8f, 1.2f); fx (s, FxChorus, 0.4f); reverb (s, 0.3f, 0.7f, 3.0f); });
        L.add ("Five Voice Brass", "Analog", { "brass", "prophet", "poly" }, "Punchy poly-brass in the spirit of late-70s five-voice synths (P5 cascade filter).", [] (PatchSpec& s)
        { archetype (s, A::Brass, 1); s.p ("flt1_model", (float) P5).p ("flt1_cutoff", 1000.0f); });
        L.add ("Diode Growler", "Analog", { "diode", "aggressive", "mono" }, "Square + saw through a resonant diode ladder with glide.", [] (PatchSpec& s)
        { archetype (s, A::Bass, 0); s.p ("flt1_model", (float) DIODE).p ("flt1_resonance", 0.6f).p ("flt1_cutoff", 500.0f); pitch (s, 0, 1); pitch (s, 1, 0); });

        // ---------------- Digital ------------------------------------------------------
        L.add ("Glass Harmonics", "Digital", { "wavetable", "bright", "glass" }, "Glass wavetable scanned by a slow LFO with stereo delay.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 3); env (s, 0, 0.05f, 2.0f, 0.6f, 2.0f); });
        L.add ("FM Tine Machine", "Digital", { "fm", "keys", "digital" }, "Two-operator style FM tine with velocity brightness.", [] (PatchSpec& s)
        { archetype (s, A::Keys, 2); s.addMod (modsrc::Velocity, "osc_a_position", 0.3f); });
        L.add ("Spectral Bloom", "Digital", { "spectral", "evolving", "additive" }, "Additive resynthesis of the Soft Pad table with moving tilt and formant.", [] (PatchSpec& s)
        { spectral (s, 0, "Soft Pad", false, 0.75f); unison (s, 0, 3, 0.1f, 0.8f); lfo (s, 0, 0, 0.09f); s.addMod (modsrc::Lfo1, "osc_a_spec_tilt", 0.5f); lfo (s, 1, 6, 0.13f); s.addMod (modsrc::Lfo2, "osc_a_spec_formant", 0.2f); filterOff (s, 0); env (s, 0, 0.9f, 1.0f, 0.9f, 2.0f); reverb (s, 0.4f, 0.85f, 6.0f); });
        L.add ("Bitcrush Arcade", "Digital", { "chip", "8-bit", "lead" }, "Stepped chip waveforms with a quantise warp.", [] (PatchSpec& s)
        { wavetable (s, 0, "Chip Pulse", 0.2f, 0.7f); warp (s, 0, 7, 0.3f); filterOff (s, 0); voice (s, 1, 0.03f, 1); env (s, 0, 0.001f, 0.3f, 0.7f, 0.1f); fx (s, FxDigitalDelay, 0.25f, { { "crush", 0.5f } }); });
        L.add ("Comb Morph Keys", "Digital", { "comb", "wavetable", "keys" }, "Comb-sweep wavetable with envelope-driven position.", [] (PatchSpec& s)
        { wavetable (s, 0, "Comb Sweep", 0.1f, 0.75f); env (s, 2, 0.0f, 0.9f, 0.2f, 0.6f); s.addMod (modsrc::Env3, "osc_a_position", 0.6f); filter (s, 0, SVF, LP12, 7000.0f, 0.1f); env (s, 0, 0.002f, 1.5f, 0.3f, 0.6f); reverb (s, 0.2f, 0.6f, 2.0f); });

        // ---------------- Bass ---------------------------------------------------------
        L.add ("Ladder Mono Bass", "Bass", { "moog", "mono", "classic" }, "Saw + square + sub through a driven transistor ladder with filter envelope.", [] (PatchSpec& s) { archetype (s, A::Bass, 0); });
        L.add ("Acid Line 303", "Bass", { "acid", "303", "squelch" }, "Squelchy acid bass: Acid Diode filter with high resonance, accent via velocity.", [] (PatchSpec& s) { archetype (s, A::Bass, 1); });
        L.add ("Reese Tide", "Bass", { "reese", "dnb", "detuned" }, "Detuned stacked saws with slow filter motion.", [] (PatchSpec& s) { archetype (s, A::Bass, 2); });
        L.add ("FM Pluck Bass", "Bass", { "fm", "digital", "punchy" }, "Digital FM bass with envelope-swept index.", [] (PatchSpec& s) { archetype (s, A::Bass, 3); });
        L.add ("Neon Wobble", "Bass", { "wobble", "dubstep", "synced" }, "Tempo-synced growl wobble with MS filter and distortion.", [] (PatchSpec& s) { archetype (s, A::Bass, 5); });

        // ---------------- Pads ----------------------------------------------------------
        L.add ("Sunset Boulevard Pad", "Pads", { "warm", "lush", "analog" }, "Wide analog saw pad with dimension chorus and hall reverb.", [] (PatchSpec& s) { archetype (s, A::Pad, 0); });
        L.add ("Crystal Sky", "Pads", { "digital", "shimmer", "bright" }, "Harmonic-sweep wavetable pad with glass layer and long reverb.", [] (PatchSpec& s) { archetype (s, A::Pad, 1); });
        L.add ("Vowel Drift", "Pads", { "evolving", "formant", "spectral" }, "Formant wavetable and spectral layer drifting through vowels.", [] (PatchSpec& s) { archetype (s, A::Pad, 2); });
        L.add ("Angel Choir Pad", "Pads", { "choir", "vocal", "airy" }, "Built-in choir multisample blended with a vocal wavetable.", [] (PatchSpec& s) { archetype (s, A::Pad, 4); });
        L.add ("Velvet Strings Pad", "Pads", { "strings", "soft", "cinematic" }, "Ensemble strings layered with soft analog saws.", [] (PatchSpec& s) { archetype (s, A::Pad, 5); });

        // ---------------- Leads -----------------------------------------------------------
        L.add ("Miami Hero Lead", "Leads", { "saw", "classic", "glide" }, "Detuned saw lead with ladder filter, glide and mod-wheel vibrato.", [] (PatchSpec& s) { archetype (s, A::Lead, 0); });
        L.add ("Square Runner", "Leads", { "pwm", "square", "retro" }, "PWM square lead with an IR3109-style OTA filter.", [] (PatchSpec& s) { archetype (s, A::Lead, 1); });
        L.add ("Sync Screamer", "Leads", { "sync", "aggressive", "rock" }, "Sweeping hard-sync wavetable with envelope and drive.", [] (PatchSpec& s) { archetype (s, A::Lead, 2); });
        L.add ("FM Whistle", "Leads", { "fm", "thin", "expressive" }, "Through-zero FM between oscillators for a whistling digital lead.", [] (PatchSpec& s) { archetype (s, A::Lead, 3); });
        L.add ("Korg Scream", "Leads", { "ms20", "resonant", "dirty" }, "Saw into a screaming Sallen-Key filter with nonlinear feedback.", [] (PatchSpec& s) { archetype (s, A::Lead, 4); });

        // ---------------- Keys --------------------------------------------------------------
        L.add ("Tine Electric Piano", "Keys", { "ep", "tine", "chorus" }, "Built-in tine EP multisample with auto-pan and chorus.", [] (PatchSpec& s) { archetype (s, A::Keys, 0); });
        L.add ("Felt Upright", "Keys", { "piano", "felt", "intimate" }, "Soft modal-synthesis felt piano.", [] (PatchSpec& s) { archetype (s, A::Keys, 1); });
        L.add ("Digital EP", "Keys", { "fm", "ep", "80s" }, "FM-style electric piano with decaying modulation index.", [] (PatchSpec& s) { archetype (s, A::Keys, 2); });
        L.add ("Funky Clav", "Keys", { "clav", "funk", "pulse" }, "Narrow pulse through an enveloped ladder plus phaser.", [] (PatchSpec& s) { archetype (s, A::Keys, 3); });
        L.add ("Club Organ", "Keys", { "organ", "house", "drawbar" }, "Drawbar organ with rotary-style chorus and tube drive.", [] (PatchSpec& s) { archetype (s, A::Organ, 0); });

        // ---------------- Plucks -------------------------------------------------------------
        L.add ("Palm Pluck", "Plucks", { "analog", "pluck", "delay" }, "Snappy analog pluck with tempo delay.", [] (PatchSpec& s) { archetype (s, A::Pluck, 0); });
        L.add ("Tine Drops", "Plucks", { "digital", "bell", "pluck" }, "Electric-tine wavetable pluck.", [] (PatchSpec& s) { archetype (s, A::Pluck, 1); });
        L.add ("Kalimba Sun", "Plucks", { "kalimba", "sample", "organic" }, "Modal kalimba sample with a sine overtone.", [] (PatchSpec& s) { archetype (s, A::Pluck, 2); });
        L.add ("Nylon Guitar", "Plucks", { "guitar", "nylon", "physical" }, "Karplus-Strong nylon pluck multisample.", [] (PatchSpec& s) { archetype (s, A::Pluck, 3); s.p ("osc_a_level", 1.0f); });
        L.add ("Glass Pizz", "Plucks", { "glass", "pizzicato", "short" }, "Glass wavetable with a fast filter envelope.", [] (PatchSpec& s)
        { archetype (s, A::Pluck, 0); wavetable (s, 0, "Glass", 0.3f, 0.75f); s.p ("osc_b_on", 0.0f).p ("flt1_model", (float) SVF).p ("flt1_cutoff", 800.0f); });

        // ---------------- Strings -----------------------------------------------------------
        L.add ("Ensemble Section", "Strings", { "ensemble", "orchestral", "sample" }, "Built-in ensemble strings with vibrato on the mod wheel.", [] (PatchSpec& s) { archetype (s, A::Strings, 0); });
        L.add ("Solina Night", "Strings", { "string machine", "analog", "vintage" }, "String-machine style saws with deep dimension chorus.", [] (PatchSpec& s) { archetype (s, A::Strings, 1); });
        L.add ("Spectral Violins", "Strings", { "spectral", "additive", "expressive" }, "Additive strings resynthesised from a bowed spectrum.", [] (PatchSpec& s) { archetype (s, A::Strings, 2); });
        L.add ("Staccato Strings", "Strings", { "short", "ensemble", "rhythmic" }, "Short ensemble articulation for rhythmic parts.", [] (PatchSpec& s)
        { archetype (s, A::Strings, 0); s.p ("osc_a_position", 0.09f).p ("osc_a_level", 1.0f); env (s, 0, 0.004f, 0.5f, 0.0f, 0.15f); });
        L.add ("Cinema Swell", "Strings", { "swell", "slow", "film" }, "Slow-attack strings with filter swell.", [] (PatchSpec& s)
        { archetype (s, A::Strings, 1); env (s, 0, 1.5f, 1.0f, 1.0f, 2.5f); env (s, 1, 2.0f, 1.0f, 0.8f, 2.0f); s.p ("flt1_env_amount", 0.25f).p ("flt1_cutoff", 1500.0f); });

        // ---------------- Choirs --------------------------------------------------------------
        L.add ("Cathedral Aah", "Choirs", { "choir", "aah", "sacred" }, "Formant-synthesised choir 'aah' with cathedral reverb.", [] (PatchSpec& s) { archetype (s, A::Choir, 0); });
        L.add ("Soft Ooh", "Choirs", { "choir", "ooh", "soft" }, "Rounded 'ooh' choir.", [] (PatchSpec& s) { archetype (s, A::Choir, 1); });
        L.add ("Synth Vox", "Choirs", { "vocal", "wavetable", "moving" }, "Vowel-morphing synthetic voices.", [] (PatchSpec& s) { archetype (s, A::Choir, 2); });
        L.add ("Breath Choir", "Choirs", { "breathy", "air", "layer" }, "Choir with a breath-noise layer.", [] (PatchSpec& s)
        { archetype (s, A::Choir, 0); sample (s, 2, "Breath Pad", 0.3f, 60, 1); });
        L.add ("Dark Monks", "Choirs", { "low", "dark", "ritual" }, "Low male-choir register with dark filter.", [] (PatchSpec& s)
        { archetype (s, A::Choir, 1); pitch (s, 0, -1); s.p ("flt1_cutoff", 1800.0f); });

        // ---------------- Brass ---------------------------------------------------------------
        L.add ("Brass Section", "Brass", { "brass", "section", "sample" }, "Built-in brass section multisample.", [] (PatchSpec& s) { archetype (s, A::Brass, 0); });
        L.add ("Synth Brass Stab", "Brass", { "stab", "analog", "80s" }, "Bright analog brass stab with filter attack.", [] (PatchSpec& s) { archetype (s, A::Brass, 1); });
        L.add ("Wavetable Horns", "Brass", { "wavetable", "horns", "swell" }, "Brass wavetable with enveloped brightness.", [] (PatchSpec& s) { archetype (s, A::Brass, 2); });
        L.add ("Vangelis Blade", "Brass", { "cs80", "cinematic", "expressive" }, "Expressive poly brass with aftertouch-controlled brightness and vibrato.", [] (PatchSpec& s)
        { archetype (s, A::Brass, 1); env (s, 0, 0.12f, 1.0f, 0.9f, 1.5f); s.addMod (modsrc::Aftertouch, "flt1_cutoff", 0.3f); reverb (s, 0.45f, 0.95f, 6.0f); });
        L.add ("Horn Swell", "Brass", { "soft", "swell", "pad" }, "Soft swelling horn pad.", [] (PatchSpec& s)
        { archetype (s, A::Brass, 0); env (s, 0, 0.6f, 1.0f, 0.9f, 1.0f); });

        // ---------------- Bells ---------------------------------------------------------------
        L.add ("FM Glass Bells", "Bells", { "fm", "bell", "glass" }, "FM ratio wavetable with stretched spectral partials.", [] (PatchSpec& s) { archetype (s, A::Bell, 0); });
        L.add ("Crystal Mallets", "Bells", { "mallet", "modal", "sample" }, "Modal glass mallet multisample.", [] (PatchSpec& s) { archetype (s, A::Bell, 1); });
        L.add ("Singing Bowl", "Bells", { "bowl", "spectral", "meditation" }, "Spectral resynthesis of a singing bowl.", [] (PatchSpec& s) { archetype (s, A::Bell, 2); });
        L.add ("Tubular Night", "Bells", { "tubular", "inharmonic", "dark" }, "Inharmonic stretched bell with low-pass.", [] (PatchSpec& s)
        { archetype (s, A::Bell, 0); s.p ("osc_b_spec_stretch", 0.6f); filter (s, 0, SVF, LP12, 3500.0f, 0.1f); });
        L.add ("Music Box", "Bells", { "music box", "toy", "high" }, "Tiny music-box tines an octave up.", [] (PatchSpec& s)
        { archetype (s, A::Bell, 1); pitch (s, 0, 1); env (s, 0, 0.001f, 1.2f, 0.0f, 0.8f); });

        // ---------------- Textures -----------------------------------------------------------
        L.add ("Granular Fog", "Textures", { "granular", "cloud", "ambient" }, "Granular cloud scanning an original texture.", [] (PatchSpec& s) { archetype (s, A::Texture, 0); });
        L.add ("Breathing Room", "Textures", { "breath", "air", "granular" }, "Breath sample layered with granular breath.", [] (PatchSpec& s) { archetype (s, A::Texture, 1); });
        L.add ("Spectral Tides", "Textures", { "spectral", "scan", "drone" }, "Slow spectral scan of an analog drone.", [] (PatchSpec& s) { archetype (s, A::Texture, 2); });
        L.add ("Lo-Fi Memory", "Textures", { "vinyl", "lofi", "nostalgic" }, "Vinyl texture under a soft wavetable pad.", [] (PatchSpec& s) { archetype (s, A::Texture, 3); });
        L.add ("Neon Rain", "Textures", { "rain", "noise", "granular" }, "Sparse high grains over filtered noise.", [] (PatchSpec& s)
        { archetype (s, A::Texture, 0); pitch (s, 0, 1); s.p ("osc_a_grain_size", 40.0f).p ("osc_a_grain_density", 60.0f); noise (s, 0.15f, 1, 0.3f); });

        // ---------------- Cinematic -----------------------------------------------------------
        L.add ("Deep Space Drone", "Cinematic", { "drone", "dark", "evolving" }, "Low detuned saws and granular drone through a slow SEM filter.", [] (PatchSpec& s) { archetype (s, A::Drone, 0); });
        L.add ("Trailer Impact", "Cinematic", { "impact", "hit", "boom" }, "Sub drop, noise burst and distortion for trailer hits.", [] (PatchSpec& s) { archetype (s, A::Sfx, 2); });
        L.add ("Tension Riser", "Cinematic", { "riser", "build", "noise" }, "Six-second rising sweep.", [] (PatchSpec& s) { archetype (s, A::Sfx, 0); });
        L.add ("Heroic Swell", "Cinematic", { "brass", "strings", "epic" }, "Layered strings and brass swell.", [] (PatchSpec& s)
        { archetype (s, A::Strings, 0); multisample (s, 1, "Brass Section", 0.5f); env (s, 0, 0.8f, 1.0f, 1.0f, 2.0f); reverb (s, 0.4f, 0.9f, 5.0f); });
        L.add ("Vice City Dusk", "Cinematic", { "pad", "nostalgic", "wide" }, "Wide nostalgic pad with tape delay and long hall.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 0); fx (s, FxTapeDelay, 0.25f); reverb (s, 0.5f, 0.95f, 7.0f); });

        // ---------------- Sound Effects ---------------------------------------------------------
        L.add ("Laser Zap", "Sound Effects", { "laser", "zap", "pitch" }, "Fast downward pitch sweep.", [] (PatchSpec& s) { archetype (s, A::Sfx, 1); });
        L.add ("Police Siren", "Sound Effects", { "siren", "lfo", "alarm" }, "Square-wave siren with band-pass filter.", [] (PatchSpec& s) { archetype (s, A::Sfx, 3); });
        L.add ("White Noise Sweep", "Sound Effects", { "noise", "sweep", "transition" }, "Filtered noise sweep for transitions.", [] (PatchSpec& s)
        { s.p ("osc_a_on", 0.0f); noise (s, 1.0f, 1, 0.0f); filter (s, 0, SVF, BP, 300.0f, 0.3f); lfo (s, 0, 2, 0.25f, false, 10, 2); s.addMod (modsrc::Lfo1, "flt1_cutoff", 0.7f); env (s, 0, 0.05f, 0.1f, 1.0f, 1.0f); reverb (s, 0.3f, 0.7f, 3.0f); });
        L.add ("Robot Talk", "Sound Effects", { "robot", "formant", "ring" }, "Ring-modulated formant wavetable.", [] (PatchSpec& s)
        { wavetable (s, 0, "Formant Vowels", 0.0f, 0.8f); lfo (s, 0, 5, 8.0f); s.addMod (modsrc::Lfo1, "osc_a_position", 0.8f, 2); fx (s, FxRingMod, 0.6f, { { "freq", 120.0f } }); filterOff (s, 0); });
        L.add ("Arcade Coin", "Sound Effects", { "chip", "game", "blip" }, "Two-step chip blip.", [] (PatchSpec& s)
        { wavetable (s, 0, "Chip Pulse", 0.0f, 0.7f); pitchEnv (s, 2, 1, -12.0f, 0.08f); s.p ("env3_sustain", 0.0f); env (s, 0, 0.001f, 0.25f, 0.0f, 0.1f); filterOff (s, 0); });

        // ---------------- Sequences -----------------------------------------------------------
        L.add ("Night Drive Sequence", "Sequences", { "sequencer", "pluck", "driving" }, "16-step sequence with analog pluck and synced delay (hold a key).", [] (PatchSpec& s) { archetype (s, A::Sequence, 0); });
        L.add ("Arp Lights", "Sequences", { "arp", "up-down", "digital" }, "Two-octave up-down arpeggio with tine pluck.", [] (PatchSpec& s) { archetype (s, A::Arp, 1); });
        L.add ("Bassline Engine", "Sequences", { "sequencer", "bass", "acid" }, "Acid bass driven by the step sequencer.", [] (PatchSpec& s)
        { archetype (s, A::Bass, 1); s.p ("arp_mode", 2.0f).p ("seq_rate", 4.0f); s.p ("voice_mode", 0.0f).p ("polyphony", 4.0f); });
        L.add ("Kalimba Arp", "Sequences", { "arp", "kalimba", "random" }, "Random-order kalimba arpeggio.", [] (PatchSpec& s)
        { archetype (s, A::Arp, 2); s.p ("arp_pattern", 5.0f); });
        L.add ("Gated Chord Seq", "Sequences", { "chord", "gated", "trance" }, "Gated chord pattern from the sequencer.", [] (PatchSpec& s)
        {
            archetype (s, A::Sequence, 0);
            auto& st = s.seq.steps;
            for (int i = 0; i < 16; ++i) { st[(size_t) i].on = i % 4 != 3; st[(size_t) i].notes = { 0, 3, 7, 12 }; st[(size_t) i].gate = 0.4f; }
        });

        // ---------------- Vintage 1980s -------------------------------------------------------
        L.add ("Eighties Poly Brass", "Vintage 1980s", { "80s", "brass", "jupiter" }, "Big 80s poly brass with JP OTA filter swell.", [] (PatchSpec& s) { archetype (s, A::Brass, 1); unison (s, 0, 3, 0.15f); });
        L.add ("Crystal EP 85", "Vintage 1980s", { "80s", "fm", "ep" }, "Mid-80s digital FM electric piano with chorus.", [] (PatchSpec& s) { archetype (s, A::Keys, 2); fx (s, FxChorus, 0.45f); });
        L.add ("Synthwave Lead", "Vintage 1980s", { "synthwave", "lead", "saw" }, "Saw lead with chorus, delay and gated hall.", [] (PatchSpec& s)
        { archetype (s, A::Lead, 0); fx (s, FxChorus, 0.3f); reverb (s, 0.3f, 0.75f, 2.0f); });
        L.add ("Pop Synth Pad 84", "Vintage 1980s", { "80s", "pad", "pop" }, "Bright detuned pad with dimension chorus typical of 80s pop.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 0); s.p ("flt1_cutoff", 4500.0f).p ("env1_attack", 0.15f); });
        L.add ("Gated Drum Synth", "Vintage 1980s", { "80s", "tom", "synth drum" }, "Pitch-swept analog tom with gated reverb.", [] (PatchSpec& s)
        { analog (s, 0, 0.0f, 0.9f); pitchEnv (s, 2, 1, 12.0f, 0.25f); noise (s, 0.2f, 1, 0.0f); env (s, 0, 0.001f, 0.4f, 0.0f, 0.3f); filterOff (s, 0); reverb (s, 0.4f, 0.4f, 0.6f); });

        // ---------------- Vintage 1990s -------------------------------------------------------
        L.add ("Nineties Digital Pad", "Vintage 1990s", { "90s", "digital", "jd" }, "Bright early-90s digital pad: harmonic wavetable, glass layer, chorus and big reverb.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 1); fx (s, FxChorus, 0.45f); reverb (s, 0.45f, 0.9f, 5.0f); });
        L.add ("Rave Hoover", "Vintage 1990s", { "hoover", "rave", "90s" }, "Detuned PWM 'hoover' with pitch scoop and chorus.", [] (PatchSpec& s)
        { analog (s, 0, 1.0f, 0.7f, 0.3f); unison (s, 0, 6, 0.45f, 0.9f); analog (s, 1, 0.67f, 0.5f); pitch (s, 1, -1); unison (s, 1, 4, 0.4f, 0.9f); pitchEnv (s, 2, 3, -5.0f, 0.25f); filter (s, 0, JN, LP24, 4000.0f, 0.1f); voice (s, 2, 0.12f, 1); fx (s, FxChorus, 0.5f); });
        L.add ("House Organ Bass", "Vintage 1990s", { "house", "organ", "90s" }, "Classic house organ stab with short decay.", [] (PatchSpec& s) { archetype (s, A::Organ, 1); });
        L.add ("Trance Supersaw", "Vintage 1990s", { "trance", "supersaw", "anthem" }, "Seven-voice detuned saw stack with stereo delay.", [] (PatchSpec& s)
        { wavetable (s, 0, "Basic Shapes", 0.66f, 0.7f); unison (s, 0, 7, 0.35f, 1.0f, 0.8f); wavetable (s, 1, "Basic Shapes", 0.66f, 0.4f); pitch (s, 1, 1); unison (s, 1, 5, 0.3f, 1.0f); filter (s, 0, SVF, LP12, 9000.0f, 0.1f); env (s, 0, 0.005f, 0.5f, 0.8f, 0.4f); fx (s, FxStereoDelay, 0.25f); reverb (s, 0.3f, 0.8f, 3.0f); });
        L.add ("Pop Bell Pad 92", "Vintage 1990s", { "90s", "bell", "pad" }, "Bell attack over a digital pad, a classic early-90s layer.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 1); wavetable (s, 2, "FM Ratios", 0.7f, 0.35f); env (s, 0, 0.01f, 2.0f, 0.7f, 1.8f); });

        return L.v;
    }
}

const std::vector<PatchSpec>& all()
{
    static const std::vector<PatchSpec> presets = build();
    return presets;
}

} // namespace vsx::FactoryPresets
