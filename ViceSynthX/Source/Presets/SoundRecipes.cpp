#include "SoundRecipes.h"

namespace vsx::Recipes
{
juce::String oscId (int o) { return "osc_" + juce::String::charToString ((juce::juce_wchar) ('a' + o)); }
juce::String fxId (int type) { return "fx_" + choices::fxTypeIds[type]; }
static juce::String fl (int f) { return "flt" + juce::String (f + 1); }
static juce::String en (int e) { return "env" + juce::String (e + 1); }
static juce::String lf (int l) { return "lfo" + juce::String (l + 1); }

PatchSpec init (const juce::String& name, const juce::String& category)
{
    PatchSpec s;
    s.meta.name = name;
    s.meta.category = category;
    s.meta.author = "VICE Factory";
    s.meta.source = "factory";
    // init sound: one analog saw through an open ladder filter
    analog (s, 0, 0.67f, 0.75f);
    filter (s, 0, MG, LP24, 12000.0f, 0.1f, 0.15f, 0.0f, 0.5f, 0.5f);
    return s;
}

void osc (PatchSpec& s, int o, int mode, float level, float position)
{
    const auto id = oscId (o);
    s.p (id + "_on", 1.0f).p (id + "_mode", (float) mode).p (id + "_level", level).p (id + "_position", position);
}

void wavetable (PatchSpec& s, int o, const juce::String& name, float position, float level)
{
    osc (s, o, WT, level, position);
    s.wavetableId[(size_t) o] = "factory:" + name;
}

void analog (PatchSpec& s, int o, float shape, float level, float pw)
{
    osc (s, o, VA, level, shape);
    s.p (oscId (o) + "_pulse_width", pw);
}

void sample (PatchSpec& s, int o, const juce::String& name, float level, int root, int loopMode)
{
    osc (s, o, SAMPLE, level, 0.0f);
    s.sampleId[(size_t) o] = "builtin:" + name;
    s.p (oscId (o) + "_root_note", (float) root).p (oscId (o) + "_loop_mode", (float) loopMode).p (oscId (o) + "_source", 1.0f);
}

void multisample (PatchSpec& s, int o, const juce::String& name, float level)
{
    osc (s, o, MULTI, level, 0.0f);
    s.multiId[(size_t) o] = "builtin:" + name;
}

void granular (PatchSpec& s, int o, const juce::String& smp, float pos, float size, float density, float spray, float level)
{
    osc (s, o, GRAIN, level, pos);
    s.sampleId[(size_t) o] = "builtin:" + smp;
    const auto id = oscId (o);
    s.p (id + "_source", 1.0f).p (id + "_grain_size", size).p (id + "_grain_density", density).p (id + "_grain_spray", spray)
     .p (id + "_grain_spread", 0.7f).p (id + "_root_note", 60.0f);
}

void spectral (PatchSpec& s, int o, const juce::String& src, bool fromSample, float level)
{
    osc (s, o, SPECTRAL, level, 0.0f);
    if (fromSample)
    {
        s.sampleId[(size_t) o] = "builtin:" + src;
        s.p (oscId (o) + "_source", 1.0f).p (oscId (o) + "_loop_mode", 1.0f);
    }
    else
    {
        s.wavetableId[(size_t) o] = "factory:" + src;
        s.p (oscId (o) + "_source", 0.0f);
    }
}

void pitch (PatchSpec& s, int o, int octave, int semi, float fine)
{
    s.p (oscId (o) + "_octave", (float) octave).p (oscId (o) + "_semi", (float) semi).p (oscId (o) + "_fine", fine);
}

void unison (PatchSpec& s, int o, int voices, float detune, float spread, float blend)
{
    s.p (oscId (o) + "_unison", (float) voices).p (oscId (o) + "_detune", detune).p (oscId (o) + "_spread", spread).p (oscId (o) + "_blend", blend);
}

void warp (PatchSpec& s, int o, int mode, float amount) { s.p (oscId (o) + "_warp_mode", (float) mode).p (oscId (o) + "_warp_amount", amount); }
void xmod (PatchSpec& s, int o, int mode, int source, float amount)
{
    s.p (oscId (o) + "_xmod_mode", (float) mode).p (oscId (o) + "_xmod_source", (float) source).p (oscId (o) + "_xmod_amount", amount);
}
void route (PatchSpec& s, int o, int r) { s.p (oscId (o) + "_route", (float) r); }

void sub (PatchSpec& s, float level, int octave, int shape)
{
    s.p ("sub_on", 1.0f).p ("sub_level", level).p ("sub_octave", (float) octave).p ("sub_shape", (float) shape);
}

void noise (PatchSpec& s, float level, int type, float color)
{
    s.p ("noise_on", 1.0f).p ("noise_level", level).p ("noise_type", (float) type).p ("noise_color", color);
}

void filter (PatchSpec& s, int f, int model, int mode, float cutoff, float reso, float drive, float envAmt, float keyTrack, float character)
{
    const auto id = fl (f);
    s.p (id + "_on", 1.0f).p (id + "_model", (float) model).p (id + "_mode", (float) mode).p (id + "_cutoff", cutoff)
     .p (id + "_resonance", reso).p (id + "_drive", drive).p (id + "_env_amount", envAmt).p (id + "_key_track", keyTrack)
     .p (id + "_character", character);
}

void filterOff (PatchSpec& s, int f) { s.p (fl (f) + "_on", 0.0f); }

void env (PatchSpec& s, int e, float a, float d, float sus, float r, float hold)
{
    const auto id = en (e);
    s.p (id + "_attack", a).p (id + "_decay", d).p (id + "_sustain", sus).p (id + "_release", r).p (id + "_hold", hold);
}

void lfo (PatchSpec& s, int l, int shape, float rate, bool sync, int div, int mode)
{
    const auto id = lf (l);
    s.p (id + "_shape", (float) shape).p (id + "_rate", rate).p (id + "_sync", sync ? 1.0f : 0.0f).p (id + "_sync_div", (float) div)
     .p (id + "_mode", (float) mode);
}

void voice (PatchSpec& s, int mode, float glide, int polyphony)
{
    s.p ("voice_mode", (float) mode).p ("glide_time", glide).p ("polyphony", (float) polyphony);
}

void fx (PatchSpec& s, int type, float mix, std::initializer_list<std::pair<const char*, float>> params)
{
    const auto id = fxId (type);
    s.p (id + "_on", 1.0f).p (id + "_mix", mix);
    for (const auto& [k, v] : params)
        s.set (id + "_" + k, v);
}

void fxOff (PatchSpec& s, int type) { s.p (fxId (type) + "_on", 0.0f); }

void vibrato (PatchSpec& s, int l, float rate, float depth)
{
    lfo (s, l, 0, rate, false, 10, 0);
    s.p (lf (l) + "_delay", 0.25f).p (lf (l) + "_fade", 0.6f);
    for (int o = 0; o < kNumOscs; ++o)
        if (s.get (oscId (o) + "_on") > 0.5f)
            s.addMod (modsrc::Lfo1 + l, oscId (o) + "_fine", depth, 0, modsrc::ModWheel);
}

void filterLfo (PatchSpec& s, int l, int f, float rate, float depth, bool sync, int div)
{
    lfo (s, l, 0, rate, sync, div, 0);
    s.addMod (modsrc::Lfo1 + l, fl (f) + "_cutoff", depth);
}

void pitchEnv (PatchSpec& s, int e, int oscMask, float semis, float decay)
{
    env (s, e, 0.0f, decay, 0.0f, decay);
    s.p (en (e) + "_decay_curve", -0.8f);
    for (int o = 0; o < kNumOscs; ++o)
        if ((oscMask >> o) & 1)
            s.addMod (modsrc::Env1 + e, oscId (o) + "_octave", juce::jlimit (-1.0f, 1.0f, semis / 96.0f));
}

void velocityToCutoff (PatchSpec& s, int f, float amount) { s.addMod (modsrc::Velocity, fl (f) + "_cutoff", amount); }

void macros (PatchSpec& s)
{
    s.addMod (modsrc::Macro1, "flt1_cutoff", 0.35f);
    if (s.get ("osc_a_mode") < 0.5f || s.get ("osc_a_mode") > 4.5f)
        s.addMod (modsrc::Macro2, "osc_a_position", 0.5f);
    else
        s.addMod (modsrc::Macro2, "lfo1_rate", 0.4f);
    s.addMod (modsrc::Macro3, "fx_reverb_mix", 0.5f);
    s.addMod (modsrc::Macro4, "flt1_drive", 0.6f);
}

juce::String archetypeName (Archetype a)
{
    switch (a)
    {
        case Archetype::Bass: return "Bass";
        case Archetype::Pad: return "Pad";
        case Archetype::Lead: return "Lead";
        case Archetype::Pluck: return "Pluck";
        case Archetype::Keys: return "Keys";
        case Archetype::Organ: return "Organ";
        case Archetype::Strings: return "Strings";
        case Archetype::Choir: return "Choir";
        case Archetype::Brass: return "Brass";
        case Archetype::Bell: return "Bell";
        case Archetype::Texture: return "Texture";
        case Archetype::Drone: return "Drone";
        case Archetype::Sfx: return "Sound Effect";
        case Archetype::Sequence: return "Sequence";
        case Archetype::Arp: return "Arpeggio";
    }
    return {};
}

static void reverb (PatchSpec& s, float mix, float size, float decay, float damping = 0.4f)
{
    fx (s, pid::FxReverb, mix, { { "size", size }, { "decay", decay }, { "damping", damping } });
}

void archetype (PatchSpec& s, Archetype a, int v)
{
    using pid::FxDistortion; using pid::FxTube; using pid::FxTape; using pid::FxEq; using pid::FxMbComp; using pid::FxFreqShift;
using pid::FxRingMod; using pid::FxChorus; using pid::FxDimension; using pid::FxFlanger; using pid::FxPhaser; using pid::FxStereoDelay;
using pid::FxTapeDelay; using pid::FxDigitalDelay; using pid::FxReverb; using pid::FxConvolution; using pid::FxStereo; using pid::FxLimiter;
    switch (a)
    {
        case Archetype::Bass:
            voice (s, 2, 0.04f, 1);
            env (s, 0, 0.002f, 0.6f, 0.85f, 0.12f);
            switch (v % 6)
            {
                case 0: // analog ladder
                    analog (s, 0, 0.67f, 0.8f);
                    analog (s, 1, 1.0f, 0.5f); pitch (s, 1, -1);
                    sub (s, 0.45f, -1, 0);
                    filter (s, 0, MG, LP24, 380.0f, 0.35f, 0.45f, 0.42f, 0.4f, 0.3f);
                    env (s, 1, 0.002f, 0.35f, 0.2f, 0.2f);
                    break;
                case 1: // acid
                    analog (s, 0, 0.67f, 0.85f);
                    filter (s, 0, ACID, LP24, 280.0f, 0.8f, 0.35f, 0.55f, 0.3f, 0.6f);
                    env (s, 1, 0.001f, 0.25f, 0.0f, 0.2f);
                    env (s, 0, 0.001f, 0.35f, 0.6f, 0.08f);
                    velocityToCutoff (s, 0, 0.15f);
                    fx (s, FxDistortion, 0.5f, { { "drive", 9.0f }, { "tone", 6000.0f }, { "output", -6.0f } });
                    break;
                case 2: // reese
                    wavetable (s, 0, "Saw Stack", 0.3f, 0.75f); unison (s, 0, 4, 0.35f, 0.4f, 0.8f);
                    wavetable (s, 1, "Analog Drift Saw", 0.5f, 0.55f); pitch (s, 1, 0, 0, 14.0f);
                    sub (s, 0.5f, -1, 0);
                    filter (s, 0, MG, LP24, 700.0f, 0.2f, 0.5f, 0.1f, 0.3f, 0.5f);
                    filterLfo (s, 0, 0, 0.18f, 0.12f);
                    break;
                case 3: // FM digital
                    wavetable (s, 0, "Digital FM", 0.2f, 0.8f);
                    env (s, 2, 0.0f, 0.4f, 0.1f, 0.3f);
                    s.addMod (modsrc::Env3, "osc_a_position", 0.5f);
                    sub (s, 0.4f, -1, 0);
                    filter (s, 0, JP, LP12, 2500.0f, 0.15f, 0.2f, 0.2f, 0.5f, 0.5f);
                    break;
                case 4: // sub
                    analog (s, 0, 0.0f, 0.9f);
                    analog (s, 1, 0.33f, 0.3f);
                    filterOff (s, 0);
                    fx (s, FxTube, 1.0f, { { "drive", 8.0f }, { "bias", 0.3f } });
                    break;
                default: // wobble
                    wavetable (s, 0, "Growl", 0.3f, 0.8f); unison (s, 0, 2, 0.15f);
                    sub (s, 0.5f, -1, 0);
                    filter (s, 0, MS, LP12, 600.0f, 0.5f, 0.5f, 0.0f, 0.3f, 0.6f);
                    lfo (s, 0, 0, 2.0f, true, 7, 1);
                    s.addMod (modsrc::Lfo1, "flt1_cutoff", 0.45f, 2);
                    s.addMod (modsrc::Lfo1, "osc_a_position", 0.4f, 2);
                    fx (s, FxDistortion, 0.6f, { { "drive", 14.0f }, { "output", -8.0f } });
                    break;
            }
            break;

        case Archetype::Pad:
            voice (s, 0, 0.0f, 16);
            env (s, 0, 0.8f, 1.5f, 0.85f, 1.8f);
            env (s, 1, 1.2f, 2.0f, 0.6f, 2.0f);
            switch (v % 6)
            {
                case 0:
                    analog (s, 0, 0.67f, 0.7f); unison (s, 0, 6, 0.25f, 0.85f);
                    analog (s, 1, 0.67f, 0.45f); pitch (s, 1, 1, 0, -7.0f); unison (s, 1, 3, 0.2f, 0.9f);
                    filter (s, 0, JN, LP24, 2200.0f, 0.15f, 0.2f, 0.12f, 0.4f, 0.33f);
                    filterLfo (s, 0, 0, 0.25f, 0.06f);
                    fx (s, FxDimension, 1.0f, { { "mode", 1.0f } });
                    reverb (s, 0.35f, 0.7f, 4.0f);
                    break;
                case 1:
                    wavetable (s, 0, "Harmonic Sweep", 0.5f, 0.65f); unison (s, 0, 4, 0.2f, 0.9f);
                    wavetable (s, 1, "Glass", 0.4f, 0.35f); pitch (s, 1, 1);
                    filter (s, 0, SVF, LP12, 6000.0f, 0.15f, 0.1f, 0.05f, 0.4f, 0.5f);
                    lfo (s, 0, 0, 0.12f);
                    s.addMod (modsrc::Lfo1, "osc_a_position", 0.25f);
                    fx (s, FxChorus, 0.4f);
                    fx (s, FxStereoDelay, 0.18f, { { "feedback", 0.45f } });
                    reverb (s, 0.4f, 0.85f, 6.0f);
                    break;
                case 2:
                    wavetable (s, 0, "Formant Vowels", 0.3f, 0.65f); unison (s, 0, 3, 0.15f, 0.8f);
                    spectral (s, 1, "Soft Pad", false, 0.45f);
                    lfo (s, 0, 6, 0.1f);
                    s.addMod (modsrc::Lfo1, "osc_a_position", 0.4f);
                    lfo (s, 1, 0, 0.07f);
                    s.addMod (modsrc::Lfo2, "osc_b_spec_tilt", 0.4f);
                    filter (s, 0, SEM, MORPH, 3000.0f, 0.35f, 0.15f, 0.1f, 0.4f, 0.2f);
                    reverb (s, 0.45f, 0.9f, 8.0f);
                    break;
                case 3:
                    wavetable (s, 0, "Glass", 0.6f, 0.6f); unison (s, 0, 3, 0.1f, 0.9f);
                    spectral (s, 1, "Glass", false, 0.35f); s.p ("osc_b_spec_stretch", 0.25f); pitch (s, 1, 1);
                    filter (s, 0, SVF, LP12, 9000.0f, 0.1f);
                    reverb (s, 0.5f, 0.9f, 7.0f, 0.2f);
                    fx (s, FxStereoDelay, 0.2f);
                    break;
                case 4:
                    multisample (s, 0, "Choir Aah", 0.75f);
                    wavetable (s, 1, "Vocal Choir", 0.4f, 0.35f); unison (s, 1, 3, 0.15f, 0.9f);
                    filter (s, 0, SVF, LP12, 5500.0f, 0.1f);
                    env (s, 0, 0.6f, 1.0f, 0.9f, 1.6f);
                    reverb (s, 0.45f, 0.85f, 5.0f);
                    break;
                default:
                    multisample (s, 0, "Ensemble Strings", 0.7f);
                    analog (s, 1, 0.67f, 0.35f); unison (s, 1, 4, 0.2f, 0.9f);
                    filter (s, 0, P5, LP24, 3500.0f, 0.12f, 0.2f, 0.1f, 0.5f, 0.4f);
                    env (s, 0, 0.45f, 1.0f, 0.9f, 1.2f);
                    fx (s, FxDimension, 1.0f, { { "mode", 0.0f } });
                    reverb (s, 0.35f, 0.8f, 4.0f);
                    break;
            }
            break;

        case Archetype::Lead:
            voice (s, 2, 0.06f, 1);
            env (s, 0, 0.004f, 0.4f, 0.85f, 0.25f);
            switch (v % 5)
            {
                case 0:
                    analog (s, 0, 0.67f, 0.7f); unison (s, 0, 3, 0.12f, 0.6f);
                    analog (s, 1, 1.0f, 0.45f); pitch (s, 1, 0, 0, 5.0f);
                    filter (s, 0, MG, LP24, 2800.0f, 0.3f, 0.35f, 0.25f, 0.6f, 0.4f);
                    env (s, 1, 0.005f, 0.6f, 0.4f, 0.3f);
                    break;
                case 1:
                    analog (s, 0, 1.0f, 0.75f, 0.35f);
                    lfo (s, 0, 1, 0.45f);
                    s.addMod (modsrc::Lfo1, "osc_a_pulse_width", 0.18f);
                    filter (s, 0, JP, LP24, 2400.0f, 0.2f, 0.3f, 0.2f, 0.6f, 0.5f);
                    break;
                case 2:
                    wavetable (s, 0, "Hard Sync", 0.1f, 0.75f); unison (s, 0, 2, 0.1f, 0.5f);
                    env (s, 2, 0.0f, 0.8f, 0.2f, 0.5f);
                    s.addMod (modsrc::Env3, "osc_a_position", 0.6f);
                    filter (s, 0, MG, LP24, 6000.0f, 0.2f, 0.3f, 0.0f, 0.5f, 0.5f);
                    fx (s, FxDistortion, 0.35f, { { "drive", 10.0f } });
                    break;
                case 3:
                    wavetable (s, 0, "FM Ratios", 0.35f, 0.75f);
                    analog (s, 1, 0.0f, 0.4f);
                    xmod (s, 0, 1, 1, 0.2f);
                    filter (s, 0, SVF, LP12, 5000.0f, 0.15f);
                    break;
                default:
                    analog (s, 0, 0.67f, 0.8f);
                    filter (s, 0, MS, LP12, 1400.0f, 0.72f, 0.55f, 0.3f, 0.6f, 0.7f);
                    env (s, 1, 0.01f, 0.5f, 0.5f, 0.3f);
                    break;
            }
            vibrato (s, 1, 5.4f, 0.15f);
            fx (s, FxStereoDelay, 0.22f, { { "feedback", 0.35f } });
            reverb (s, 0.15f, 0.6f, 2.5f);
            break;

        case Archetype::Pluck:
            voice (s, 0, 0.0f, 16);
            env (s, 0, 0.001f, 0.6f, 0.0f, 0.35f);
            env (s, 1, 0.001f, 0.22f, 0.0f, 0.2f);
            switch (v % 4)
            {
                case 0:
                    analog (s, 0, 0.67f, 0.7f);
                    analog (s, 1, 1.0f, 0.45f); pitch (s, 1, 0, 0, 7.0f);
                    filter (s, 0, MG, LP24, 300.0f, 0.3f, 0.3f, 0.6f, 0.5f, 0.4f);
                    velocityToCutoff (s, 0, 0.12f);
                    break;
                case 1:
                    wavetable (s, 0, "Electric Tine", 0.0f, 0.75f);
                    env (s, 2, 0.0f, 0.3f, 0.0f, 0.3f);
                    s.addMod (modsrc::Env3, "osc_a_position", 0.7f);
                    filter (s, 0, SVF, LP12, 4000.0f, 0.2f, 0.1f, 0.3f, 0.5f, 0.5f);
                    break;
                case 2:
                    sample (s, 0, "Kalimba", 0.8f, 64, 0);
                    analog (s, 1, 0.0f, 0.15f); pitch (s, 1, 2);
                    filter (s, 0, SVF, LP12, 9000.0f, 0.05f);
                    break;
                default:
                    multisample (s, 0, "Nylon Pluck", 0.85f);
                    filterOff (s, 0);
                    env (s, 0, 0.001f, 3.0f, 0.0f, 0.6f);
                    break;
            }
            fx (s, FxStereoDelay, 0.25f, { { "feedback", 0.4f } });
            reverb (s, 0.2f, 0.6f, 2.2f);
            break;

        case Archetype::Keys:
            voice (s, 0, 0.0f, 16);
            switch (v % 4)
            {
                case 0:
                    multisample (s, 0, "Tine Piano", 0.85f);
                    filterOff (s, 0);
                    env (s, 0, 0.001f, 4.0f, 0.0f, 0.4f);
                    lfo (s, 0, 0, 4.5f);
                    s.addMod (modsrc::Lfo1, "osc_a_pan", 0.35f);
                    fx (s, FxChorus, 0.35f, { { "rate", 0.4f } });
                    reverb (s, 0.18f, 0.5f, 1.8f);
                    break;
                case 1:
                    multisample (s, 0, "Felt Piano", 0.85f);
                    filterOff (s, 0);
                    env (s, 0, 0.001f, 6.0f, 0.0f, 0.5f);
                    reverb (s, 0.25f, 0.6f, 2.5f);
                    break;
                case 2:
                    wavetable (s, 0, "Electric Tine", 0.0f, 0.8f);
                    env (s, 2, 0.0f, 1.2f, 0.0f, 0.8f);
                    s.addMod (modsrc::Env3, "osc_a_position", 0.6f);
                    analog (s, 1, 0.0f, 0.25f);
                    filterOff (s, 0);
                    env (s, 0, 0.001f, 2.5f, 0.0f, 0.5f);
                    fx (s, FxChorus, 0.3f);
                    reverb (s, 0.2f, 0.55f, 2.0f);
                    break;
                default:
                    analog (s, 0, 1.0f, 0.8f, 0.22f);
                    filter (s, 0, MG, LP24, 900.0f, 0.25f, 0.3f, 0.45f, 0.6f, 0.4f);
                    env (s, 1, 0.001f, 0.18f, 0.1f, 0.1f);
                    env (s, 0, 0.001f, 0.5f, 0.3f, 0.08f);
                    fx (s, FxPhaser, 0.3f);
                    break;
            }
            break;

        case Archetype::Organ:
            voice (s, 0, 0.0f, 16);
            env (s, 0, 0.004f, 0.1f, 1.0f, 0.06f);
            wavetable (s, 0, "Drawbar Organ", v % 2 == 0 ? 0.3f : 0.65f, 0.75f);
            filterOff (s, 0);
            if (v % 2 == 1)
            {
                analog (s, 1, 1.0f, 0.25f); pitch (s, 1, 1);
                filter (s, 0, MG, LP24, 3000.0f, 0.15f, 0.3f, 0.15f);
                env (s, 0, 0.002f, 0.4f, 0.6f, 0.08f);
            }
            fx (s, FxChorus, 0.45f, { { "rate", 5.5f }, { "depth", 0.25f } });
            fx (s, FxTube, 0.8f, { { "drive", 6.0f } });
            reverb (s, 0.15f, 0.5f, 1.5f);
            break;

        case Archetype::Strings:
            voice (s, 0, 0.0f, 16);
            switch (v % 3)
            {
                case 0:
                    multisample (s, 0, "Ensemble Strings", 0.8f);
                    filter (s, 0, SVF, LP12, 7000.0f, 0.05f);
                    env (s, 0, 0.3f, 1.0f, 0.9f, 0.9f);
                    break;
                case 1:
                    analog (s, 0, 0.67f, 0.7f); unison (s, 0, 5, 0.18f, 0.85f);
                    analog (s, 1, 0.67f, 0.35f); pitch (s, 1, 1); unison (s, 1, 3, 0.15f, 0.9f);
                    filter (s, 0, P5, LP24, 4000.0f, 0.1f, 0.2f, 0.05f, 0.5f, 0.4f);
                    env (s, 0, 0.4f, 1.0f, 0.85f, 1.0f);
                    fx (s, FxDimension, 1.0f, { { "mode", 2.0f } });
                    break;
                default:
                    spectral (s, 0, "Strings Ensemble", false, 0.75f); unison (s, 0, 4, 0.12f, 0.85f);
                    filter (s, 0, SVF, LP12, 6000.0f, 0.1f);
                    env (s, 0, 0.35f, 1.0f, 0.9f, 1.0f);
                    break;
            }
            vibrato (s, 1, 5.0f, 0.06f);
            reverb (s, 0.3f, 0.75f, 3.5f);
            break;

        case Archetype::Choir:
            voice (s, 0, 0.0f, 16);
            env (s, 0, 0.4f, 1.0f, 0.95f, 1.3f);
            switch (v % 3)
            {
                case 0: multisample (s, 0, "Choir Aah", 0.85f); spectral (s, 1, "Vocal Choir", false, 0.25f); break;
                case 1: multisample (s, 0, "Choir Ooh", 0.85f); break;
                default:
                    wavetable (s, 0, "Formant Vowels", 0.0f, 0.7f); unison (s, 0, 5, 0.12f, 0.9f);
                    wavetable (s, 1, "Vocal Choir", 0.5f, 0.4f); unison (s, 1, 3, 0.1f, 0.9f);
                    lfo (s, 0, 6, 0.15f);
                    s.addMod (modsrc::Lfo1, "osc_a_position", 0.5f, 2);
                    vibrato (s, 1, 5.2f, 0.12f);
                    break;
            }
            filter (s, 0, SVF, LP12, 6000.0f, 0.1f);
            reverb (s, 0.45f, 0.85f, 5.0f);
            break;

        case Archetype::Brass:
            voice (s, 0, 0.0f, 12);
            switch (v % 3)
            {
                case 0:
                    multisample (s, 0, "Brass Section", 0.85f);
                    filter (s, 0, MG, LP24, 5000.0f, 0.1f, 0.25f, 0.15f, 0.5f, 0.5f);
                    env (s, 0, 0.05f, 0.8f, 0.9f, 0.35f);
                    break;
                case 1:
                    analog (s, 0, 0.67f, 0.7f); unison (s, 0, 2, 0.1f, 0.6f);
                    analog (s, 1, 0.67f, 0.55f); pitch (s, 1, 0, 0, 8.0f);
                    filter (s, 0, JP, LP24, 850.0f, 0.15f, 0.3f, 0.45f, 0.6f, 0.5f);
                    env (s, 1, 0.08f, 0.6f, 0.5f, 0.4f);
                    env (s, 0, 0.03f, 0.5f, 0.85f, 0.4f);
                    fx (s, FxChorus, 0.3f);
                    break;
                default:
                    wavetable (s, 0, "Brass", 0.2f, 0.75f); unison (s, 0, 3, 0.1f, 0.7f);
                    env (s, 2, 0.12f, 0.6f, 0.4f, 0.4f);
                    s.addMod (modsrc::Env3, "osc_a_position", 0.6f);
                    filter (s, 0, P5, LP24, 3000.0f, 0.15f, 0.3f, 0.2f);
                    env (s, 0, 0.04f, 0.5f, 0.9f, 0.3f);
                    break;
            }
            vibrato (s, 1, 5.5f, 0.1f);
            reverb (s, 0.25f, 0.7f, 2.5f);
            break;

        case Archetype::Bell:
            voice (s, 0, 0.0f, 16);
            env (s, 0, 0.001f, 3.5f, 0.0f, 2.5f);
            switch (v % 3)
            {
                case 0:
                    wavetable (s, 0, "FM Ratios", 0.7f, 0.65f);
                    spectral (s, 1, "Glass", false, 0.4f); s.p ("osc_b_spec_stretch", 0.4f);
                    filterOff (s, 0);
                    break;
                case 1:
                    multisample (s, 0, "Glass Mallet", 0.85f);
                    filterOff (s, 0);
                    env (s, 0, 0.001f, 3.0f, 0.0f, 1.0f);
                    break;
                default:
                    spectral (s, 0, "Glass Bowl", true, 0.75f); s.p ("osc_a_spec_stretch", 0.2f);
                    s.p ("osc_a_root_note", 64.0f);
                    filterOff (s, 0);
                    break;
            }
            reverb (s, 0.35f, 0.8f, 4.5f, 0.25f);
            fx (s, FxStereoDelay, 0.15f);
            break;

        case Archetype::Texture:
            voice (s, 0, 0.0f, 12);
            env (s, 0, 1.2f, 2.0f, 0.9f, 3.0f);
            switch (v % 4)
            {
                case 0:
                    granular (s, 0, "Granular Texture", 0.4f, 140.0f, 35.0f, 0.4f, 0.8f);
                    lfo (s, 0, 6, 0.08f);
                    s.addMod (modsrc::Lfo1, "osc_a_position", 0.35f);
                    break;
                case 1:
                    sample (s, 0, "Breath Pad", 0.7f, 60, 1);
                    granular (s, 1, "Breath Pad", 0.5f, 200.0f, 25.0f, 0.6f, 0.5f);
                    break;
                case 2:
                    spectral (s, 0, "Analog Drone", true, 0.75f); s.p ("osc_a_spec_speed", 0.3f).p ("osc_a_root_note", 48.0f);
                    lfo (s, 0, 0, 0.05f);
                    s.addMod (modsrc::Lfo1, "osc_a_spec_formant", 0.15f);
                    break;
                default:
                    sample (s, 0, "Vinyl Noise", 0.35f, 60, 1);
                    wavetable (s, 1, "Soft Pad", 0.5f, 0.6f); unison (s, 1, 4, 0.2f, 0.9f);
                    break;
            }
            filter (s, 0, SEM, LP24, 4500.0f, 0.2f, 0.2f, 0.05f);
            reverb (s, 0.5f, 0.95f, 9.0f);
            fx (s, FxTapeDelay, 0.2f);
            break;

        case Archetype::Drone:
            voice (s, 0, 0.0f, 8);
            env (s, 0, 2.5f, 3.0f, 1.0f, 5.0f);
            analog (s, 0, 0.67f, 0.6f); pitch (s, 0, -1); unison (s, 0, 6, 0.4f, 0.9f);
            granular (s, 1, "Analog Drone", 0.3f, 300.0f, 20.0f, 0.5f, 0.5f);
            filter (s, 0, SEM, LP24, 450.0f, 0.4f, 0.3f, 0.0f, 0.2f, 0.5f);
            filterLfo (s, 0, 0, 0.05f, 0.2f);
            reverb (s, 0.55f, 1.0f, 14.0f);
            break;

        case Archetype::Sfx:
            voice (s, 0, 0.0f, 8);
            switch (v % 4)
            {
                case 0: // riser
                    noise (s, 0.4f, 0, 0.3f);
                    wavetable (s, 0, "Harmonic Sweep", 0.0f, 0.6f); unison (s, 0, 5, 0.3f, 0.9f);
                    env (s, 2, 6.0f, 0.1f, 1.0f, 1.0f);
                    s.p ("env3_attack_curve", 0.6f);
                    s.addMod (modsrc::Env3, "osc_a_octave", 0.25f);
                    s.addMod (modsrc::Env3, "osc_a_position", 1.0f);
                    filter (s, 0, SVF, HP, 200.0f, 0.4f);
                    s.addMod (modsrc::Env3, "flt1_cutoff", 0.6f);
                    env (s, 0, 0.5f, 0.1f, 1.0f, 2.0f);
                    break;
                case 1: // laser
                    analog (s, 0, 0.67f, 0.8f);
                    pitchEnv (s, 2, 1, 36.0f, 0.18f);
                    filter (s, 0, MS, LP12, 3000.0f, 0.6f, 0.4f);
                    env (s, 0, 0.001f, 0.3f, 0.0f, 0.2f);
                    break;
                case 2: // impact
                    analog (s, 0, 0.0f, 0.9f); pitch (s, 0, -1);
                    pitchEnv (s, 2, 1, 24.0f, 0.5f);
                    noise (s, 0.6f, 1, -0.2f);
                    filter (s, 0, MG, LP24, 1800.0f, 0.2f, 0.5f, 0.3f);
                    env (s, 1, 0.001f, 0.6f, 0.0f, 0.5f);
                    env (s, 0, 0.001f, 1.8f, 0.0f, 1.8f);
                    fx (s, FxDistortion, 0.4f, { { "drive", 12.0f } });
                    break;
                default: // siren
                    analog (s, 0, 1.0f, 0.7f);
                    lfo (s, 0, 1, 0.6f);
                    s.addMod (modsrc::Lfo1, "osc_a_semi", 0.3f);
                    filter (s, 0, JP, BP, 1500.0f, 0.5f);
                    env (s, 0, 0.05f, 0.1f, 1.0f, 0.5f);
                    break;
            }
            reverb (s, 0.4f, 0.9f, 6.0f);
            break;

        case Archetype::Sequence:
        case Archetype::Arp:
            archetype (s, Archetype::Pluck, v);
            if (a == Archetype::Sequence)
                s.p ("arp_mode", 2.0f).p ("seq_rate", 4.0f).p ("seq_length", 16.0f).p ("seq_follow", 1.0f);
            else
                s.p ("arp_mode", 1.0f).p ("arp_pattern", 2.0f).p ("arp_octaves", 2.0f).p ("arp_rate", 4.0f).p ("arp_gate", 0.5f);
            fx (s, pid::FxStereoDelay, 0.25f, { { "sync", 1.0f }, { "div_l", 8.0f }, { "div_r", 10.0f }, { "feedback", 0.4f } });
            break;
    }
}

} // namespace vsx::Recipes
