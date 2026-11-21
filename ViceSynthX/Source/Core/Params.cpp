#include "Params.h"

namespace vsx
{
namespace choices
{
    const juce::StringArray oscModes { "Wavetable", "Analog", "Sample", "Multisample", "Granular", "Spectral" };
    const juce::StringArray oscSources { "Wavetable", "Sample" };
    const juce::StringArray warpModes { "Off", "Sync", "Bend +", "Bend -", "PWM", "Mirror", "Asym", "Quantize" };
    const juce::StringArray xmodModes { "Off", "FM", "PM", "RM", "AM", "Hard Sync" };
    const juce::StringArray xmodSources { "OSC A", "OSC B", "OSC C", "Sub", "Noise" };
    const juce::StringArray routes { "Filter 1", "Filter 2", "Filter 1+2", "Direct" };
    const juce::StringArray loopModes { "Off", "Forward", "Ping-Pong" };
    const juce::StringArray grainWindows { "Hann", "Triangle", "Tukey", "Exponential" };
    const juce::StringArray subShapes { "Sine", "Triangle", "Saw", "Square" };
    const juce::StringArray noiseTypes { "White", "Pink", "Brown", "Digital", "Crackle" };
    const juce::StringArray filterModels { "MG Ladder", "Diode Ladder", "Acid Diode", "MS Sallen-Key", "JP OTA",
                                           "JN OTA", "P5 Cascade", "SEM State-Var", "Analog SVF" };
    const juce::StringArray filterModes { "Low-pass 24", "Low-pass 12", "Band-pass", "High-pass", "Notch", "Morph" };
    const juce::StringArray filterRoutings { "Serial", "Parallel" };
    const juce::StringArray voiceModes { "Poly", "Mono", "Legato" };
    const juce::StringArray glideModes { "Always", "Legato Only" };
    const juce::StringArray qualities { "Eco (1x)", "High (2x)", "Ultra (4x)" };
    const juce::StringArray lfoShapes { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold",
                                        "Smooth Random", "Custom" };
    const juce::StringArray lfoModes { "Free", "Retrigger", "Envelope" };
    const juce::StringArray fxRoutings { "Serial", "Parallel A|B" };
    const juce::StringArray fxBuses { "A", "B" };
    const juce::StringArray distTypes { "Soft Clip", "Hard Clip", "Wavefold", "Rectify", "Bitcrush", "Downsample" };
    const juce::StringArray dimensionModes { "Mode 1", "Mode 2", "Mode 3", "Mode 4" };
    const juce::StringArray convImpulses { "Concert Hall", "Plate", "Room", "Chamber", "Spring", "Cathedral",
                                           "Reverse Swell", "User IR" };
    const juce::StringArray ringShapes { "Sine", "Square", "Triangle" };
    const juce::StringArray arpModes { "Off", "Arpeggiator", "Sequencer" };
    const juce::StringArray arpPatterns { "Up", "Down", "Up-Down", "Down-Up", "As Played", "Random", "Chord", "Converge" };
    const juce::StringArray arpOctaveModes { "Up", "Down", "Up-Down", "Random" };
    const juce::StringArray arpVelModes { "As Played", "Fixed", "Accent" };

    const juce::StringArray fxTypeNames { "Distortion", "Tube Saturation", "Tape Saturation", "Parametric EQ",
                                          "Multiband Comp", "Frequency Shifter", "Ring Modulator", "Analog Chorus",
                                          "Dimension Chorus", "Flanger", "Phaser", "Stereo Delay", "Tape Delay",
                                          "Digital Delay", "Algorithmic Reverb", "Convolution Reverb",
                                          "Stereo Enhancer", "Limiter" };
    const juce::StringArray fxTypeIds { "distortion", "tube", "tape", "eq", "mbcomp", "freqshift", "ringmod", "chorus",
                                        "dimension", "flanger", "phaser", "stereodelay", "tapedelay", "digitaldelay",
                                        "reverb", "convolution", "stereo", "limiter" };

    juce::String filterModelDescription (int model)
    {
        switch (model)
        {
            case 0: return "4-pole transistor ladder (Moog-ladder-inspired): tanh per stage, global feedback, bass loss at high resonance, self-oscillates. Warm, round, classic analog bass/lead.";
            case 1: return "4-pole diode ladder with coupled stages: thinner, buzzy, aggressive resonance, self-oscillates.";
            case 2: return "TB-303-inspired diode ladder: mismatched first stage, high-pass in the resonance path, squelchy 'acid' resonance. Character = accent/squelch.";
            case 3: return "Korg MS-20-inspired 2-pole Sallen-Key (Korg35-style) with clipping diodes in the feedback path: screaming, gritty resonance.";
            case 4: return "Roland Jupiter-8-inspired IR3109-style 4-pole OTA cascade: smooth, bright, 12/24 dB, clean self-oscillation.";
            case 5: return "Roland Juno-106-inspired OTA 4-pole low-pass with separate non-resonant high-pass (Character = HPF position 0-3, 0 boosts bass).";
            case 6: return "Sequential Prophet-5-inspired CEM/SSM-style 4-pole cascade: punchy, slightly gritty, resonance with partial gain compensation.";
            case 7: return "Oberheim SEM-inspired 2-pole state-variable: continuous LP-notch-HP morph (Character), soft, never fully self-oscillates.";
            case 8: return "Analog-style state-variable filter with nonlinear damping: 12 dB, clean resonance, self-oscillates, all modes.";
            default: return {};
        }
    }
}

// ---------------------------------------------------------------------------------
namespace modsrc
{
    juce::String name (int s)
    {
        if (s >= Env1 && s <= Env4) return "ENV " + juce::String (s - Env1 + 1);
        if (s >= Lfo1 && s <= Lfo10) return "LFO " + juce::String (s - Lfo1 + 1);
        if (s >= Macro1 && s <= Macro8) return "Macro " + juce::String (s - Macro1 + 1);
        switch (s)
        {
            case Velocity: return "Velocity";
            case Note: return "Note";
            case ModWheel: return "Mod Wheel";
            case Aftertouch: return "Aftertouch";
            case PitchBend: return "Pitch Bend";
            case Random: return "Random";
            case Gate: return "Gate";
            case Expression: return "Expression";
            case Breath: return "Breath";
            case Timbre: return "Timbre (CC74)";
            case SeqMod: return "Seq Mod";
            default: return "None";
        }
    }

    juce::String id (int s)
    {
        if (s >= Env1 && s <= Env4) return "env" + juce::String (s - Env1 + 1);
        if (s >= Lfo1 && s <= Lfo10) return "lfo" + juce::String (s - Lfo1 + 1);
        if (s >= Macro1 && s <= Macro8) return "macro" + juce::String (s - Macro1 + 1);
        switch (s)
        {
            case Velocity: return "velocity";
            case Note: return "note";
            case ModWheel: return "modwheel";
            case Aftertouch: return "aftertouch";
            case PitchBend: return "pitchbend";
            case Random: return "random";
            case Gate: return "gate";
            case Expression: return "expression";
            case Breath: return "breath";
            case Timbre: return "timbre";
            case SeqMod: return "seqmod";
            default: return "none";
        }
    }

    int fromId (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase().removeCharacters (" _-");
        for (int s = 0; s < Count; ++s)
        {
            if (id (s) == t || name (s).toLowerCase().removeCharacters (" _-()") == t)
                return s;
        }
        if (t == "envelope1" || t == "ampenv") return Env1;
        if (t == "envelope2" || t == "filterenv") return Env2;
        if (t == "envelope3") return Env3;
        if (t == "envelope4") return Env4;
        if (t == "mw" || t == "cc1") return ModWheel;
        if (t == "vel") return Velocity;
        if (t == "keytrack" || t == "key") return Note;
        if (t == "at" || t == "pressure") return Aftertouch;
        if (t == "cc74") return Timbre;
        if (t == "cc11") return Expression;
        if (t == "cc2") return Breath;
        return -1;
    }

    bool isBipolar (int s)
    {
        return (s >= Lfo1 && s <= Lfo10) || s == PitchBend || s == Note || s == Random || s == SeqMod;
    }
}

// ---------------------------------------------------------------------------------
float ParamDef::snap (float v) const
{
    v = juce::jlimit (minValue, maxValue, v);
    if (type != ParamType::Float)
        v = std::round (v);
    return v;
}

static juce::String trimNumber (double v, int decimals)
{
    return juce::String (v, decimals);
}

juce::String ParamRegistry::formatValue (const ParamDef& def, float v, int)
{
    switch (def.type)
    {
        case ParamType::Bool: return v >= 0.5f ? "On" : "Off";
        case ParamType::Choice:
        {
            const int i = juce::jlimit (0, juce::jmax (0, def.choices.size() - 1), (int) std::round (v));
            return def.choices[i];
        }
        case ParamType::Int:
        {
            const int i = (int) std::round (v);
            if (def.unit == "st" && i > 0) return "+" + juce::String (i) + " st";
            return juce::String (i) + (def.unit.isEmpty() ? "" : " " + def.unit);
        }
        case ParamType::Float: break;
    }

    const auto& u = def.unit;
    if (u == "Hz")
    {
        if (v >= 1000.0f) return trimNumber (v / 1000.0, v >= 10000.0f ? 1 : 2) + " kHz";
        if (v >= 100.0f) return trimNumber (v, 0) + " Hz";
        if (v >= 10.0f) return trimNumber (v, 1) + " Hz";
        return trimNumber (v, 2) + " Hz";
    }
    if (u == "s")
    {
        if (v < 0.01f) return trimNumber (v * 1000.0, 1) + " ms";
        if (v < 1.0f) return trimNumber (v * 1000.0, 0) + " ms";
        return trimNumber (v, 2) + " s";
    }
    if (u == "ms") return trimNumber (v, v < 10.0f ? 1 : 0) + " ms";
    if (u == "dB") return (v > 0.05f ? "+" : "") + trimNumber (v, 1) + " dB";
    if (u == "%") return trimNumber (v * 100.0, 0) + " %";
    if (u == "ct") return (v > 0.5f ? "+" : "") + trimNumber (v, 0) + " ct";
    if (u == "st") return (v > 0.05f ? "+" : "") + trimNumber (v, 1) + " st";
    if (u == "deg") return trimNumber (v, 0) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
    if (u == "x") return trimNumber (v, 2) + "x";
    return trimNumber (v, 2);
}

juce::StringArray ParamRegistry::syncDivisionNames()
{
    return { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D",
             "1/2T", "1/2", "1/2D", "1/1", "2/1", "4/1", "8/1" };
}

double ParamRegistry::syncDivisionBeats (int i)
{
    static const double beats[] = { 0.0625, 0.125 * 2.0 / 3.0, 0.125, 0.25 * 2.0 / 3.0, 0.25, 0.375, 0.5 * 2.0 / 3.0,
                                    0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 4.0 / 3.0, 2.0, 3.0, 4.0, 8.0, 16.0, 32.0 };
    return beats[juce::jlimit (0, 18, i)];
}

// ---------------------------------------------------------------------------------
namespace
{
    int& registryLayoutErrors() { static int errors = 0; return errors; }

    struct Builder
    {
        std::vector<ParamDef>& defs;

        ParamDef& push (ParamDef d, int expectedIndex)
        {
            d.index = (int) defs.size();
            jassert (d.index == expectedIndex);
            if (d.index != expectedIndex) ++registryLayoutErrors(); // checked by tests in Release builds too
            defs.push_back (std::move (d));
            return defs.back();
        }

        ParamDef& f (int idx, juce::String id, juce::String name, juce::String group, float mn, float mx, float def,
                     juce::String unit, bool mod, juce::String desc, float centre = -1.0f)
        {
            ParamDef d;
            d.id = id; d.name = name; d.group = group; d.unit = unit; d.description = desc;
            d.type = ParamType::Float; d.minValue = mn; d.maxValue = mx; d.defaultValue = def;
            d.range = juce::NormalisableRange<float> (mn, mx);
            if (centre > mn && centre < mx)
                d.range.setSkewForCentre (centre);
            d.modulatable = mod;
            return push (d, idx);
        }

        ParamDef& i (int idx, juce::String id, juce::String name, juce::String group, int mn, int mx, int def,
                     juce::String unit, bool mod, juce::String desc)
        {
            ParamDef d;
            d.id = id; d.name = name; d.group = group; d.unit = unit; d.description = desc;
            d.type = ParamType::Int; d.minValue = (float) mn; d.maxValue = (float) mx; d.defaultValue = (float) def;
            d.range = juce::NormalisableRange<float> ((float) mn, (float) mx, 1.0f);
            d.modulatable = mod;
            return push (d, idx);
        }

        ParamDef& b (int idx, juce::String id, juce::String name, juce::String group, bool def, juce::String desc)
        {
            ParamDef d;
            d.id = id; d.name = name; d.group = group; d.description = desc;
            d.type = ParamType::Bool; d.minValue = 0.0f; d.maxValue = 1.0f; d.defaultValue = def ? 1.0f : 0.0f;
            d.range = juce::NormalisableRange<float> (0.0f, 1.0f, 1.0f);
            return push (d, idx);
        }

        ParamDef& c (int idx, juce::String id, juce::String name, juce::String group, const juce::StringArray& ch,
                     int def, juce::String desc)
        {
            ParamDef d;
            d.id = id; d.name = name; d.group = group; d.description = desc;
            d.type = ParamType::Choice; d.choices = ch;
            d.minValue = 0.0f; d.maxValue = (float) (ch.size() - 1); d.defaultValue = (float) def;
            d.range = juce::NormalisableRange<float> (0.0f, (float) (ch.size() - 1), 1.0f);
            return push (d, idx);
        }
    };
}

ParamRegistry::ParamRegistry()
{
    using namespace pid;
    defs.reserve ((size_t) kNumParams);
    Builder r { defs };

    // ---- Global --------------------------------------------------------------
    {
        const juce::String g = "Global";
        r.f (G (global::MasterVolume), "master_volume", "Master Volume", g, -60.0f, 6.0f, -3.0f, "dB", false, "Final output level.", -12.0f);
        r.f (G (global::MasterTune), "master_tune", "Master Tune", g, -100.0f, 100.0f, 0.0f, "ct", false, "Global tuning offset in cents.");
        r.i (G (global::Transpose), "transpose", "Transpose", g, -24, 24, 0, "st", false, "Global transpose in semitones.");
        r.i (G (global::Polyphony), "polyphony", "Polyphony", g, 1, kMaxVoices, 16, "", false, "Maximum simultaneous voices; extra notes steal the quietest/oldest voice.");
        r.c (G (global::VoiceMode), "voice_mode", "Voice Mode", g, choices::voiceModes, 0, "Poly, Mono (retrigger) or Legato (no envelope retrigger on overlapping notes).");
        r.f (G (global::GlideTime), "glide_time", "Glide", g, 0.0f, 5.0f, 0.0f, "s", false, "Portamento time.", 0.3f);
        r.c (G (global::GlideMode), "glide_mode", "Glide Mode", g, choices::glideModes, 0, "Glide always, or only between overlapping (legato) notes.");
        r.i (G (global::BendUp), "bend_up", "Bend Up", g, 0, 24, 2, "st", false, "Pitch-bend range upwards.");
        r.i (G (global::BendDown), "bend_down", "Bend Down", g, 0, 24, 2, "st", false, "Pitch-bend range downwards.");
        r.f (G (global::AmpVelocity), "amp_velocity", "Velocity > Amp", g, 0.0f, 1.0f, 0.5f, "%", true, "How much note velocity scales the amplitude.");
        r.f (G (global::OscDrift), "osc_drift", "Analog Drift", g, 0.0f, 1.0f, 0.1f, "%", true, "Slow random per-voice oscillator pitch drift like analog VCOs.");
        r.c (G (global::Quality), "quality", "Quality", g, choices::qualities, 1, "Oversampling of the whole voice engine (oscillators + filters). Higher = less aliasing, more CPU.").automatable = false;
        r.c (G (global::FilterRouting), "filter_routing", "Filter Routing", g, choices::filterRoutings, 1, "Serial: Filter 1 feeds Filter 2. Parallel: both filters run side by side.");
        r.c (G (global::FxRouting), "fx_routing", "FX Routing", g, choices::fxRoutings, 0, "Serial: bus A then bus B. Parallel: bus A and B process the dry signal side by side.");
        r.f (G (global::FxBalance), "fx_balance", "FX A/B Balance", g, 0.0f, 1.0f, 0.5f, "%", true, "Balance between FX bus A and B in parallel routing.");
    }

    // ---- Oscillators ------------------------------------------------------------
    for (int o = 0; o < kNumOscs; ++o)
    {
        const juce::String letter = juce::String::charToString ((juce::juce_wchar) ('a' + o));
        const juce::String up = letter.toUpperCase();
        const juce::String p = "osc_" + letter + "_";
        const juce::String n = "OSC " + up + " ";
        const juce::String g = "OSC " + up;

        r.b (O (o, osc::On), p + "on", n + "On", g, o == 0, "Enables oscillator " + up + ".");
        r.c (O (o, osc::Mode), p + "mode", n + "Mode", g, choices::oscModes, 0,
             "Synthesis method: Wavetable (morphing single-cycle tables), Analog (band-limited virtual analog shapes), Sample, Multisample (key/velocity zones, SFZ), Granular (grain clouds from the sample or wavetable), Spectral (additive resynthesis of the wavetable/sample spectrum).");
        r.c (O (o, osc::Source), p + "source", n + "Source", g, choices::oscSources, 0,
             "Material used by the Granular and Spectral modes: the oscillator's wavetable or its loaded sample.");
        r.f (O (o, osc::Level), p + "level", n + "Level", g, 0.0f, 1.0f, 0.75f, "%", true, "Output level.");
        r.f (O (o, osc::Pan), p + "pan", n + "Pan", g, -1.0f, 1.0f, 0.0f, "%", true, "Stereo position (-1 left, +1 right).");
        r.i (O (o, osc::Octave), p + "octave", n + "Octave", g, -4, 4, 0, "oct", true, "Octave transpose.");
        r.i (O (o, osc::Semi), p + "semi", n + "Semi", g, -12, 12, 0, "st", true, "Semitone transpose.");
        r.f (O (o, osc::Fine), p + "fine", n + "Fine", g, -100.0f, 100.0f, 0.0f, "ct", true, "Fine tune in cents.");
        r.f (O (o, osc::Position), p + "position", n + "Position", g, 0.0f, 1.0f, 0.0f, "%", true,
             "Mode-dependent morph: wavetable frame (Wavetable/Spectral), shape sine>tri>saw>square (Analog), start offset (Sample/Multisample), read position (Granular).");
        r.i (O (o, osc::Unison), p + "unison", n + "Unison", g, 1, kMaxUnison, 1, "", false, "Number of unison voices (1-16).");
        r.f (O (o, osc::Detune), p + "detune", n + "Detune", g, 0.0f, 1.0f, 0.2f, "%", true, "Unison detune spread (up to +/-1 semitone at 100%).");
        r.f (O (o, osc::Blend), p + "blend", n + "Blend", g, 0.0f, 1.0f, 0.75f, "%", true, "Level of the outer unison voices relative to the centre voice.");
        r.f (O (o, osc::Spread), p + "spread", n + "Stereo Spread", g, 0.0f, 1.0f, 0.5f, "%", true, "Stereo width of the unison voices.");
        r.f (O (o, osc::Phase), p + "phase", n + "Phase", g, 0.0f, 360.0f, 0.0f, "deg", true, "Start phase.");
        r.f (O (o, osc::PhaseRand), p + "phase_rand", n + "Phase Random", g, 0.0f, 1.0f, 1.0f, "%", false, "Randomisation of start phase per note (100% = free-running analog feel).");
        r.c (O (o, osc::WarpMode), p + "warp_mode", n + "Warp", g, choices::warpModes, 0, "Phase-warp: Sync (virtual hard sync), Bend +/- (phase bend), PWM (squeeze), Mirror, Asym (phase distortion), Quantize.");
        r.f (O (o, osc::WarpAmount), p + "warp_amount", n + "Warp Amount", g, 0.0f, 1.0f, 0.0f, "%", true, "Amount of the selected warp.");
        r.c (O (o, osc::XModMode), p + "xmod_mode", n + "X-Mod", g, choices::xmodModes, 0, "Cross-modulation from another oscillator: FM (linear through-zero), PM (phase), RM (ring), AM, Hard Sync.");
        r.c (O (o, osc::XModSource), p + "xmod_source", n + "X-Mod Source", g, choices::xmodSources, o == 0 ? 1 : 0, "Modulating oscillator for X-Mod.");
        r.f (O (o, osc::XModAmount), p + "xmod_amount", n + "X-Mod Amount", g, 0.0f, 1.0f, 0.0f, "%", true, "FM index / PM depth / ring or AM mix.");
        r.f (O (o, osc::Fold), p + "fold", n + "Wavefold", g, 0.0f, 1.0f, 0.0f, "%", true, "Sine wavefolder drive: adds bright, metallic harmonics.");
        r.c (O (o, osc::Route), p + "route", n + "Route", g, choices::routes, 0, "Which filter(s) this oscillator feeds, or Direct (bypass filters).");
        r.f (O (o, osc::PulseWidth), p + "pulse_width", n + "Pulse Width", g, 0.02f, 0.98f, 0.5f, "%", true, "Pulse width for the Analog square shape (PWM).");
        r.c (O (o, osc::LoopMode), p + "loop_mode", n + "Loop", g, choices::loopModes, 0, "Sample loop mode.");
        r.f (O (o, osc::LoopStart), p + "loop_start", n + "Loop Start", g, 0.0f, 1.0f, 0.0f, "%", true, "Loop start (fraction of sample).");
        r.f (O (o, osc::LoopEnd), p + "loop_end", n + "Loop End", g, 0.0f, 1.0f, 1.0f, "%", true, "Loop end (fraction of sample).");
        r.f (O (o, osc::LoopXfade), p + "loop_xfade", n + "Loop X-Fade", g, 0.0f, 0.5f, 0.02f, "%", false, "Loop crossfade length (fraction of loop).");
        r.i (O (o, osc::RootNote), p + "root_note", n + "Root Key", g, 0, 127, 60, "", false, "MIDI root key of a single sample (60 = C4).");
        r.f (O (o, osc::GrainSize), p + "grain_size", n + "Grain Size", g, 5.0f, 1000.0f, 80.0f, "ms", true, "Grain length.", 80.0f);
        r.f (O (o, osc::GrainDensity), p + "grain_density", n + "Grain Density", g, 1.0f, 200.0f, 30.0f, "Hz", true, "Grains per second.", 30.0f);
        r.f (O (o, osc::GrainSpray), p + "grain_spray", n + "Grain Spray", g, 0.0f, 1.0f, 0.1f, "%", true, "Random position jitter of grains.");
        r.f (O (o, osc::GrainPitchRand), p + "grain_pitch_rand", n + "Grain Pitch Rand", g, 0.0f, 24.0f, 0.0f, "st", true, "Random grain pitch deviation.");
        r.c (O (o, osc::GrainWindow), p + "grain_window", n + "Grain Window", g, choices::grainWindows, 0, "Grain amplitude window.");
        r.f (O (o, osc::GrainSpread), p + "grain_spread", n + "Grain Spread", g, 0.0f, 1.0f, 0.5f, "%", true, "Random stereo placement of grains.");
        r.f (O (o, osc::SpecTilt), p + "spec_tilt", n + "Spectral Tilt", g, -1.0f, 1.0f, 0.0f, "%", true, "Spectral brightness tilt (negative = darker, positive = brighter).");
        r.f (O (o, osc::SpecOddEven), p + "spec_odd_even", n + "Odd/Even", g, -1.0f, 1.0f, 0.0f, "%", true, "Balance of odd vs even partials (-1 = odd only/hollow, +1 = even emphasised).");
        r.f (O (o, osc::SpecStretch), p + "spec_stretch", n + "Inharmonic Stretch", g, -1.0f, 1.0f, 0.0f, "%", true, "Stretches/compresses partial frequencies (bells, pianos, metallic).");
        r.f (O (o, osc::SpecFormant), p + "spec_formant", n + "Formant Shift", g, -24.0f, 24.0f, 0.0f, "st", true, "Shifts the spectral envelope independently of pitch.");
        r.i (O (o, osc::SpecPartials), p + "spec_partials", n + "Partials", g, 1, 64, 64, "", true, "Number of additive partials.");
        r.f (O (o, osc::SpecSpeed), p + "spec_speed", n + "Scan Speed", g, 0.0f, 4.0f, 1.0f, "x", true, "Spectral time-scan speed through the sample (0 = freeze).", 1.0f);
        r.f (O (o, osc::SampleEnd), p + "sample_end", n + "Sample End", g, 0.0f, 1.0f, 1.0f, "%", true, "End of sample playback (fraction of the sample). Sample start is the Position control.");
        r.b (O (o, osc::RootAuto), p + "root_auto", n + "Root Auto", g, false, "Use the root key and tuning stored in / detected from the sample instead of the Root Key control (default off: C4 unless set).");
    }

    // ---- Sub / Noise -------------------------------------------------------------
    r.b (SUB (sub::On), "sub_on", "Sub On", "Sub", false, "Enables the sub oscillator.");
    r.c (SUB (sub::Shape), "sub_shape", "Sub Shape", "Sub", choices::subShapes, 0, "Sub oscillator waveform.");
    r.i (SUB (sub::Octave), "sub_octave", "Sub Octave", "Sub", -2, 0, -1, "oct", false, "Sub octave relative to the played note.");
    r.f (SUB (sub::Level), "sub_level", "Sub Level", "Sub", 0.0f, 1.0f, 0.5f, "%", true, "Sub level.");
    r.f (SUB (sub::Pan), "sub_pan", "Sub Pan", "Sub", -1.0f, 1.0f, 0.0f, "%", true, "Sub stereo position.");
    r.c (SUB (sub::Route), "sub_route", "Sub Route", "Sub", choices::routes, 0, "Sub routing.");

    r.b (NOISE (noise::On), "noise_on", "Noise On", "Noise", false, "Enables the noise oscillator.");
    r.c (NOISE (noise::Type), "noise_type", "Noise Type", "Noise", choices::noiseTypes, 0, "Noise colour/type: White, Pink, Brown, Digital (pitched sample-and-hold), Crackle (vinyl-like impulses).");
    r.f (NOISE (noise::Level), "noise_level", "Noise Level", "Noise", 0.0f, 1.0f, 0.3f, "%", true, "Noise level.");
    r.f (NOISE (noise::Color), "noise_color", "Noise Color", "Noise", -1.0f, 1.0f, 0.0f, "%", true, "Spectral tilt filter (dark to bright).");
    r.f (NOISE (noise::Pan), "noise_pan", "Noise Pan", "Noise", -1.0f, 1.0f, 0.0f, "%", true, "Noise stereo position.");
    r.c (NOISE (noise::Route), "noise_route", "Noise Route", "Noise", choices::routes, 0, "Noise routing.");
    r.f (NOISE (noise::KeyTrack), "noise_keytrack", "Noise Key Track", "Noise", 0.0f, 1.0f, 0.0f, "%", true, "Pitch tracking of Digital/Crackle noise rate.");
    r.f (NOISE (noise::Width), "noise_width", "Noise Width", "Noise", 0.0f, 1.0f, 1.0f, "%", true, "Stereo decorrelation of the noise.");

    // ---- Filters ---------------------------------------------------------------------
    for (int fi = 0; fi < kNumFilters; ++fi)
    {
        const juce::String p = "flt" + juce::String (fi + 1) + "_";
        const juce::String n = "Filter " + juce::String (fi + 1) + " ";
        const juce::String g = "Filter " + juce::String (fi + 1);
        r.b (F (fi, flt::On), p + "on", n + "On", g, fi == 0, "Enables the filter.");
        r.c (F (fi, flt::Model), p + "model", n + "Model", g, choices::filterModels, fi == 0 ? 0 : 7,
             "Analog circuit model: 0 MG Ladder (Moog-style), 1 Diode Ladder, 2 Acid Diode (TB-303-style), 3 MS Sallen-Key (MS-20-style), 4 JP OTA (Jupiter-8-style), 5 JN OTA (Juno-106-style), 6 P5 Cascade (Prophet-5-style), 7 SEM State-Var (Oberheim-style), 8 Analog SVF.");
        r.c (F (fi, flt::Mode), p + "mode", n + "Mode", g, choices::filterModes, 0, "Response type. Morph = continuous LP>BP>HP sweep (SEM: LP>Notch>HP) controlled by Character.");
        r.f (F (fi, flt::Cutoff), p + "cutoff", n + "Cutoff", g, 20.0f, 20000.0f, fi == 0 ? 12000.0f : 2000.0f, "Hz", true, "Cutoff frequency.", 1000.0f);
        r.f (F (fi, flt::Resonance), p + "resonance", n + "Resonance", g, 0.0f, 1.0f, 0.15f, "%", true, "Resonance / feedback. Most models self-oscillate near 100%.");
        r.f (F (fi, flt::Drive), p + "drive", n + "Drive", g, 0.0f, 1.0f, 0.2f, "%", true, "Input drive into the nonlinear circuit (0 to +30 dB).");
        r.f (F (fi, flt::EnvAmount), p + "env_amount", n + "Env Amount", g, -1.0f, 1.0f, 0.0f, "%", true, "Cutoff modulation by ENV 2 (+/-1 = +/-8 octaves).");
        r.f (F (fi, flt::KeyTrack), p + "key_track", n + "Key Track", g, 0.0f, 1.0f, 0.5f, "%", true, "Cutoff follows the played note (100% = 1 octave per octave, centred on C4).");
        r.f (F (fi, flt::Drift), p + "drift", n + "Drift", g, 0.0f, 1.0f, 0.2f, "%", true, "Slow random cutoff drift per voice (analog instability).");
        r.f (F (fi, flt::Tolerance), p + "tolerance", n + "Tolerance", g, 0.0f, 1.0f, 0.3f, "%", false, "Per-voice component tolerance: random mismatch of stage cutoffs and feedback gain.");
        r.f (F (fi, flt::Character), p + "character", n + "Character", g, 0.0f, 1.0f, 0.5f, "%", true, "Model-specific: ladder bass compensation, 303 squelch, MS clip hardness, JN high-pass, SEM/Morph mode sweep, OTA stage saturation.");
        r.f (F (fi, flt::Mix), p + "mix", n + "Mix", g, 0.0f, 1.0f, 1.0f, "%", true, "Dry/wet mix of the filter.");
        r.f (F (fi, flt::Gain), p + "gain", n + "Gain", g, -24.0f, 24.0f, 0.0f, "dB", true, "Filter output gain.");
    }

    // ---- Envelopes ------------------------------------------------------------------
    for (int e = 0; e < kNumEnvs; ++e)
    {
        const juce::String p = "env" + juce::String (e + 1) + "_";
        const juce::String n = "ENV " + juce::String (e + 1) + " ";
        const juce::String g = "ENV " + juce::String (e + 1);
        const bool amp = e == 0;
        r.f (E (e, env::Delay), p + "delay", n + "Delay", g, 0.0f, 5.0f, 0.0f, "s", true, "Delay before the attack starts.", 0.5f);
        r.f (E (e, env::Attack), p + "attack", n + "Attack", g, 0.0f, 20.0f, amp ? 0.003f : 0.005f, "s", true, "Attack time.", 1.0f);
        r.f (E (e, env::Hold), p + "hold", n + "Hold", g, 0.0f, 5.0f, 0.0f, "s", true, "Hold time at peak.", 0.5f);
        r.f (E (e, env::Decay), p + "decay", n + "Decay", g, 0.0f, 20.0f, amp ? 1.0f : 0.5f, "s", true, "Decay time to the sustain level.", 1.0f);
        r.f (E (e, env::Sustain), p + "sustain", n + "Sustain", g, 0.0f, 1.0f, amp ? 1.0f : 0.3f, "%", true, "Sustain level.");
        r.f (E (e, env::Release), p + "release", n + "Release", g, 0.0f, 20.0f, amp ? 0.15f : 0.3f, "s", true, "Release time after note-off.", 1.0f);
        r.f (E (e, env::AttackCurve), p + "attack_curve", n + "Attack Curve", g, -1.0f, 1.0f, 0.0f, "", false, "Attack segment curvature (negative = fast start like an RC charge).");
        r.f (E (e, env::DecayCurve), p + "decay_curve", n + "Decay Curve", g, -1.0f, 1.0f, -0.6f, "", false, "Decay curvature (negative = exponential-like).");
        r.f (E (e, env::ReleaseCurve), p + "release_curve", n + "Release Curve", g, -1.0f, 1.0f, -0.6f, "", false, "Release curvature (negative = exponential-like).");
    }

    // ---- LFOs ------------------------------------------------------------------------
    for (int l = 0; l < kNumLfos; ++l)
    {
        const juce::String p = "lfo" + juce::String (l + 1) + "_";
        const juce::String n = "LFO " + juce::String (l + 1) + " ";
        const juce::String g = "LFO " + juce::String (l + 1);
        r.c (L (l, lfo::Shape), p + "shape", n + "Shape", g, choices::lfoShapes, 0, "Waveform. Custom = user-drawn curve.");
        r.f (L (l, lfo::Rate), p + "rate", n + "Rate", g, 0.01f, 40.0f, 2.0f, "Hz", true, "Free-running rate.", 2.0f);
        r.b (L (l, lfo::Sync), p + "sync", n + "Sync", g, false, "Lock the rate to host tempo.");
        r.c (L (l, lfo::SyncDiv), p + "sync_div", n + "Sync Rate", g, ParamRegistry::syncDivisionNames(), 10, "Tempo-synced cycle length.");
        r.c (L (l, lfo::Mode), p + "mode", n + "Mode", g, choices::lfoModes, 0, "Free (all voices share phase), Retrigger (restart per note), Envelope (one cycle then hold).");
        r.f (L (l, lfo::Phase), p + "phase", n + "Phase", g, 0.0f, 1.0f, 0.0f, "%", true, "Phase offset / start phase.");
        r.f (L (l, lfo::Delay), p + "delay", n + "Delay", g, 0.0f, 5.0f, 0.0f, "s", false, "Delay before the LFO starts after note-on.", 0.5f);
        r.f (L (l, lfo::Fade), p + "fade", n + "Fade In", g, 0.0f, 5.0f, 0.0f, "s", false, "Fade-in time after the delay.", 0.5f);
        r.f (L (l, lfo::Smooth), p + "smooth", n + "Smooth", g, 0.0f, 1.0f, 0.0f, "%", true, "Output smoothing (slew).");
        r.f (L (l, lfo::Depth), p + "depth", n + "Depth", g, 0.0f, 1.0f, 1.0f, "%", true, "Output depth (scales every modulation routing of this LFO).");
    }

    for (int m = 0; m < kNumMacros; ++m)
        r.f (MACRO (m), "macro" + juce::String (m + 1), "Macro " + juce::String (m + 1), "Macros", 0.0f, 1.0f, 0.0f, "%", false,
             "Macro control " + juce::String (m + 1) + " (assign in the modulation matrix).");

    for (int s = 0; s < kNumModSlots; ++s)
        r.f (MODAMT (s), "mod" + juce::String (s + 1) + "_amount", "Mod " + juce::String (s + 1) + " Amount", "Matrix",
             -1.0f, 1.0f, 0.0f, "%", false, "Depth of modulation slot " + juce::String (s + 1) + " (fraction of the destination range).");

    // ---- Effects -----------------------------------------------------------------------
    for (int t = 0; t < NumFx; ++t)
    {
        const juce::String p = "fx_" + choices::fxTypeIds[t] + "_";
        const juce::String n = choices::fxTypeNames[t] + " ";
        const juce::String g = "FX " + choices::fxTypeNames[t];
        const bool mixable = ! (t == FxEq || t == FxStereo || t == FxLimiter || t == FxMbComp);
        r.b (FX (t, fx::On), p + "on", n + "On", g, false, "Enables " + choices::fxTypeNames[t] + ".");
        r.f (FX (t, fx::Mix), p + "mix", n + "Mix", g, 0.0f, 1.0f, mixable ? (t >= FxStereoDelay && t <= FxConvolution ? 0.3f : ((t == FxChorus || t == FxFlanger || t == FxPhaser) ? 0.5f : 1.0f)) : 1.0f, "%", true, "Dry/wet mix.");
        r.c (FX (t, fx::Bus), p + "bus", n + "Bus", g, choices::fxBuses, 0, "FX bus (relevant in parallel routing).");

        switch (t)
        {
            case FxDistortion:
                r.c (FX (t, dist::Type), p + "type", n + "Type", g, choices::distTypes, 0, "Distortion algorithm.");
                r.f (FX (t, dist::Drive), p + "drive", n + "Drive", g, 0.0f, 48.0f, 12.0f, "dB", true, "Input drive.");
                r.f (FX (t, dist::Tone), p + "tone", n + "Tone", g, 200.0f, 20000.0f, 8000.0f, "Hz", true, "Post-distortion low-pass.", 2000.0f);
                r.f (FX (t, dist::Output), p + "output", n + "Output", g, -24.0f, 12.0f, -6.0f, "dB", true, "Output level.");
                break;
            case FxTube:
                r.f (FX (t, tube::Drive), p + "drive", n + "Drive", g, 0.0f, 36.0f, 9.0f, "dB", true, "Tube stage drive.");
                r.f (FX (t, tube::Bias), p + "bias", n + "Bias", g, 0.0f, 1.0f, 0.3f, "%", true, "Asymmetry (even harmonics).");
                r.f (FX (t, tube::Tone), p + "tone", n + "Tone", g, -1.0f, 1.0f, 0.0f, "%", true, "Tilt EQ after the tube stage.");
                r.f (FX (t, tube::Output), p + "output", n + "Output", g, -24.0f, 12.0f, -3.0f, "dB", true, "Output level.");
                break;
            case FxTape:
                r.f (FX (t, tape::Drive), p + "drive", n + "Drive", g, 0.0f, 24.0f, 6.0f, "dB", true, "Tape input level.");
                r.f (FX (t, tape::Bump), p + "bump", n + "Head Bump", g, 0.0f, 1.0f, 0.4f, "%", true, "Low-frequency head-bump resonance.");
                r.f (FX (t, tape::Tone), p + "tone", n + "HF Loss", g, 0.0f, 1.0f, 0.3f, "%", true, "High-frequency roll-off.");
                r.f (FX (t, tape::Output), p + "output", n + "Output", g, -24.0f, 12.0f, -2.0f, "dB", true, "Output level.");
                break;
            case FxEq:
                r.f (FX (t, eq::LowCut), p + "low_cut", n + "Low Cut", g, 20.0f, 1000.0f, 20.0f, "Hz", true, "High-pass filter (20 Hz = off).", 100.0f);
                r.f (FX (t, eq::LowFreq), p + "low_freq", n + "Low Shelf Freq", g, 30.0f, 1000.0f, 120.0f, "Hz", true, "Low shelf frequency.", 150.0f);
                r.f (FX (t, eq::LowGain), p + "low_gain", n + "Low Shelf Gain", g, -18.0f, 18.0f, 0.0f, "dB", true, "Low shelf gain.");
                r.f (FX (t, eq::Mid1Freq), p + "mid1_freq", n + "Mid 1 Freq", g, 60.0f, 12000.0f, 500.0f, "Hz", true, "Peak 1 frequency.", 800.0f);
                r.f (FX (t, eq::Mid1Gain), p + "mid1_gain", n + "Mid 1 Gain", g, -18.0f, 18.0f, 0.0f, "dB", true, "Peak 1 gain.");
                r.f (FX (t, eq::Mid1Q), p + "mid1_q", n + "Mid 1 Q", g, 0.2f, 12.0f, 0.8f, "", true, "Peak 1 bandwidth (Q).", 1.0f);
                r.f (FX (t, eq::Mid2Freq), p + "mid2_freq", n + "Mid 2 Freq", g, 200.0f, 16000.0f, 3000.0f, "Hz", true, "Peak 2 frequency.", 2500.0f);
                r.f (FX (t, eq::Mid2Gain), p + "mid2_gain", n + "Mid 2 Gain", g, -18.0f, 18.0f, 0.0f, "dB", true, "Peak 2 gain.");
                r.f (FX (t, eq::Mid2Q), p + "mid2_q", n + "Mid 2 Q", g, 0.2f, 12.0f, 0.8f, "", true, "Peak 2 bandwidth (Q).", 1.0f);
                r.f (FX (t, eq::HighFreq), p + "high_freq", n + "High Shelf Freq", g, 1000.0f, 18000.0f, 8000.0f, "Hz", true, "High shelf frequency.", 5000.0f);
                r.f (FX (t, eq::HighGain), p + "high_gain", n + "High Shelf Gain", g, -18.0f, 18.0f, 0.0f, "dB", true, "High shelf gain.");
                r.f (FX (t, eq::HighCut), p + "high_cut", n + "High Cut", g, 1000.0f, 20000.0f, 20000.0f, "Hz", true, "Low-pass filter (20 kHz = off).", 6000.0f);
                break;
            case FxMbComp:
                r.f (FX (t, mbc::LowXover), p + "low_xover", n + "Low X-Over", g, 40.0f, 1000.0f, 200.0f, "Hz", false, "Low/mid crossover.", 200.0f);
                r.f (FX (t, mbc::HighXover), p + "high_xover", n + "High X-Over", g, 1000.0f, 12000.0f, 3000.0f, "Hz", false, "Mid/high crossover.", 3000.0f);
                r.f (FX (t, mbc::LowThresh), p + "low_thresh", n + "Low Threshold", g, -60.0f, 0.0f, -18.0f, "dB", true, "Low band threshold.");
                r.f (FX (t, mbc::MidThresh), p + "mid_thresh", n + "Mid Threshold", g, -60.0f, 0.0f, -18.0f, "dB", true, "Mid band threshold.");
                r.f (FX (t, mbc::HighThresh), p + "high_thresh", n + "High Threshold", g, -60.0f, 0.0f, -18.0f, "dB", true, "High band threshold.");
                r.f (FX (t, mbc::Ratio), p + "ratio", n + "Ratio", g, 1.0f, 20.0f, 4.0f, "x", true, "Compression ratio.", 4.0f);
                r.f (FX (t, mbc::Attack), p + "attack", n + "Attack", g, 0.1f, 100.0f, 5.0f, "ms", false, "Attack time.", 10.0f);
                r.f (FX (t, mbc::Release), p + "release", n + "Release", g, 10.0f, 1000.0f, 120.0f, "ms", false, "Release time.", 150.0f);
                r.f (FX (t, mbc::LowGain), p + "low_gain", n + "Low Gain", g, -12.0f, 18.0f, 3.0f, "dB", true, "Low band make-up gain.");
                r.f (FX (t, mbc::MidGain), p + "mid_gain", n + "Mid Gain", g, -12.0f, 18.0f, 3.0f, "dB", true, "Mid band make-up gain.");
                r.f (FX (t, mbc::HighGain), p + "high_gain", n + "High Gain", g, -12.0f, 18.0f, 3.0f, "dB", true, "High band make-up gain.");
                r.f (FX (t, mbc::Output), p + "output", n + "Output", g, -24.0f, 12.0f, 0.0f, "dB", true, "Output level.");
                break;
            case FxFreqShift:
                r.f (FX (t, fshift::Shift), p + "shift", n + "Shift", g, -2000.0f, 2000.0f, 50.0f, "Hz", true, "Frequency shift (not pitch shift): inharmonic, metallic.");
                r.f (FX (t, fshift::Stereo), p + "stereo", n + "Stereo", g, 0.0f, 1.0f, 0.0f, "%", true, "Right channel shifts in the opposite direction.");
                r.f (FX (t, fshift::Feedback), p + "feedback", n + "Feedback", g, 0.0f, 0.95f, 0.0f, "%", true, "Feedback (barber-pole sweeps).");
                break;
            case FxRingMod:
                r.f (FX (t, ring::Freq), p + "freq", n + "Frequency", g, 1.0f, 5000.0f, 220.0f, "Hz", true, "Carrier frequency.", 300.0f);
                r.c (FX (t, ring::Shape), p + "shape", n + "Shape", g, choices::ringShapes, 0, "Carrier waveform.");
                r.f (FX (t, ring::Spread), p + "spread", n + "Spread", g, 0.0f, 1.0f, 0.0f, "%", true, "Stereo carrier detune.");
                break;
            case FxChorus:
                r.f (FX (t, chorus::Rate), p + "rate", n + "Rate", g, 0.05f, 10.0f, 0.6f, "Hz", true, "Modulation rate.", 1.0f);
                r.f (FX (t, chorus::Depth), p + "depth", n + "Depth", g, 0.0f, 1.0f, 0.4f, "%", true, "Modulation depth.");
                r.f (FX (t, chorus::Delay), p + "delay", n + "Delay", g, 1.0f, 40.0f, 8.0f, "ms", true, "Base delay.", 10.0f);
                r.f (FX (t, chorus::Feedback), p + "feedback", n + "Feedback", g, 0.0f, 0.9f, 0.1f, "%", true, "Feedback.");
                r.f (FX (t, chorus::Tone), p + "tone", n + "Tone", g, 1000.0f, 20000.0f, 9000.0f, "Hz", true, "BBD-style wet low-pass.", 6000.0f);
                r.f (FX (t, chorus::Width), p + "width", n + "Width", g, 0.0f, 1.0f, 1.0f, "%", true, "Stereo width.");
                break;
            case FxDimension:
                r.c (FX (t, dimension::Mode), p + "mode", n + "Mode", g, choices::dimensionModes, 1, "Dimension-style preset depth/rate (1 subtle ... 4 deep).");
                r.f (FX (t, dimension::Width), p + "width", n + "Width", g, 0.0f, 1.0f, 1.0f, "%", true, "Stereo width.");
                break;
            case FxFlanger:
                r.f (FX (t, flanger::Rate), p + "rate", n + "Rate", g, 0.01f, 10.0f, 0.25f, "Hz", true, "Sweep rate.", 0.5f);
                r.f (FX (t, flanger::Depth), p + "depth", n + "Depth", g, 0.0f, 1.0f, 0.6f, "%", true, "Sweep depth.");
                r.f (FX (t, flanger::Delay), p + "delay", n + "Delay", g, 0.1f, 10.0f, 1.5f, "ms", true, "Base delay.", 2.0f);
                r.f (FX (t, flanger::Feedback), p + "feedback", n + "Feedback", g, -0.95f, 0.95f, 0.5f, "%", true, "Feedback (negative = hollow).");
                r.f (FX (t, flanger::Stereo), p + "stereo", n + "Stereo", g, 0.0f, 1.0f, 0.5f, "%", true, "L/R sweep phase offset.");
                break;
            case FxPhaser:
                r.f (FX (t, phaser::Rate), p + "rate", n + "Rate", g, 0.01f, 10.0f, 0.3f, "Hz", true, "Sweep rate.", 0.5f);
                r.f (FX (t, phaser::Depth), p + "depth", n + "Depth", g, 0.0f, 1.0f, 0.7f, "%", true, "Sweep depth.");
                r.f (FX (t, phaser::Feedback), p + "feedback", n + "Feedback", g, -0.95f, 0.95f, 0.4f, "%", true, "Feedback.");
                r.i (FX (t, phaser::Stages), p + "stages", n + "Stages", g, 2, 12, 6, "", false, "Number of all-pass stages.");
                r.f (FX (t, phaser::Center), p + "center", n + "Center", g, 100.0f, 8000.0f, 800.0f, "Hz", true, "Sweep centre frequency.", 800.0f);
                r.f (FX (t, phaser::Stereo), p + "stereo", n + "Stereo", g, 0.0f, 1.0f, 0.5f, "%", true, "L/R sweep phase offset.");
                break;
            case FxStereoDelay:
                r.f (FX (t, sdelay::TimeL), p + "time_l", n + "Time L", g, 1.0f, 2000.0f, 375.0f, "ms", true, "Left delay time (free mode).", 300.0f);
                r.f (FX (t, sdelay::TimeR), p + "time_r", n + "Time R", g, 1.0f, 2000.0f, 500.0f, "ms", true, "Right delay time (free mode).", 300.0f);
                r.b (FX (t, sdelay::Sync), p + "sync", n + "Sync", g, true, "Tempo sync.");
                r.c (FX (t, sdelay::DivL), p + "div_l", n + "Div L", g, ParamRegistry::syncDivisionNames(), 8, "Left synced time.");
                r.c (FX (t, sdelay::DivR), p + "div_r", n + "Div R", g, ParamRegistry::syncDivisionNames(), 10, "Right synced time.");
                r.f (FX (t, sdelay::Feedback), p + "feedback", n + "Feedback", g, 0.0f, 0.98f, 0.4f, "%", true, "Feedback.");
                r.b (FX (t, sdelay::PingPong), p + "pingpong", n + "Ping-Pong", g, false, "Cross-feed L/R.");
                r.f (FX (t, sdelay::LowCut), p + "low_cut", n + "Low Cut", g, 20.0f, 2000.0f, 150.0f, "Hz", true, "Feedback high-pass.", 200.0f);
                r.f (FX (t, sdelay::HighCut), p + "high_cut", n + "High Cut", g, 1000.0f, 20000.0f, 8000.0f, "Hz", true, "Feedback low-pass.", 5000.0f);
                break;
            case FxTapeDelay:
                r.f (FX (t, tdelay::Time), p + "time", n + "Time", g, 20.0f, 2000.0f, 400.0f, "ms", true, "Delay time (free mode).", 400.0f);
                r.b (FX (t, tdelay::Sync), p + "sync", n + "Sync", g, true, "Tempo sync.");
                r.c (FX (t, tdelay::Div), p + "div", n + "Div", g, ParamRegistry::syncDivisionNames(), 8, "Synced time.");
                r.f (FX (t, tdelay::Feedback), p + "feedback", n + "Feedback", g, 0.0f, 1.05f, 0.45f, "%", true, "Feedback (>100% runs away into saturation).");
                r.f (FX (t, tdelay::Wow), p + "wow", n + "Wow", g, 0.0f, 1.0f, 0.3f, "%", true, "Slow pitch wobble.");
                r.f (FX (t, tdelay::Flutter), p + "flutter", n + "Flutter", g, 0.0f, 1.0f, 0.2f, "%", true, "Fast pitch wobble.");
                r.f (FX (t, tdelay::Saturation), p + "saturation", n + "Saturation", g, 0.0f, 1.0f, 0.4f, "%", true, "Tape saturation in the loop.");
                r.f (FX (t, tdelay::Tone), p + "tone", n + "Tone", g, 500.0f, 12000.0f, 3500.0f, "Hz", true, "Loop low-pass (tape darkening).", 3000.0f);
                break;
            case FxDigitalDelay:
                r.f (FX (t, ddelay::Time), p + "time", n + "Time", g, 1.0f, 2000.0f, 250.0f, "ms", true, "Delay time (free mode).", 300.0f);
                r.b (FX (t, ddelay::Sync), p + "sync", n + "Sync", g, true, "Tempo sync.");
                r.c (FX (t, ddelay::Div), p + "div", n + "Div", g, ParamRegistry::syncDivisionNames(), 7, "Synced time.");
                r.f (FX (t, ddelay::Feedback), p + "feedback", n + "Feedback", g, 0.0f, 0.98f, 0.35f, "%", true, "Feedback.");
                r.f (FX (t, ddelay::Crush), p + "crush", n + "Crush", g, 0.0f, 1.0f, 0.0f, "%", true, "Bit and sample-rate reduction of the repeats.");
                r.f (FX (t, ddelay::Spread), p + "spread", n + "Spread", g, 0.0f, 1.0f, 0.3f, "%", true, "Right channel time offset.");
                break;
            case FxReverb:
                r.f (FX (t, reverb::Size), p + "size", n + "Size", g, 0.0f, 1.0f, 0.6f, "%", true, "Room size.");
                r.f (FX (t, reverb::Decay), p + "decay", n + "Decay", g, 0.1f, 30.0f, 3.0f, "s", true, "RT60 decay time.", 3.0f);
                r.f (FX (t, reverb::Predelay), p + "predelay", n + "Pre-delay", g, 0.0f, 250.0f, 20.0f, "ms", true, "Pre-delay.", 50.0f);
                r.f (FX (t, reverb::Damping), p + "damping", n + "Damping", g, 0.0f, 1.0f, 0.4f, "%", true, "High-frequency damping.");
                r.f (FX (t, reverb::Diffusion), p + "diffusion", n + "Diffusion", g, 0.0f, 1.0f, 0.75f, "%", true, "Input diffusion.");
                r.f (FX (t, reverb::Modulation), p + "modulation", n + "Modulation", g, 0.0f, 1.0f, 0.3f, "%", true, "Delay-line modulation (chorused tail).");
                r.f (FX (t, reverb::Width), p + "width", n + "Width", g, 0.0f, 1.0f, 1.0f, "%", true, "Stereo width.");
                r.f (FX (t, reverb::LowCut), p + "low_cut", n + "Low Cut", g, 20.0f, 1000.0f, 120.0f, "Hz", true, "Wet high-pass.", 150.0f);
                break;
            case FxConvolution:
                r.c (FX (t, conv::Impulse), p + "impulse", n + "Impulse", g, choices::convImpulses, 0, "Built-in synthetic impulse responses or a user-loaded IR file.");
                r.f (FX (t, conv::Predelay), p + "predelay", n + "Pre-delay", g, 0.0f, 250.0f, 10.0f, "ms", false, "Pre-delay.", 50.0f);
                r.f (FX (t, conv::LowCut), p + "low_cut", n + "Low Cut", g, 20.0f, 1000.0f, 100.0f, "Hz", true, "Wet high-pass.", 150.0f);
                r.f (FX (t, conv::HighCut), p + "high_cut", n + "High Cut", g, 1000.0f, 20000.0f, 12000.0f, "Hz", true, "Wet low-pass.", 6000.0f);
                r.f (FX (t, conv::Length), p + "length", n + "Length", g, 0.05f, 1.0f, 1.0f, "%", false, "Impulse length (fraction, with fade-out).");
                break;
            case FxStereo:
                r.f (FX (t, stereo::Width), p + "width", n + "Width", g, 0.0f, 2.0f, 1.3f, "x", true, "Mid/side width (1 = unchanged, 0 = mono).", 1.0f);
                r.f (FX (t, stereo::Haas), p + "haas", n + "Haas", g, 0.0f, 30.0f, 0.0f, "ms", true, "Right-channel micro delay.");
                r.f (FX (t, stereo::BassMono), p + "bass_mono", n + "Bass Mono", g, 20.0f, 500.0f, 20.0f, "Hz", true, "Below this frequency the signal is mono (20 Hz = off).", 120.0f);
                break;
            case FxLimiter:
                r.f (FX (t, limiter::Input), p + "input", n + "Input", g, -12.0f, 24.0f, 0.0f, "dB", true, "Input gain.");
                r.f (FX (t, limiter::Ceiling), p + "ceiling", n + "Ceiling", g, -24.0f, 0.0f, -0.3f, "dB", false, "Output ceiling.");
                r.f (FX (t, limiter::Release), p + "release", n + "Release", g, 5.0f, 1000.0f, 80.0f, "ms", false, "Release time.", 100.0f);
                break;
            default: jassertfalse; break;
        }
    }

    // ---- Arp / Seq -------------------------------------------------------------------
    r.c (ARP (arp::Mode), "arp_mode", "Arp/Seq Mode", "Arp", choices::arpModes, 0, "Off, Arpeggiator or step Sequencer.");
    r.c (ARP (arp::Pattern), "arp_pattern", "Arp Pattern", "Arp", choices::arpPatterns, 0, "Note order.");
    r.i (ARP (arp::Octaves), "arp_octaves", "Arp Octaves", "Arp", 1, 4, 1, "", false, "Octave range.");
    r.c (ARP (arp::Rate), "arp_rate", "Arp Rate", "Arp", ParamRegistry::syncDivisionNames(), 4, "Step length (tempo-synced).");
    r.f (ARP (arp::Gate), "arp_gate", "Arp Gate", "Arp", 0.05f, 1.0f, 0.6f, "%", false, "Note length as a fraction of a step.");
    r.f (ARP (arp::Swing), "arp_swing", "Arp Swing", "Arp", 0.0f, 0.75f, 0.0f, "%", false, "Delays every second step.");
    r.b (ARP (arp::Latch), "arp_latch", "Arp Latch", "Arp", false, "Hold notes after keys are released.");
    r.c (ARP (arp::OctaveMode), "arp_octave_mode", "Arp Octave Mode", "Arp", choices::arpOctaveModes, 0, "How octaves are traversed.");
    r.c (ARP (arp::VelMode), "arp_vel_mode", "Arp Velocity", "Arp", choices::arpVelModes, 0, "Velocity source for arp notes.");
    r.f (ARP (arp::Probability), "arp_probability", "Arp Probability", "Arp", 0.0f, 1.0f, 1.0f, "%", false, "Probability that a step plays.");

    r.c (SEQ (seq::Rate), "seq_rate", "Seq Rate", "Seq", ParamRegistry::syncDivisionNames(), 4, "Step length (tempo-synced).");
    r.i (SEQ (seq::Length), "seq_length", "Seq Length", "Seq", 1, 32, 16, "", false, "Number of steps.");
    r.f (SEQ (seq::Swing), "seq_swing", "Seq Swing", "Seq", 0.0f, 0.75f, 0.0f, "%", false, "Delays every second step.");
    r.f (SEQ (seq::Gate), "seq_gate", "Seq Gate", "Seq", 0.05f, 2.0f, 1.0f, "x", false, "Global gate-length multiplier.");
    r.b (SEQ (seq::Follow), "seq_follow", "Seq Key Follow", "Seq", true, "Transpose the pattern by the held key (relative to C4).");
    r.f (SEQ (seq::VelScale), "seq_vel_scale", "Seq Velocity", "Seq", 0.0f, 1.0f, 1.0f, "%", false, "Scales step velocities.");

    jassert ((int) defs.size() == kNumParams);

    for (auto& d : defs)
    {
        lookup[d.id.toStdString()] = d.index;
        if (d.modulatable)
            modulatable.push_back (d.index);
    }
}

const ParamRegistry& ParamRegistry::get()
{
    static const ParamRegistry instance;
    return instance;
}

int ParamRegistry::layoutErrors() { (void) get(); return registryLayoutErrors(); }

int ParamRegistry::indexOf (const juce::String& id) const
{
    auto it = lookup.find (id.trim().toLowerCase().toStdString());
    return it == lookup.end() ? -1 : it->second;
}

juce::AudioProcessorValueTreeState::ParameterLayout ParamRegistry::createLayout() const
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& d : defs)
    {
        const juce::ParameterID pid { d.id, 1 };
        const ParamDef* defPtr = &d;
        auto toText = [defPtr] (float v, int maxLen) { return ParamRegistry::formatValue (*defPtr, v, maxLen); };

        switch (d.type)
        {
            case ParamType::Float:
            {
                auto fromText = [defPtr] (const juce::String& t)
                {
                    auto v = t.retainCharacters ("0123456789.-+").getFloatValue();
                    if (defPtr->unit == "%") v *= 0.01f;
                    if (defPtr->unit == "Hz" && t.containsIgnoreCase ("k")) v *= 1000.0f;
                    if (defPtr->unit == "s" && t.containsIgnoreCase ("ms")) v *= 0.001f;
                    return v;
                };
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, d.name, d.range, d.defaultValue,
                    juce::AudioParameterFloatAttributes().withLabel (d.unit == "%" ? juce::String() : d.unit)
                        .withStringFromValueFunction (toText)
                        .withValueFromStringFunction (fromText)
                        .withAutomatable (d.automatable)));
                break;
            }
            case ParamType::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (
                    pid, d.name, (int) d.minValue, (int) d.maxValue, (int) d.defaultValue,
                    juce::AudioParameterIntAttributes().withStringFromValueFunction (
                        [defPtr] (int v, int len) { return ParamRegistry::formatValue (*defPtr, (float) v, len); })
                        .withAutomatable (d.automatable)));
                break;
            case ParamType::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    pid, d.name, d.defaultValue > 0.5f,
                    juce::AudioParameterBoolAttributes().withAutomatable (d.automatable)));
                break;
            case ParamType::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    pid, d.name, d.choices, (int) d.defaultValue,
                    juce::AudioParameterChoiceAttributes().withAutomatable (d.automatable)));
                break;
        }
    }
    return layout;
}

} // namespace vsx
