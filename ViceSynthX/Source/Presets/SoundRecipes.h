#pragma once

#include "PresetFormat.h"

/**
    Sound-design building blocks shared by the factory library and the offline AI designer.
    All functions operate on a PatchSpec in real parameter units.
*/
namespace vsx::Recipes
{
enum Model { MG = 0, DIODE, ACID, MS, JP, JN, P5, SEM, SVF };
enum Mode { LP24 = 0, LP12, BP, HP, NOTCH, MORPH };
enum OscMode { WT = 0, VA, SAMPLE, MULTI, GRAIN, SPECTRAL };

juce::String oscId (int o);   // "osc_a"
juce::String fxId (int type); // "fx_reverb"

PatchSpec init (const juce::String& name = "Init", const juce::String& category = "Analog");

void osc (PatchSpec& s, int o, int mode, float level = 0.75f, float position = 0.0f);
void wavetable (PatchSpec& s, int o, const juce::String& factoryName, float position = 0.0f, float level = 0.75f);
void analog (PatchSpec& s, int o, float shape, float level = 0.75f, float pw = 0.5f); // shape 0 sine .. 0.67 saw .. 1 square
void sample (PatchSpec& s, int o, const juce::String& builtinName, float level = 0.75f, int root = 60, int loopMode = 1);
void multisample (PatchSpec& s, int o, const juce::String& builtinName, float level = 0.75f);
void granular (PatchSpec& s, int o, const juce::String& builtinSample, float pos, float size, float density, float spray, float level = 0.7f);
void spectral (PatchSpec& s, int o, const juce::String& tableOrSample, bool fromSample, float level = 0.7f);
void pitch (PatchSpec& s, int o, int octave, int semi = 0, float fine = 0.0f);
void unison (PatchSpec& s, int o, int voices, float detune, float spread = 0.7f, float blend = 0.75f);
void warp (PatchSpec& s, int o, int mode, float amount);
void xmod (PatchSpec& s, int o, int mode, int source, float amount);
void route (PatchSpec& s, int o, int r);
void sub (PatchSpec& s, float level, int octave = -1, int shape = 0);
void noise (PatchSpec& s, float level, int type = 0, float color = 0.0f);

void filter (PatchSpec& s, int f, int model, int mode, float cutoff, float reso, float drive = 0.2f, float envAmt = 0.0f,
             float keyTrack = 0.5f, float character = 0.5f);
void filterOff (PatchSpec& s, int f);
void env (PatchSpec& s, int e, float a, float d, float sus, float r, float hold = 0.0f);
void lfo (PatchSpec& s, int l, int shape, float rate, bool sync = false, int div = 10, int mode = 0);
void voice (PatchSpec& s, int mode, float glide = 0.0f, int polyphony = 16);

void fx (PatchSpec& s, int type, float mix, std::initializer_list<std::pair<const char*, float>> params = {});
void fxOff (PatchSpec& s, int type);

void vibrato (PatchSpec& s, int lfoIndex, float rateHz, float depthOnModWheel);
void filterLfo (PatchSpec& s, int lfoIndex, int filterIndex, float rate, float depth, bool sync = false, int div = 10);
void pitchEnv (PatchSpec& s, int envIndex, int oscMask, float semitones, float decay);
void velocityToCutoff (PatchSpec& s, int filterIndex, float amount);
void macros (PatchSpec& s); // standard macro set: 1 brightness, 2 movement, 3 space, 4 drive

// ---- archetypes (used by factory presets and the offline designer) ---------------------------
enum class Archetype { Bass, Pad, Lead, Pluck, Keys, Organ, Strings, Choir, Brass, Bell, Texture, Drone, Sfx, Sequence, Arp };
void archetype (PatchSpec& s, Archetype a, int variant = 0);
juce::String archetypeName (Archetype a);

} // namespace vsx::Recipes
