#include "FactoryExpansion.h"
#include "SoundRecipes.h"
#include "../Synth/FmVoice.h"
#include <functional>
#include <set>

namespace vsx::FactoryExpansion
{
using namespace Recipes;
using pid::FxChorus; using pid::FxDimension; using pid::FxPhaser; using pid::FxFlanger; using pid::FxStereoDelay; using pid::FxTapeDelay;
using pid::FxDigitalDelay; using pid::FxReverb; using pid::FxDistortion; using pid::FxTube; using pid::FxTape; using pid::FxStereo;

namespace
{
    struct Part
    {
        const char* label;                         // used in the description
        std::vector<const char*> tags;
        std::function<void (PatchSpec&)> apply;
        int flags = 0;                             // kSlowSource on voicings, kShortEnvelope on treatments
    };
    enum { kSlowSource = 1, kShortEnvelope = 2 };   // a slow-speaking source is never given a short envelope

    struct Family
    {
        const char* category;
        int count;
        std::vector<Part> voicings, treatments;
        std::vector<const char*> adjectives, nouns;
    };

    // reverb with program: 0 hall, 1 plate, 2 room, 3 chamber, 4 ambient, 5 nonlinear, 6 gated, 7 reverse
    void space (PatchSpec& s, float mix, float size, float decay, int program)
    {
        fx (s, FxReverb, mix, { { "size", size }, { "decay", decay } });
        s.set ("fx_reverb_program", (float) program);
    }
    void ampEnv (PatchSpec& s, float a, float d, float sus, float r) { env (s, 0, a, d, sus, r); }
    void fltEnv (PatchSpec& s, float a, float d, float sus, float r) { env (s, 1, a, d, sus, r); }
    void saw (PatchSpec& s, int o, float level = 0.7f) { analog (s, o, 0.67f, level); }
    void sqr (PatchSpec& s, int o, float level = 0.7f, float pw = 0.5f) { analog (s, o, 1.0f, level, pw); }
    void tri (PatchSpec& s, int o, float level = 0.7f) { analog (s, o, 0.33f, level); }
    void sine (PatchSpec& s, int o, float level = 0.7f) { analog (s, o, 0.0f, level); }

    // =================================================================================
    std::vector<Family> families()
    {
        std::vector<Family> f;

        // ---------------- vintage analog -----------------------------------------------------
        f.push_back ({ "Analog", 50,
            {
                { "two detuned saws", { "saw", "detuned" }, [] (PatchSpec& s) { saw (s, 0); saw (s, 1, 0.6f); pitch (s, 1, 0, 0, 7.0f); } },
                { "saw with a square an octave down", { "saw", "square" }, [] (PatchSpec& s) { saw (s, 0); sqr (s, 1, 0.55f); pitch (s, 1, -1); } },
                { "a single pulse with slow PWM", { "pwm", "pulse" }, [] (PatchSpec& s) { sqr (s, 0, 0.75f, 0.35f); lfo (s, 2, 1, 0.3f); s.addMod (modsrc::Lfo3, "osc_a_pulse_width", 0.25f); } },
                { "triangle over a square sub", { "triangle", "sub" }, [] (PatchSpec& s) { tri (s, 0, 0.8f); sub (s, 0.55f, -1, 1); } },
                { "hard-synced saw swept by an envelope", { "sync", "sweep" }, [] (PatchSpec& s) { saw (s, 0, 0.6f); saw (s, 1, 0.7f); xmod (s, 1, 5, 0, 1.0f); pitch (s, 1, 0, 7); env (s, 2, 0.0f, 0.8f, 0.2f, 0.4f); s.addMod (modsrc::Env3, "osc_b_semi", 0.35f); } },
                { "three stacked saws", { "saw", "stack" }, [] (PatchSpec& s) { saw (s, 0, 0.55f); saw (s, 1, 0.5f); pitch (s, 1, 0, 0, -9.0f); saw (s, 2, 0.45f); pitch (s, 2, -1, 0, 4.0f); } },
                { "square with breath noise", { "square", "breathy" }, [] (PatchSpec& s) { sqr (s, 0, 0.7f); noise (s, 0.12f, 1, -0.3f); } },
                { "five-voice unison saw", { "unison", "wide" }, [] (PatchSpec& s) { saw (s, 0, 0.75f); unison (s, 0, 5, 0.22f, 0.85f); } },
                { "two narrow pulses", { "pulse", "nasal" }, [] (PatchSpec& s) { sqr (s, 0, 0.6f, 0.2f); sqr (s, 1, 0.55f, 0.3f); pitch (s, 1, 1, 0, 3.0f); } },
                { "cross-modulated saw pair", { "fm", "metallic" }, [] (PatchSpec& s) { saw (s, 0, 0.7f); sine (s, 1, 0.4f); xmod (s, 0, 1, 1, 0.18f); } },
            },
            {
                { "a punchy transistor ladder with a snappy filter envelope", { "ladder", "punchy" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 900.0f, 0.35f, 0.3f, 0.35f); fltEnv (s, 0.002f, 0.45f, 0.25f, 0.3f); ampEnv (s, 0.003f, 0.6f, 0.75f, 0.35f); } },
                { "a 4-pole OTA filter opened slowly", { "ota", "sweep" }, [] (PatchSpec& s) { filter (s, 0, JP, LP24, 700.0f, 0.3f, 0.2f, 0.3f); fltEnv (s, 0.9f, 1.2f, 0.6f, 1.0f); ampEnv (s, 0.25f, 1.0f, 0.85f, 0.9f); } },
                { "a warm OTA low-pass with ensemble chorus", { "chorus", "warm" }, [] (PatchSpec& s) { filter (s, 0, JN, LP24, 2400.0f, 0.15f, 0.15f, 0.15f, 0.4f, 0.33f); fltEnv (s, 0.01f, 1.0f, 0.5f, 0.6f); ampEnv (s, 0.02f, 1.0f, 0.8f, 0.7f); fx (s, FxChorus, 0.45f); } },
                { "a resonant cascade filter for poly stabs", { "cascade", "stab" }, [] (PatchSpec& s) { filter (s, 0, P5, LP24, 1100.0f, 0.45f, 0.25f, 0.4f); fltEnv (s, 0.001f, 0.3f, 0.15f, 0.25f); ampEnv (s, 0.002f, 0.4f, 0.4f, 0.3f); space (s, 0.18f, 0.5f, 1.4f, 2); } },
                { "a state-variable filter with gentle movement", { "state-variable", "soft" }, [] (PatchSpec& s) { filter (s, 0, SEM, LP12, 1800.0f, 0.25f, 0.1f, 0.2f); filterLfo (s, 1, 0, 0.18f, 0.15f); ampEnv (s, 0.05f, 0.9f, 0.85f, 0.8f); space (s, 0.2f, 0.6f, 2.2f, 3); } },
                { "a screaming Sallen-Key low-pass", { "sallen-key", "aggressive" }, [] (PatchSpec& s) { filter (s, 0, MS, LP12, 1500.0f, 0.6f, 0.35f, 0.3f); fltEnv (s, 0.005f, 0.5f, 0.4f, 0.3f); ampEnv (s, 0.004f, 0.5f, 0.8f, 0.3f); } },
                { "a squelchy diode ladder", { "diode", "squelch" }, [] (PatchSpec& s) { filter (s, 0, DIODE, LP24, 600.0f, 0.55f, 0.3f, 0.45f); fltEnv (s, 0.001f, 0.25f, 0.1f, 0.2f); ampEnv (s, 0.002f, 0.3f, 0.7f, 0.2f); voice (s, 1, 0.06f, 1); } },
                { "a clean filter with tape echo", { "clean", "echo" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP24, 3200.0f, 0.1f, 0.05f, 0.1f); ampEnv (s, 0.01f, 0.7f, 0.7f, 0.5f); fx (s, FxTapeDelay, 0.22f, { { "feedback", 0.35f } }); } },
            },
            { "Velvet", "Copper", "Amber", "Solar", "Midnight", "Analog", "Warm", "Silver", "Rust", "Golden", "Cobalt", "Dusty", "Crimson", "Bronze", "Faded", "Electric", "Royal", "Smoky" },
            { "Poly", "Saw", "Pulse", "Sweep", "Stab", "Voltage", "Circuit", "Drift", "Ladder", "Drone", "Brass", "Keys", "Wave", "Signal", "Glow", "Motion", "Engine", "Current" } });

        // ---------------- 1980s ---------------------------------------------------------------
        f.push_back ({ "Vintage 1980s", 35,
            {
                { "the Glass wavetable", { "digital", "glass" }, [] (PatchSpec& s) { wavetable (s, 0, "Glass", 0.3f, 0.75f); } },
                { "the Brass wavetable", { "brass", "digital" }, [] (PatchSpec& s) { wavetable (s, 0, "Brass", 0.4f, 0.75f); unison (s, 0, 2, 0.08f, 0.6f); } },
                { "chorused saw pair", { "saw", "poly" }, [] (PatchSpec& s) { saw (s, 0); saw (s, 1, 0.55f); pitch (s, 1, 0, 0, 6.0f); } },
                { "PWM string ensemble", { "pwm", "strings" }, [] (PatchSpec& s) { sqr (s, 0, 0.7f, 0.4f); lfo (s, 2, 1, 0.45f); s.addMod (modsrc::Lfo3, "osc_a_pulse_width", 0.3f); unison (s, 0, 3, 0.1f, 0.8f); } },
                { "the Electric Tine wavetable", { "tine", "ep" }, [] (PatchSpec& s) { wavetable (s, 0, "Electric Tine", 0.2f, 0.8f); } },
                { "hollow square", { "square", "hollow" }, [] (PatchSpec& s) { wavetable (s, 0, "Hollow Square", 0.5f, 0.75f); } },
                { "the Digital FM wavetable", { "fm", "digital" }, [] (PatchSpec& s) { wavetable (s, 0, "Digital FM", 0.35f, 0.75f); env (s, 2, 0.0f, 0.6f, 0.2f, 0.4f); s.addMod (modsrc::Env3, "osc_a_position", 0.4f); } },
                { "stepped digital waves", { "stepped", "lo-fi" }, [] (PatchSpec& s) { wavetable (s, 0, "Stepped Digital", 0.25f, 0.7f); } },
            },
            {
                { "bright chorus and a plate", { "chorus", "plate" }, [] (PatchSpec& s) { filter (s, 0, JP, LP24, 5200.0f, 0.15f, 0.1f, 0.15f); ampEnv (s, 0.005f, 0.9f, 0.7f, 0.6f); fx (s, FxChorus, 0.45f); space (s, 0.25f, 0.55f, 2.0f, 1); } },
                { "a gated reverb", { "gated", "punchy" }, [] (PatchSpec& s) { filter (s, 0, P5, LP24, 3500.0f, 0.2f, 0.15f, 0.25f); fltEnv (s, 0.001f, 0.35f, 0.3f, 0.3f); ampEnv (s, 0.002f, 0.4f, 0.5f, 0.3f); space (s, 0.3f, 0.6f, 3.0f, 6); } },
                { "a slow attack and a large hall", { "pad", "slow" }, [] (PatchSpec& s) { filter (s, 0, JN, LP24, 2600.0f, 0.15f, 0.1f, 0.1f, 0.4f, 0.33f); ampEnv (s, 0.8f, 1.5f, 0.85f, 1.8f); fx (s, FxChorus, 0.35f); space (s, 0.35f, 0.85f, 4.5f, 0); } },
                { "a plucked envelope with stereo echoes", { "pluck", "delay" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 1800.0f, 0.25f, 0.2f, 0.4f); fltEnv (s, 0.001f, 0.3f, 0.0f, 0.3f); ampEnv (s, 0.001f, 0.5f, 0.0f, 0.4f); fx (s, FxStereoDelay, 0.3f, { { "feedback", 0.4f } }); } },
                { "dimension chorus on a keyboard envelope", { "dimension", "keys" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP12, 4500.0f, 0.1f, 0.05f, 0.1f); ampEnv (s, 0.003f, 1.4f, 0.35f, 0.6f); fx (s, FxDimension, 0.5f); space (s, 0.18f, 0.5f, 1.6f, 2); } },
                { "a lead with vibrato and a digital delay", { "lead", "vibrato" }, [] (PatchSpec& s) { filter (s, 0, MS, LP12, 2800.0f, 0.3f, 0.2f, 0.2f); ampEnv (s, 0.01f, 0.4f, 0.9f, 0.3f); vibrato (s, 1, 5.5f, 0.2f); voice (s, 2, 0.05f, 1); fx (s, FxDigitalDelay, 0.25f); } },
            },
            { "Neon", "Laser", "Chrome", "Pastel", "Synthetic", "Mirror", "Arcade", "Video", "Night", "Ocean", "Coastal", "Sunset", "Vapor", "Retro" },
            { "Drive", "Skyline", "Avenue", "Horizon", "Fantasy", "Dream", "Motion", "Club", "Signal", "Heat", "Cruise", "Palms", "Glow", "Highway" } });

        // ---------------- 1990s ---------------------------------------------------------------
        f.push_back ({ "Vintage 1990s", 35,
            {
                { "a seven-voice unison saw stack", { "supersaw", "trance" }, [] (PatchSpec& s) { wavetable (s, 0, "Saw Stack", 0.5f, 0.7f); unison (s, 0, 7, 0.35f, 0.95f); } },
                { "formant vowels", { "vowel", "vocal" }, [] (PatchSpec& s) { wavetable (s, 0, "Formant Vowels", 0.3f, 0.75f); lfo (s, 2, 1, 0.25f); s.addMod (modsrc::Lfo3, "osc_a_position", 0.3f); } },
                { "a bit-crushed saw", { "lo-fi", "crushed" }, [] (PatchSpec& s) { wavetable (s, 0, "Bitcrush Saw", 0.45f, 0.7f); } },
                { "a wavefolded sine", { "wavefold", "harsh" }, [] (PatchSpec& s) { wavetable (s, 0, "Wavefold", 0.35f, 0.7f); env (s, 2, 0.0f, 0.5f, 0.3f, 0.4f); s.addMod (modsrc::Env3, "osc_a_position", 0.35f); } },
                { "the Growl wavetable", { "growl", "rave" }, [] (PatchSpec& s) { wavetable (s, 0, "Growl", 0.4f, 0.75f); } },
                { "a harmonic sweep", { "harmonic", "sweep" }, [] (PatchSpec& s) { wavetable (s, 0, "Harmonic Sweep", 0.2f, 0.75f); lfo (s, 2, 0, 0.12f); s.addMod (modsrc::Lfo3, "osc_a_position", 0.45f); } },
                { "detuned saw and square layers", { "layer", "rave" }, [] (PatchSpec& s) { saw (s, 0); unison (s, 0, 3, 0.25f, 0.8f); sqr (s, 1, 0.5f); pitch (s, 1, 1); } },
                { "a drawbar organ wavetable", { "organ", "house" }, [] (PatchSpec& s) { wavetable (s, 0, "Drawbar Organ", 0.5f, 0.8f); } },
            },
            {
                { "a tempo-synced trance gate", { "gate", "trance" }, [] (PatchSpec& s) { filter (s, 0, JP, LP24, 3200.0f, 0.2f, 0.15f, 0.1f); ampEnv (s, 0.005f, 0.4f, 1.0f, 0.4f); lfo (s, 3, 4, 1.0f, true, 7); s.addMod (modsrc::Lfo4, "osc_a_level", -0.6f); space (s, 0.25f, 0.7f, 2.5f, 1); } },
                { "a rave stab envelope", { "stab", "rave" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 2200.0f, 0.3f, 0.3f, 0.35f); fltEnv (s, 0.001f, 0.2f, 0.1f, 0.2f); ampEnv (s, 0.001f, 0.3f, 0.2f, 0.3f); space (s, 0.2f, 0.5f, 1.5f, 2); } },
                { "an acid diode filter", { "acid", "resonant" }, [] (PatchSpec& s) { filter (s, 0, ACID, LP24, 700.0f, 0.65f, 0.35f, 0.5f); fltEnv (s, 0.001f, 0.25f, 0.05f, 0.2f); ampEnv (s, 0.002f, 0.3f, 0.6f, 0.15f); voice (s, 1, 0.08f, 1); } },
                { "a deep pad with digital delay", { "pad", "deep" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP24, 1500.0f, 0.2f, 0.1f, 0.15f); ampEnv (s, 0.6f, 1.5f, 0.85f, 1.6f); fx (s, FxDigitalDelay, 0.25f, { { "feedback", 0.45f } }); space (s, 0.3f, 0.8f, 4.0f, 4); } },
                { "a distorted lead", { "distortion", "lead" }, [] (PatchSpec& s) { filter (s, 0, MS, LP12, 3000.0f, 0.35f, 0.35f, 0.2f); ampEnv (s, 0.005f, 0.4f, 0.9f, 0.25f); fx (s, FxDistortion, 0.3f, { { "drive", 9.0f } }); voice (s, 2, 0.04f, 1); } },
                { "a phaser-swept keyboard", { "phaser", "keys" }, [] (PatchSpec& s) { filter (s, 0, JN, LP24, 3800.0f, 0.15f, 0.1f, 0.1f, 0.4f, 0.33f); ampEnv (s, 0.003f, 1.1f, 0.4f, 0.5f); fx (s, FxPhaser, 0.35f); } },
            },
            { "Hyper", "Euro", "Acid", "Rave", "Cyber", "Techno", "Digital", "Warehouse", "Strobe", "Ultra", "Hard", "Future", "Dance", "Floor" },
            { "Anthem", "Hoover", "Stab", "Pulse", "Gate", "Rush", "Rush Hour", "Signal", "Engine", "Vortex", "Matrix", "Sequence", "Riser", "Theory" } });

        // ---------------- bass -------------------------------------------------------------------
        f.push_back ({ "Bass", 45,
            {
                { "saw over a sine sub", { "saw", "sub" }, [] (PatchSpec& s) { saw (s, 0, 0.7f); sub (s, 0.55f, -1, 0); } },
                { "square over a square sub", { "square", "sub" }, [] (PatchSpec& s) { sqr (s, 0, 0.7f); sub (s, 0.5f, -1, 1); } },
                { "a pure sine sub with a click", { "sub", "sine" }, [] (PatchSpec& s) { sine (s, 0, 0.85f); noise (s, 0.05f, 0); env (s, 2, 0.0f, 0.02f, 0.0f, 0.02f); s.addMod (modsrc::Env3, "noise_level", 0.6f); } },
                { "a cross-modulated FM bass", { "fm", "punchy" }, [] (PatchSpec& s) { sine (s, 0, 0.75f); sine (s, 1, 0.5f); pitch (s, 1, 1); xmod (s, 0, 2, 1, 0.35f); env (s, 2, 0.0f, 0.25f, 0.0f, 0.2f); s.addMod (modsrc::Env3, "osc_a_xmod_amount", 0.6f); } },
                { "the Bass Weight wavetable", { "weighty", "digital" }, [] (PatchSpec& s) { wavetable (s, 0, "Bass Weight", 0.3f, 0.8f); } },
                { "the Growl wavetable", { "growl", "aggressive" }, [] (PatchSpec& s) { wavetable (s, 0, "Growl", 0.5f, 0.75f); sub (s, 0.4f, -1, 0); } },
                { "detuned saws (reese)", { "reese", "detuned" }, [] (PatchSpec& s) { saw (s, 0, 0.6f); saw (s, 1, 0.6f); pitch (s, 1, 0, 0, 18.0f); } },
                { "the Warm Bass sample", { "sampled", "round" }, [] (PatchSpec& s) { multisample (s, 0, "Warm Bass", 0.8f); } },
                { "a narrow pulse", { "pulse", "nasal" }, [] (PatchSpec& s) { sqr (s, 0, 0.75f, 0.18f); sub (s, 0.35f, -1, 0); } },
            },
            {
                { "a punchy ladder envelope", { "ladder", "punch" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 420.0f, 0.3f, 0.35f, 0.45f, 0.4f); fltEnv (s, 0.001f, 0.28f, 0.15f, 0.2f); ampEnv (s, 0.002f, 0.5f, 0.85f, 0.15f); voice (s, 1, 0.0f, 1); } },
                { "an acid diode squelch", { "acid", "squelch" }, [] (PatchSpec& s) { filter (s, 0, DIODE, LP24, 380.0f, 0.6f, 0.3f, 0.55f, 0.3f); fltEnv (s, 0.001f, 0.2f, 0.05f, 0.15f); ampEnv (s, 0.002f, 0.3f, 0.75f, 0.12f); voice (s, 1, 0.07f, 1); } },
                { "a round state-variable low-pass", { "round", "smooth" }, [] (PatchSpec& s) { filter (s, 0, SEM, LP12, 650.0f, 0.2f, 0.15f, 0.25f, 0.4f); fltEnv (s, 0.003f, 0.4f, 0.4f, 0.2f); ampEnv (s, 0.004f, 0.6f, 0.9f, 0.2f); voice (s, 1, 0.0f, 1); } },
                { "a gritty Sallen-Key drive", { "gritty", "driven" }, [] (PatchSpec& s) { filter (s, 0, MS, LP12, 800.0f, 0.4f, 0.4f, 0.35f, 0.4f); fltEnv (s, 0.001f, 0.3f, 0.2f, 0.2f); ampEnv (s, 0.002f, 0.4f, 0.85f, 0.15f); fx (s, FxTube, 0.35f); voice (s, 1, 0.0f, 1); } },
                { "a deep 12 dB sub filter", { "deep", "sub" }, [] (PatchSpec& s) { filter (s, 0, JN, LP12, 300.0f, 0.1f, 0.1f, 0.15f, 0.3f, 0.33f); ampEnv (s, 0.004f, 0.7f, 0.95f, 0.2f); voice (s, 1, 0.0f, 1); } },
                { "a tempo-synced wobble", { "wobble", "dubstep" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 500.0f, 0.4f, 0.3f, 0.0f); filterLfo (s, 1, 0, 1.0f, 0.5f, true, 9); ampEnv (s, 0.003f, 0.5f, 1.0f, 0.15f); voice (s, 1, 0.0f, 1); } },
            },
            { "Deep", "Low", "Heavy", "Rubber", "Concrete", "Thick", "Dark", "Round", "Sub", "Iron", "Velvet", "Tight", "Fat", "Grim", "Liquid" },
            { "Floor", "Engine", "Foundation", "Bounce", "Thump", "Groove", "Pressure", "Tremor", "Pulse", "Rumble", "Weight", "Spine", "Quake", "Root", "Basement" } });

        // ---------------- pads -------------------------------------------------------------------
        f.push_back ({ "Pads", 40,
            {
                { "a wide unison saw", { "unison", "wide" }, [] (PatchSpec& s) { saw (s, 0, 0.65f); unison (s, 0, 6, 0.2f, 0.95f); } },
                { "the Soft Pad wavetable", { "soft", "digital" }, [] (PatchSpec& s) { wavetable (s, 0, "Soft Pad", 0.4f, 0.75f); unison (s, 0, 3, 0.12f, 0.8f); } },
                { "the Strings Ensemble wavetable", { "strings", "ensemble" }, [] (PatchSpec& s) { wavetable (s, 0, "Strings Ensemble", 0.5f, 0.75f); unison (s, 0, 3, 0.1f, 0.8f); } },
                { "the Vocal Choir wavetable", { "choir", "vocal" }, [] (PatchSpec& s) { wavetable (s, 0, "Vocal Choir", 0.4f, 0.75f); } },
                { "the Breath Pad sample", { "breath", "airy" }, [] (PatchSpec& s) { sample (s, 0, "Breath Pad", 0.75f); } },
                { "a granular texture", { "granular", "evolving" }, [] (PatchSpec& s) { granular (s, 0, "Granular Texture", 0.3f, 120.0f, 25.0f, 0.3f, 0.7f); } },
                { "spectral resynthesis of the Soft Pad table", { "spectral", "additive" }, [] (PatchSpec& s) { spectral (s, 0, "Soft Pad", false, 0.7f); } },
                { "a PWM pair", { "pwm", "analog" }, [] (PatchSpec& s) { sqr (s, 0, 0.6f, 0.45f); sqr (s, 1, 0.55f, 0.35f); pitch (s, 1, 0, 0, 5.0f); lfo (s, 2, 1, 0.25f); s.addMod (modsrc::Lfo3, "osc_a_pulse_width", 0.25f); } },
                { "the Glass wavetable", { "glass", "bright" }, [] (PatchSpec& s) { wavetable (s, 0, "Glass", 0.6f, 0.7f); unison (s, 0, 2, 0.1f, 0.7f); } },
            },
            {
                { "a slow swell into a hall", { "slow", "hall" }, [] (PatchSpec& s) { filter (s, 0, JN, LP24, 2200.0f, 0.15f, 0.1f, 0.1f, 0.4f, 0.33f); ampEnv (s, 1.2f, 2.0f, 0.85f, 2.5f); space (s, 0.38f, 0.85f, 5.0f, 0); } },
                { "an evolving filter LFO", { "evolving", "motion" }, [] (PatchSpec& s) { filter (s, 0, SEM, LP12, 1600.0f, 0.3f, 0.1f, 0.0f); filterLfo (s, 1, 0, 0.08f, 0.3f); ampEnv (s, 0.7f, 1.5f, 0.9f, 2.0f); space (s, 0.3f, 0.8f, 4.0f, 3); } },
                { "an ambient wash", { "ambient", "wash" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP24, 3000.0f, 0.1f, 0.05f, 0.0f); ampEnv (s, 1.5f, 2.0f, 0.9f, 3.5f); space (s, 0.5f, 0.95f, 9.0f, 4); } },
                { "dimension chorus", { "dimension", "lush" }, [] (PatchSpec& s) { filter (s, 0, JP, LP24, 2800.0f, 0.2f, 0.1f, 0.1f); ampEnv (s, 0.5f, 1.2f, 0.85f, 1.5f); fx (s, FxDimension, 0.55f); space (s, 0.3f, 0.75f, 3.5f, 1); } },
                { "a dark low-pass", { "dark", "warm" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 900.0f, 0.2f, 0.15f, 0.1f); ampEnv (s, 0.9f, 1.5f, 0.9f, 2.0f); space (s, 0.32f, 0.8f, 4.5f, 3); } },
                { "slow pitch drift and tape echo", { "drift", "tape" }, [] (PatchSpec& s) { filter (s, 0, P5, LP24, 2000.0f, 0.15f, 0.1f, 0.1f); ampEnv (s, 0.8f, 1.5f, 0.85f, 2.0f); s.set ("osc_drift", 0.45f); fx (s, FxTapeDelay, 0.2f, { { "feedback", 0.35f } }); space (s, 0.3f, 0.8f, 4.0f, 0); } },
            },
            { "Silent", "Endless", "Frozen", "Golden", "Distant", "Hollow", "Floating", "Velvet", "Aurora", "Misty", "Warm", "Lunar", "Polar", "Gentle", "Opal" },
            { "Horizon", "Canopy", "Cathedral", "Tide", "Haze", "Bloom", "Glacier", "Dawn", "Garden", "Cloud", "Reverie", "Lagoon", "Vapour", "Sanctuary", "Echo" } });

        // ---------------- strings ----------------------------------------------------------------
        f.push_back ({ "Strings", 30,
            {
                { "the Ensemble Strings multisample", { "ensemble", "sampled" }, [] (PatchSpec& s) { multisample (s, 0, "Ensemble Strings", 0.8f); }, kSlowSource },
                { "the Strings Ensemble wavetable", { "wavetable", "ensemble" }, [] (PatchSpec& s) { wavetable (s, 0, "Strings Ensemble", 0.45f, 0.75f); unison (s, 0, 4, 0.12f, 0.85f); } },
                { "a bowed unison saw", { "saw", "bowed" }, [] (PatchSpec& s) { saw (s, 0, 0.65f); unison (s, 0, 4, 0.14f, 0.85f); } },
                { "PWM strings", { "pwm", "vintage" }, [] (PatchSpec& s) { sqr (s, 0, 0.65f, 0.4f); lfo (s, 2, 1, 0.5f); s.addMod (modsrc::Lfo3, "osc_a_pulse_width", 0.3f); unison (s, 0, 3, 0.1f, 0.85f); } },
                { "the Plucked String sample", { "plucked", "sampled" }, [] (PatchSpec& s) { sample (s, 0, "Plucked String", 0.8f, 60, 1); } },
                { "spectral strings", { "spectral", "airy" }, [] (PatchSpec& s) { spectral (s, 0, "Strings Ensemble", false, 0.7f); } },
            },
            {
                { "a short staccato envelope", { "staccato", "short" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP12, 4500.0f, 0.1f, 0.05f, 0.1f); ampEnv (s, 0.004f, 0.35f, 0.0f, 0.15f); space (s, 0.22f, 0.6f, 2.0f, 3); }, kShortEnvelope },
                { "a legato swell", { "legato", "swell" }, [] (PatchSpec& s) { filter (s, 0, JN, LP24, 3000.0f, 0.1f, 0.05f, 0.15f, 0.4f, 0.33f); ampEnv (s, 0.6f, 1.0f, 0.9f, 1.2f); voice (s, 2, 0.08f, 1); space (s, 0.3f, 0.8f, 3.5f, 0); } },
                { "amplitude tremolo", { "tremolo", "motion" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP24, 3800.0f, 0.1f, 0.05f, 0.0f); ampEnv (s, 0.2f, 1.0f, 0.9f, 0.8f); lfo (s, 3, 0, 6.0f); s.addMod (modsrc::Lfo4, "osc_a_level", -0.25f); space (s, 0.25f, 0.7f, 2.5f, 3); } },
                { "a pizzicato pluck", { "pizzicato", "pluck" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 2500.0f, 0.1f, 0.1f, 0.3f); fltEnv (s, 0.001f, 0.2f, 0.0f, 0.2f); ampEnv (s, 0.001f, 0.35f, 0.0f, 0.3f); space (s, 0.25f, 0.6f, 2.0f, 2); }, kShortEnvelope },
                { "a cinematic hall swell", { "cinematic", "hall" }, [] (PatchSpec& s) { filter (s, 0, SEM, LP12, 2400.0f, 0.15f, 0.05f, 0.2f); env (s, 1, 1.5f, 1.0f, 0.8f, 1.5f); ampEnv (s, 1.2f, 1.5f, 0.9f, 2.5f); space (s, 0.4f, 0.9f, 6.0f, 0); } },
            },
            { "Silken", "Grand", "Bowed", "Solemn", "Bright", "Tender", "Distant", "Royal", "Velvet", "Noble", "Somber", "Lyrical" },
            { "Section", "Quartet", "Ensemble", "Bows", "Overture", "Chamber", "Strings", "Theme", "Adagio", "Elegy", "Sonata", "Motif" } });

        // ---------------- bells ------------------------------------------------------------------
        f.push_back ({ "Bells", 30,
            {
                { "the Glass wavetable", { "glass", "bright" }, [] (PatchSpec& s) { wavetable (s, 0, "Glass", 0.75f, 0.75f); } },
                { "the Metal Hit sample", { "metal", "struck" }, [] (PatchSpec& s) { sample (s, 0, "Metal Hit", 0.8f, 60, 0); } },
                { "the Glass Bowl sample", { "bowl", "singing" }, [] (PatchSpec& s) { sample (s, 0, "Glass Bowl", 0.8f, 60, 1); } },
                { "the FM Ratios wavetable", { "fm", "inharmonic" }, [] (PatchSpec& s) { wavetable (s, 0, "FM Ratios", 0.55f, 0.75f); } },
                { "the Glass Mallet multisample", { "mallet", "sampled" }, [] (PatchSpec& s) { multisample (s, 0, "Glass Mallet", 0.8f); } },
                { "ring-modulated sines", { "ring", "metallic" }, [] (PatchSpec& s) { sine (s, 0, 0.7f); sine (s, 1, 0.5f); pitch (s, 1, 1, 7, 13.0f); xmod (s, 0, 3, 1, 0.6f); } },
            },
            {
                { "a long natural decay in a hall", { "long", "hall" }, [] (PatchSpec& s) { filterOff (s, 0); ampEnv (s, 0.001f, 3.5f, 0.0f, 3.0f); space (s, 0.32f, 0.8f, 4.5f, 0); } },
                { "a short chime with echoes", { "chime", "echo" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP12, 9000.0f, 0.1f, 0.05f, 0.0f); ampEnv (s, 0.001f, 1.2f, 0.0f, 1.0f); fx (s, FxStereoDelay, 0.28f, { { "feedback", 0.4f } }); space (s, 0.2f, 0.6f, 2.5f, 1); } },
                { "a dark struck tone", { "dark", "struck" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 2200.0f, 0.15f, 0.1f, 0.25f); fltEnv (s, 0.001f, 1.0f, 0.0f, 0.8f); ampEnv (s, 0.001f, 2.5f, 0.0f, 2.0f); space (s, 0.25f, 0.7f, 3.0f, 3); } },
                { "a shimmering chorus", { "shimmer", "chorus" }, [] (PatchSpec& s) { filterOff (s, 0); ampEnv (s, 0.001f, 2.5f, 0.15f, 2.0f); fx (s, FxChorus, 0.4f); space (s, 0.35f, 0.85f, 6.0f, 4); } },
                { "a velocity-bright mallet", { "velocity", "mallet" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP24, 2800.0f, 0.1f, 0.05f, 0.2f); velocityToCutoff (s, 0, 0.5f); ampEnv (s, 0.001f, 1.6f, 0.0f, 1.2f); space (s, 0.2f, 0.55f, 1.8f, 2); } },
            },
            { "Crystal", "Silver", "Temple", "Frozen", "Golden", "Distant", "Tiny", "Winter", "Moon", "Shining", "Ancient", "Glass" },
            { "Chime", "Bell", "Carillon", "Gong", "Bowl", "Tines", "Spire", "Peal", "Toll", "Glimmer", "Lantern", "Prism" } });

        // ---------------- leads ------------------------------------------------------------------
        f.push_back ({ "Leads", 40,
            {
                { "a hard-synced saw", { "sync", "bright" }, [] (PatchSpec& s) { saw (s, 0, 0.55f); saw (s, 1, 0.7f); xmod (s, 1, 5, 0, 1.0f); pitch (s, 1, 0, 12); } },
                { "a square", { "square", "classic" }, [] (PatchSpec& s) { sqr (s, 0, 0.75f); } },
                { "a three-voice unison saw", { "saw", "unison" }, [] (PatchSpec& s) { saw (s, 0, 0.7f); unison (s, 0, 3, 0.12f, 0.6f); } },
                { "a whistling FM pair", { "fm", "whistle" }, [] (PatchSpec& s) { sine (s, 0, 0.75f); sine (s, 1, 0.4f); pitch (s, 1, 1); xmod (s, 0, 2, 1, 0.22f); } },
                { "a wavefolded sine", { "wavefold", "edgy" }, [] (PatchSpec& s) { wavetable (s, 0, "Wavefold", 0.45f, 0.7f); } },
                { "formant vowels", { "vowel", "talking" }, [] (PatchSpec& s) { wavetable (s, 0, "Formant Vowels", 0.5f, 0.75f); s.addMod (modsrc::ModWheel, "osc_a_position", 0.4f); } },
                { "a narrow pulse", { "pulse", "reedy" }, [] (PatchSpec& s) { sqr (s, 0, 0.75f, 0.25f); } },
                { "a five-voice supersaw", { "supersaw", "anthem" }, [] (PatchSpec& s) { wavetable (s, 0, "Saw Stack", 0.5f, 0.7f); unison (s, 0, 5, 0.3f, 0.8f); } },
            },
            {
                { "mono glide with mod-wheel vibrato", { "mono", "glide" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 3000.0f, 0.25f, 0.25f, 0.2f); ampEnv (s, 0.005f, 0.3f, 0.9f, 0.2f); voice (s, 1, 0.07f, 1); vibrato (s, 1, 5.6f, 0.25f); } },
                { "a ping-pong delay", { "delay", "wide" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP24, 4500.0f, 0.15f, 0.1f, 0.1f); ampEnv (s, 0.004f, 0.3f, 0.85f, 0.3f); voice (s, 2, 0.04f, 1); fx (s, FxStereoDelay, 0.3f, { { "feedback", 0.45f }, { "pingpong", 1.0f } }); } },
                { "tube distortion", { "distorted", "rock" }, [] (PatchSpec& s) { filter (s, 0, MS, LP12, 3500.0f, 0.3f, 0.3f, 0.15f); ampEnv (s, 0.004f, 0.3f, 0.9f, 0.25f); voice (s, 1, 0.03f, 1); fx (s, FxTube, 0.45f); } },
                { "a filter-envelope bite", { "bite", "envelope" }, [] (PatchSpec& s) { filter (s, 0, P5, LP24, 1400.0f, 0.4f, 0.25f, 0.45f); fltEnv (s, 0.002f, 0.35f, 0.3f, 0.3f); ampEnv (s, 0.003f, 0.4f, 0.85f, 0.25f); voice (s, 2, 0.02f, 1); } },
                { "a legato portamento in a plate", { "legato", "plate" }, [] (PatchSpec& s) { filter (s, 0, JP, LP24, 3800.0f, 0.2f, 0.15f, 0.1f); ampEnv (s, 0.01f, 0.3f, 0.95f, 0.35f); voice (s, 2, 0.12f, 1); space (s, 0.22f, 0.6f, 2.0f, 1); } },
            },
            { "Solar", "Burning", "Sharp", "Neon", "Singing", "Electric", "Screaming", "Lucid", "Bright", "Cutting", "Molten", "Brave" },
            { "Lead", "Hero", "Solo", "Arc", "Flare", "Line", "Voice", "Blade", "Comet", "Signal", "Beam", "Phoenix" } });

        // ---------------- brass ------------------------------------------------------------------
        f.push_back ({ "Brass", 30,
            {
                { "the Brass wavetable", { "brass", "digital" }, [] (PatchSpec& s) { wavetable (s, 0, "Brass", 0.45f, 0.75f); unison (s, 0, 2, 0.1f, 0.6f); } },
                { "a pair of saws", { "saw", "analog" }, [] (PatchSpec& s) { saw (s, 0, 0.65f); saw (s, 1, 0.6f); pitch (s, 1, 0, 0, 8.0f); } },
                { "the Brass Section multisample", { "section", "sampled" }, [] (PatchSpec& s) { multisample (s, 0, "Brass Section", 0.8f); } },
                { "pulse and saw", { "pulse", "saw" }, [] (PatchSpec& s) { sqr (s, 0, 0.55f, 0.35f); saw (s, 1, 0.6f); } },
                { "cross-modulated sines", { "fm", "horn" }, [] (PatchSpec& s) { sine (s, 0, 0.75f); sine (s, 1, 0.5f); xmod (s, 0, 2, 1, 0.3f); env (s, 2, 0.08f, 0.4f, 0.5f, 0.3f); s.addMod (modsrc::Env3, "osc_a_xmod_amount", 0.4f); } },
                { "a four-voice unison saw", { "unison", "section" }, [] (PatchSpec& s) { saw (s, 0, 0.7f); unison (s, 0, 4, 0.12f, 0.7f); } },
            },
            {
                { "a slow swell", { "swell", "slow" }, [] (PatchSpec& s) { filter (s, 0, P5, LP24, 900.0f, 0.2f, 0.2f, 0.4f); fltEnv (s, 0.35f, 0.8f, 0.6f, 0.6f); ampEnv (s, 0.25f, 0.8f, 0.9f, 0.6f); space (s, 0.25f, 0.75f, 3.0f, 0); } },
                { "a stab", { "stab", "short" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 1200.0f, 0.25f, 0.25f, 0.45f); fltEnv (s, 0.005f, 0.25f, 0.25f, 0.2f); ampEnv (s, 0.004f, 0.3f, 0.4f, 0.2f); space (s, 0.18f, 0.5f, 1.4f, 2); } },
                { "velocity brightness", { "velocity", "expressive" }, [] (PatchSpec& s) { filter (s, 0, JP, LP24, 1000.0f, 0.2f, 0.15f, 0.35f); velocityToCutoff (s, 0, 0.6f); fltEnv (s, 0.05f, 0.6f, 0.5f, 0.4f); ampEnv (s, 0.05f, 0.6f, 0.85f, 0.4f); } },
                { "chorus and a plate", { "chorus", "plate" }, [] (PatchSpec& s) { filter (s, 0, JN, LP24, 1800.0f, 0.15f, 0.1f, 0.3f, 0.4f, 0.33f); fltEnv (s, 0.08f, 0.6f, 0.5f, 0.4f); ampEnv (s, 0.06f, 0.6f, 0.85f, 0.5f); fx (s, FxChorus, 0.35f); space (s, 0.22f, 0.55f, 2.0f, 1); } },
                { "a hall fanfare", { "fanfare", "hall" }, [] (PatchSpec& s) { filter (s, 0, SEM, LP12, 1600.0f, 0.2f, 0.15f, 0.35f); fltEnv (s, 0.1f, 0.7f, 0.6f, 0.5f); ampEnv (s, 0.08f, 0.8f, 0.9f, 0.8f); space (s, 0.32f, 0.85f, 4.0f, 0); } },
            },
            { "Bold", "Royal", "Brassy", "Golden", "Proud", "Blazing", "Mighty", "Polished", "Heroic", "Gleaming" },
            { "Horns", "Fanfare", "Section", "Brass", "Trumpets", "Herald", "Salute", "March", "Anthem", "Call" } });

        // ---------------- keys -------------------------------------------------------------------
        f.push_back ({ "Keys", 30,
            {
                { "the Tine Piano multisample", { "ep", "tine" }, [] (PatchSpec& s) { multisample (s, 0, "Tine Piano", 0.8f); } },
                { "the Felt Piano multisample", { "piano", "felt" }, [] (PatchSpec& s) { multisample (s, 0, "Felt Piano", 0.8f); } },
                { "the Electric Tine wavetable", { "ep", "digital" }, [] (PatchSpec& s) { wavetable (s, 0, "Electric Tine", 0.3f, 0.8f); } },
                { "the Digital FM wavetable", { "fm", "ep" }, [] (PatchSpec& s) { wavetable (s, 0, "Digital FM", 0.25f, 0.8f); env (s, 2, 0.0f, 0.8f, 0.1f, 0.5f); s.addMod (modsrc::Env3, "osc_a_position", 0.3f); } },
                { "the Drawbar Organ wavetable", { "organ", "drawbar" }, [] (PatchSpec& s) { wavetable (s, 0, "Drawbar Organ", 0.6f, 0.8f); } },
                { "the Hollow Square wavetable", { "clav", "hollow" }, [] (PatchSpec& s) { wavetable (s, 0, "Hollow Square", 0.7f, 0.75f); } },
            },
            {
                { "auto-pan tremolo", { "tremolo", "autopan" }, [] (PatchSpec& s) { filterOff (s, 0); ampEnv (s, 0.002f, 2.0f, 0.3f, 0.5f); lfo (s, 3, 0, 4.5f); s.addMod (modsrc::Lfo4, "osc_a_pan", 0.5f); space (s, 0.15f, 0.5f, 1.5f, 2); } },
                { "a chorus", { "chorus", "lush" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP24, 6000.0f, 0.1f, 0.05f, 0.0f); ampEnv (s, 0.002f, 1.8f, 0.35f, 0.5f); fx (s, FxChorus, 0.4f); space (s, 0.16f, 0.5f, 1.5f, 2); } },
                { "a phaser", { "phaser", "funky" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 4000.0f, 0.15f, 0.1f, 0.15f); ampEnv (s, 0.002f, 1.2f, 0.3f, 0.4f); fx (s, FxPhaser, 0.35f); } },
                { "a dry room", { "dry", "intimate" }, [] (PatchSpec& s) { filterOff (s, 0); ampEnv (s, 0.002f, 2.2f, 0.25f, 0.6f); space (s, 0.12f, 0.35f, 0.8f, 2); } },
                { "velocity-controlled brightness", { "velocity", "dynamic" }, [] (PatchSpec& s) { filter (s, 0, SEM, LP12, 2200.0f, 0.1f, 0.05f, 0.1f); velocityToCutoff (s, 0, 0.6f); ampEnv (s, 0.002f, 1.6f, 0.3f, 0.5f); space (s, 0.15f, 0.5f, 1.4f, 3); } },
            },
            { "Smoky", "Soft", "Velvet", "Bright", "Late", "Rainy", "Vintage", "Warm", "Jazz", "Mellow" },
            { "Keys", "Piano", "Lounge", "Tines", "Ballad", "Club", "Studio", "Parlor", "Session", "Groove" } });

        // ---------------- cinematic --------------------------------------------------------------
        f.push_back ({ "Cinematic", 30,
            {
                { "a granular texture", { "granular", "texture" }, [] (PatchSpec& s) { granular (s, 0, "Granular Texture", 0.5f, 200.0f, 18.0f, 0.5f, 0.7f); } },
                { "the Analog Drone sample", { "drone", "dark" }, [] (PatchSpec& s) { sample (s, 0, "Analog Drone", 0.8f, 48, 1); } },
                { "noise spectra", { "noise", "spectral" }, [] (PatchSpec& s) { spectral (s, 0, "Noise Spectra", false, 0.7f); } },
                { "granular metal hits", { "metal", "granular" }, [] (PatchSpec& s) { granular (s, 0, "Metal Hit", 0.2f, 80.0f, 30.0f, 0.4f, 0.65f); } },
                { "a vinyl-noise pad", { "vinyl", "nostalgic" }, [] (PatchSpec& s) { wavetable (s, 0, "Soft Pad", 0.5f, 0.6f); sample (s, 1, "Vinyl Noise", 0.4f, 60, 1); } },
                { "the Vocal Aah sample", { "vocal", "haunting" }, [] (PatchSpec& s) { sample (s, 0, "Vocal Aah", 0.8f, 60, 1); } },
            },
            {
                { "a rising pitch envelope", { "riser", "tension" }, [] (PatchSpec& s) { filter (s, 0, SVF, LP24, 1500.0f, 0.3f, 0.1f, 0.0f); env (s, 2, 4.0f, 0.1f, 1.0f, 2.0f); s.addMod (modsrc::Env3, "flt1_cutoff", 0.6f); pitchEnv (s, 3, 1, 12.0f, 4.0f); ampEnv (s, 2.0f, 1.0f, 1.0f, 2.5f); space (s, 0.4f, 0.9f, 6.0f, 0); } },
                { "a deep ambient drone", { "drone", "ambient" }, [] (PatchSpec& s) { filter (s, 0, MG, LP24, 800.0f, 0.25f, 0.15f, 0.0f); filterLfo (s, 1, 0, 0.05f, 0.25f); ampEnv (s, 2.5f, 2.0f, 1.0f, 4.0f); space (s, 0.5f, 1.0f, 12.0f, 4); } },
                { "a reverse reverb", { "reverse", "swell" }, [] (PatchSpec& s) { filter (s, 0, SEM, LP12, 2500.0f, 0.15f, 0.05f, 0.0f); ampEnv (s, 0.5f, 1.5f, 0.8f, 2.0f); space (s, 0.45f, 0.8f, 3.0f, 7); } },
                { "a tempo-synced pulse", { "pulse", "rhythmic" }, [] (PatchSpec& s) { filter (s, 0, JP, LP24, 1800.0f, 0.35f, 0.15f, 0.0f); filterLfo (s, 1, 0, 1.0f, 0.4f, true, 8); ampEnv (s, 0.3f, 1.0f, 1.0f, 2.0f); space (s, 0.35f, 0.85f, 5.0f, 0); } },
                { "a slow tension sweep", { "tension", "sweep" }, [] (PatchSpec& s) { filter (s, 0, P5, LP24, 400.0f, 0.45f, 0.2f, 0.6f); fltEnv (s, 6.0f, 2.0f, 1.0f, 3.0f); ampEnv (s, 1.5f, 2.0f, 1.0f, 3.0f); space (s, 0.4f, 0.95f, 8.0f, 4); } },
            },
            { "Dark", "Hidden", "Ominous", "Vast", "Eerie", "Silent", "Deep", "Distant", "Fading", "Burning", "Cold", "Shadow" },
            { "Horizon", "Abyss", "Monolith", "Signal", "Descent", "Nebula", "Ruins", "Expanse", "Vault", "Omen", "Frontier", "Eclipse" } });

        return f;
    }

    // =================================================================================
    // Original FM voices for DX-style presets: ten instrument types x four variants.
    struct FmDesign { const char* name; const char* desc; std::vector<const char*> tags; fm::Patch voice; int fx; };

    fm::Operator op (int coarse, int fine, int level, std::array<uint8_t, 4> r, std::array<uint8_t, 4> l, int vel, int det, int rs = 2, int fixed = 0)
    {
        fm::Operator o;
        o.coarse = (uint8_t) coarse; o.fine = (uint8_t) fine; o.outputLevel = (uint8_t) level; o.rate = r; o.level = l;
        o.velSens = (uint8_t) vel; o.detune = (uint8_t) det; o.rateScale = (uint8_t) rs; o.fixedMode = (uint8_t) fixed;
        return o;
    }

    std::vector<FmDesign> fmDesigns()
    {
        std::vector<FmDesign> d;
        auto make = [&d] (const char* name, const char* desc, std::vector<const char*> tags, int algo, int fb, std::array<fm::Operator, 6> ops, int fx)
        {
            fm::Patch p;
            p.op = ops;
            p.algorithm = (uint8_t) (algo - 1);
            p.feedback = (uint8_t) fb;
            juce::String n (name);
            p.setName (n.toUpperCase().substring (0, 10));
            p.clamp();
            d.push_back ({ name, desc, tags, p, fx });
        };
        // electric pianos (algorithm 5 pairs): tine brightness, bark, decay and detune vary
        const int epMod[4] = { 46, 58, 66, 52 };
        const int epRatio[4] = { 14, 14, 1, 3 };
        const int epDecay[4] = { 28, 22, 34, 18 };
        const char* epNames[4] = { "Satin Tine EP", "Bark Tine EP", "Round Body EP", "Bell Tine EP" };
        for (int v = 0; v < 4; ++v)
            make (epNames[v], "Electric piano: three FM operator pairs with velocity-sensitive tines.", { "fm", "ep", "keys" }, 5, 4 + v % 3,
                  { op (1, 0, 99, { 96, (uint8_t) epDecay[v], 24, 62 }, { 99, 82, 0, 0 }, 2, 6),
                    op (epRatio[v], 0, epMod[v], { 95, 50, 30, 70 }, { 99, 20, 0, 0 }, 7, 7),
                    op (1, 0, 93, { 95, (uint8_t) (epDecay[v] + 2), 24, 62 }, { 99, 80, 0, 0 }, 2, 8),
                    op (1, 0, 70 + v * 3, { 95, 40, 26, 65 }, { 99, 60, 0, 0 }, 6, 6),
                    op (1, 0, 80, { 94, 32, 24, 60 }, { 99, 78, 0, 0 }, 2, 9),
                    op (1, 0, 60 + v * 4, { 92, 45, 28, 64 }, { 99, 55, 0, 0 }, 5, 5) }, 0);
        // bells: inharmonic ratios
        const int bellRatio[4][2] = { { 3, 50 }, { 2, 74 }, { 5, 20 }, { 7, 3 } };
        const char* bellNames[4] = { "Cathedral FM Bell", "Ice FM Bell", "Temple FM Bell", "Tiny FM Bell" };
        for (int v = 0; v < 4; ++v)
            make (bellNames[v], "Bell: inharmonic modulator ratios over sine carriers, long decay.", { "fm", "bell" }, 5, 2 + v,
                  { op (1, 0, 99, { 98, (uint8_t) (16 + v * 4), 20, 28 }, { 99, 60, 0, 0 }, 2, 7),
                    op (bellRatio[v][0], bellRatio[v][1], 70, { 98, 26, 20, 30 }, { 99, 45, 0, 0 }, 4, 8),
                    op (2, 0, 88, { 98, (uint8_t) (20 + v * 4), 18, 26 }, { 99, 58, 0, 0 }, 2, 6),
                    op (bellRatio[(v + 1) % 4][0] + 1, bellRatio[v][1], 62, { 98, 30, 20, 30 }, { 99, 40, 0, 0 }, 4, 7),
                    op (4, 0, 80, { 98, 24, 18, 26 }, { 99, 55, 0, 0 }, 2, 9),
                    op (9, 41, 55, { 98, 32, 20, 30 }, { 99, 30, 0, 0 }, 4, 5) }, 1);
        // basses: one carrier, modulator stack
        const char* bassNames[4] = { "Slap FM Bass", "Wood FM Bass", "Synth FM Bass", "Picked FM Bass" };
        for (int v = 0; v < 4; ++v)
            make (bassNames[v], "Bass: one carrier with a decaying modulator stack and feedback.", { "fm", "bass" }, 16, 4 + v,
                  { op (1, 0, 99, { 99, (uint8_t) (36 + v * 6), 30, 72 }, { 99, 84, 60, 0 }, 3, 7),
                    op (v == 2 ? 1 : 2, 0, 70 + v * 4, { 99, (uint8_t) (55 - v * 5), 35, 70 }, { 99, 50, 30, 0 }, 6, 7),
                    op (1, 0, 66, { 99, 45, 30, 70 }, { 99, 70, 50, 0 }, 4, 8),
                    op (3, 0, 52 + v * 3, { 99, 60, 40, 70 }, { 99, 40, 20, 0 }, 6, 7),
                    op (1, 0, 60, { 99, 50, 30, 70 }, { 99, 60, 40, 0 }, 4, 6),
                    op (v + 1, 0, 50, { 99, 65, 40, 70 }, { 99, 35, 15, 0 }, 6, 7) }, 2);
        // brass: one modulator feeding four carriers, slow attack
        const char* brassNames[4] = { "Soft FM Horns", "Bright FM Brass", "Dark FM Horns", "Punch FM Brass" };
        for (int v = 0; v < 4; ++v)
            make (brassNames[v], "Brass: a shared modulator over detuned carriers with an envelope-shaped attack.", { "fm", "brass" }, 22, 6,
                  { op (1, 0, 97, { (uint8_t) (52 + v * 6), 40, 30, 55 }, { 99, 92, 88, 0 }, 2, 7),
                    op (1, 0, 70 + v * 3, { (uint8_t) (50 + v * 6), 42, 30, 55 }, { 99, 86, 80, 0 }, 4, 7),
                    op (1, 0, 95, { (uint8_t) (54 + v * 6), 40, 30, 55 }, { 99, 92, 88, 0 }, 2, 6),
                    op (1, 0, 94, { (uint8_t) (53 + v * 6), 40, 30, 55 }, { 99, 92, 88, 0 }, 2, 8),
                    op (1, 0, 93, { (uint8_t) (51 + v * 6), 40, 30, 55 }, { 99, 92, 88, 0 }, 2, 9),
                    op (1, 0, 66 + v * 5, { (uint8_t) (45 + v * 6), 38, 30, 55 }, { 99, 88, 82, 0 }, 5, 7) }, 1);
        // organs: drawbar-like carrier ratios
        const int drawbars[4][6] = { { 0, 1, 2, 3, 4, 6 }, { 1, 2, 3, 4, 6, 8 }, { 0, 1, 3, 5, 6, 8 }, { 1, 3, 5, 7, 9, 11 } };
        const char* organNames[4] = { "Full FM Drawbars", "Bright FM Organ", "Jazz FM Organ", "Odd Harmonic Organ" };
        for (int v = 0; v < 4; ++v)
        {
            std::array<fm::Operator, 6> ops;
            for (int i = 0; i < 6; ++i) ops[(size_t) i] = op (drawbars[v][i], 0, 92 - i * 4 + (v == 2 && i == 1 ? 6 : 0), { 99, 99, 99, 80 }, { 99, 99, 99, 0 }, 0, 7 + (i % 3) - 1);
            make (organNames[v], "Organ: six sine carriers at drawbar-like ratios.", { "fm", "organ" }, 32, v, ops, 3);
        }
        // mallets
        const char* malletNames[4] = { "Rose FM Marimba", "Steel FM Vibes", "Wood FM Block", "Toy FM Mallet" };
        const int malletRatio[4] = { 4, 3, 10, 7 };
        for (int v = 0; v < 4; ++v)
            make (malletNames[v], "Mallet: fast-decaying high-ratio modulators.", { "fm", "mallet" }, 7, v,
                  { op (1, 0, 99, { 99, (uint8_t) (40 + v * 4), 30, 50 }, { 99, 62, 0, 0 }, 3, 7),
                    op (malletRatio[v], 0, 64 + v * 2, { 99, 72, 40, 60 }, { 99, 0, 0, 0 }, 6, 7),
                    op (1, 0, 95, { 99, (uint8_t) (44 + v * 4), 30, 50 }, { 99, 58, 0, 0 }, 3, 7),
                    op (malletRatio[(v + 2) % 4], 0, 54, { 99, 80, 40, 60 }, { 99, 0, 0, 0 }, 7, 7),
                    op (3, 0, 56, { 99, 75, 40, 60 }, { 99, 0, 0, 0 }, 6, 7),
                    op (1, 0, 40, { 99, 70, 40, 60 }, { 99, 0, 0, 0 }, 4, 7) }, 1);
        // plucks
        const char* pluckNames[4] = { "Koto FM Pluck", "Harp FM Pluck", "Nylon FM Pluck", "Spark FM Pluck" };
        for (int v = 0; v < 4; ++v)
            make (pluckNames[v], "Pluck: two operator stacks with short modulator envelopes.", { "fm", "pluck" }, 1, 3 + v,
                  { op (1, 0, 99, { 99, (uint8_t) (32 + v * 3), 25, 45 }, { 99, 58, 0, 0 }, 3, 7),
                    op (2 + v % 2, 0, 70 + v * 3, { 99, 52, 30, 50 }, { 99, 28, 0, 0 }, 6, 7),
                    op (1, 0, 96, { 99, (uint8_t) (36 + v * 3), 25, 45 }, { 99, 56, 0, 0 }, 3, 8),
                    op (1, 0, 68, { 99, 56, 30, 50 }, { 99, 28, 0, 0 }, 5, 6),
                    op (3 + v, 0, 58, { 99, 62, 30, 50 }, { 99, 18, 0, 0 }, 5, 7),
                    op (1, 0, 54, { 99, 64, 30, 50 }, { 99, 8, 0, 0 }, 4, 7) }, 2);
        // pads
        const char* padNames[4] = { "Crystal FM Pad", "Warm FM Pad", "Metal FM Pad", "Choir FM Pad" };
        for (int v = 0; v < 4; ++v)
            make (padNames[v], "Pad: slow operator envelopes with light, detuned modulation.", { "fm", "pad" }, 2, 3 + v,
                  { op (1, 0, 96, { 40, 30, 20, 35 }, { 99, 90, 85, 0 }, 1, 7 + v % 2),
                    op (1, v, 60 + v * 4, { 35, 25, 20, 35 }, { 99, 80, 70, 0 }, 2, 9),
                    op (1, 0, 94, { 42, 30, 20, 35 }, { 99, 90, 85, 0 }, 1, 5),
                    op (v + 2, 0, 58 + v * 2, { 30, 22, 18, 35 }, { 99, 70, 60, 0 }, 2, 7),
                    op (1, 50, 52, { 28, 20, 18, 35 }, { 99, 65, 55, 0 }, 2, 8),
                    op (5, 0, 46 + v * 3, { 25, 20, 18, 35 }, { 99, 60, 50, 0 }, 2, 7) }, 4);
        // leads
        const char* leadNames[4] = { "Saw FM Lead", "Hollow FM Lead", "Sync FM Lead", "Flute FM Lead" };
        for (int v = 0; v < 4; ++v)
            make (leadNames[v], "Lead: operator feedback for saw- and square-like spectra.", { "fm", "lead" }, 1, v == 3 ? 2 : 7 - v,
                  { op (1, 0, 99, { 92, 40, 30, 60 }, { 99, 90, 85, 0 }, 2, 7),
                    op (v == 1 ? 2 : 1, 0, 76 + v * 2, { 92, 40, 30, 60 }, { 99, 85, 80, 0 }, 3, 8),
                    op (1, 0, 96, { 92, 40, 30, 60 }, { 99, 90, 85, 0 }, 2, 6),
                    op (2, 0, 66, { 92, 45, 30, 60 }, { 99, 80, 70, 0 }, 3, 7),
                    op (1, 0, 70, { 92, 45, 30, 60 }, { 99, 80, 70, 0 }, 3, 7),
                    op (1, 0, 72 - v * 6, { 92, 45, 30, 60 }, { 99, 80, 70, 0 }, 3, 7) }, 2);
        // clavs
        const char* clavNames[4] = { "Funk FM Clav", "Muted FM Clav", "Wah FM Clav", "Bright FM Clav" };
        for (int v = 0; v < 4; ++v)
            make (clavNames[v], "Clav: percussive odd-ratio modulator chain.", { "fm", "clav" }, 3, 5 + v % 3,
                  { op (1, 0, 99, { 99, (uint8_t) (48 + v * 3), 40, 70 }, { 99, 74, 0, 0 }, 4, 7),
                    op (3, 0, 76 + v * 2, { 99, 70, 40, 70 }, { 99, 40, 0, 0 }, 6, 7),
                    op (5, 0, 62 + v * 3, { 99, 75, 40, 70 }, { 99, 20, 0, 0 }, 6, 7),
                    op (1, 0, 95, { 99, (uint8_t) (50 + v * 3), 40, 70 }, { 99, 74, 0, 0 }, 4, 8),
                    op (2, 0, 68, { 99, 70, 40, 70 }, { 99, 30, 0, 0 }, 6, 6),
                    op (7, 0, 50 + v * 2, { 99, 80, 40, 70 }, { 99, 10, 0, 0 }, 6, 7) }, 3);
        return d;
    }
}

// =================================================================================
std::vector<PatchSpec> build()
{
    std::vector<PatchSpec> out;
    std::set<juce::String> names;
    auto uniqueName = [&names] (juce::String n)
    {
        if (names.count (n) == 0) { names.insert (n); return n; }
        for (int i = 2; i < 100; ++i)
            if (names.count (n + " " + juce::String (i)) == 0) { names.insert (n + " " + juce::String (i)); return n + " " + juce::String (i); }
        return n;
    };

    for (const auto& fam : families())
    {
        const int V = (int) fam.voicings.size(), T = (int) fam.treatments.size();
        const int A = (int) fam.adjectives.size(), N = (int) fam.nouns.size();
        // Every (voicing, treatment) pair is used at most once: candidate k = q*V + v pairs voicing v with treatment
        // (v + q) % T, which is injective for any V and T, and neighbours differ in both building blocks.
        // Musically incompatible pairs (a slow-speaking source with a short envelope) are skipped.
        int i = 0;
        for (int k = 0; k < V * T && i < fam.count; ++k)
        {
            const int v = k % V;
            const int t = (v + k / V) % T;
            if ((fam.voicings[(size_t) v].flags & kSlowSource) != 0 && (fam.treatments[(size_t) t].flags & kShortEnvelope) != 0)
                continue;
            const auto& vo = fam.voicings[(size_t) v];
            const auto& tr = fam.treatments[(size_t) t];
            const auto name = uniqueName (juce::String (fam.adjectives[(size_t) ((i * 7 + i / A) % A)]) + " "
                                          + fam.nouns[(size_t) ((i * 5 + i / N + 3) % N)]);
            PatchSpec s = init (name, fam.category);
            vo.apply (s);
            tr.apply (s);
            macros (s);
            s.meta.name = name;
            s.meta.category = fam.category;
            s.meta.description = juce::String (fam.category) + ": " + vo.label + " with " + tr.label + ".";
            s.meta.source = "factory";
            s.meta.author = "VICE Factory";
            s.meta.tags.clear();
            for (auto* tag : vo.tags) s.meta.tags.addIfNotAlreadyThere (tag);
            for (auto* tag : tr.tags) s.meta.tags.addIfNotAlreadyThere (tag);
            out.push_back (std::move (s));
            ++i;
        }
        jassert (i == fam.count); // every family must reach its count with distinct pairs
    }

    for (const auto& d : fmDesigns())
    {
        const auto name = uniqueName (d.name);
        PatchSpec s = init (name, "FM");
        s.p ("osc_a_mode", 6.0f).p ("osc_a_level", 0.85f);
        s.fmVoice[0] = d.voice.toHex();
        filterOff (s, 0);
        filterOff (s, 1);
        env (s, 0, 0.001f, 0.5f, 1.0f, fm::releaseSeconds (d.voice));
        switch (d.fx)
        {
            case 0: fx (s, FxChorus, 0.3f); space (s, 0.16f, 0.5f, 1.6f, 1); break;
            case 1: space (s, 0.25f, 0.75f, 3.5f, 0); break;
            case 2: break; // dry
            case 3: fx (s, FxChorus, 0.3f); space (s, 0.14f, 0.45f, 1.4f, 2); break;
            default: space (s, 0.35f, 0.85f, 5.0f, 4); break;
        }
        s.meta.name = name;
        s.meta.category = "FM";
        s.meta.description = d.desc;
        s.meta.source = "factory";
        s.meta.author = "VICE Factory";
        s.meta.tags.clear();
        for (auto* tag : d.tags) s.meta.tags.add (tag);
        out.push_back (std::move (s));
    }
    return out;
}
} // namespace vsx::FactoryExpansion
