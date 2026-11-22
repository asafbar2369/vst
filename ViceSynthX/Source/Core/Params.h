#pragma once
#include <array>

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>
#include <vector>

/**
    Central parameter registry.

    Every automatable / modulatable control of VICE SYNTH X is declared exactly once
    here. The DSP engine, GUI, DAW automation, preset system and AI layer all read
    from this registry, so parameter IDs, ranges and descriptions can never drift
    between subsystems.

    Index layout is fixed at compile time (constexpr bases below) and verified at
    registration time.
*/
namespace vsx
{
constexpr int kNumOscs = 3;
constexpr int kNumFilters = 2;
constexpr int kNumEnvs = 4;
constexpr int kNumLfos = 10;
constexpr int kNumMacros = 8;
constexpr int kNumModSlots = 32;
constexpr int kMaxUnison = 16;
constexpr int kMaxVoices = 32;

namespace pid
{
    namespace global { enum : int { MasterVolume, MasterTune, Transpose, Polyphony, VoiceMode, GlideTime, GlideMode,
                                    BendUp, BendDown, AmpVelocity, OscDrift, Quality, FilterRouting, FxRouting,
                                    FxBalance, MasterOut, FxBypass,
                                    VecOn, VecX, VecY, VecD, WseqOn, WseqSteps, WseqRate, WseqSync, WseqDiv, WseqXfade,
                                    WseqStart, WseqEnd, WseqPattern, MpeOn, MpeBendRange, Morph, Count }; }

    namespace osc { enum : int { On, Mode, Source, Level, Pan, Octave, Semi, Fine, Position, Unison, Detune, Blend, Spread,
                                 Phase, PhaseRand, WarpMode, WarpAmount, XModMode, XModSource, XModAmount, Fold, Route,
                                 PulseWidth, LoopMode, LoopStart, LoopEnd, LoopXfade, RootNote,
                                 GrainSize, GrainDensity, GrainSpray, GrainPitchRand, GrainWindow, GrainSpread,
                                 SpecTilt, SpecOddEven, SpecStretch, SpecFormant, SpecPartials, SpecSpeed, SampleEnd, RootAuto, FmAmount,
                                 PhysExcite, PhysDecay, PhysBright, PhysDamp, PhysInharm, PhysMaterial, PdShape, PdAmount,
                                 AddDecay, AddMotion, Count }; }

    namespace sub { enum : int { On, Shape, Octave, Level, Pan, Route, Count }; }
    namespace noise { enum : int { On, Type, Level, Color, Pan, Route, KeyTrack, Width, Count }; }

    namespace flt { enum : int { On, Model, Mode, Cutoff, Resonance, Drive, EnvAmount, KeyTrack, Drift, Tolerance,
                                 Character, Mix, Gain, FmAmount, FmSource, Count }; }

    namespace env { enum : int { Delay, Attack, Hold, Decay, Sustain, Release, AttackCurve, DecayCurve, ReleaseCurve, Count }; }
    namespace lfo { enum : int { Shape, Rate, Sync, SyncDiv, Mode, Phase, Delay, Fade, Smooth, Depth, Count }; }

    // Effects: every effect starts with On, Mix, Bus
    namespace fx { enum : int { On, Mix, Bus, CommonCount }; }
    namespace dist   { enum : int { Type = fx::CommonCount, Drive, Tone, Output, Count }; }
    namespace tube   { enum : int { Drive = fx::CommonCount, Bias, Tone, Output, Count }; }
    namespace tape   { enum : int { Drive = fx::CommonCount, Bump, Tone, Output, Count }; }
    namespace eq     { enum : int { LowCut = fx::CommonCount, LowFreq, LowGain, Mid1Freq, Mid1Gain, Mid1Q, Mid2Freq, Mid2Gain,
                                    Mid2Q, HighFreq, HighGain, HighCut, Count }; }
    namespace mbc    { enum : int { LowXover = fx::CommonCount, HighXover, LowThresh, MidThresh, HighThresh, Ratio, Attack,
                                    Release, LowGain, MidGain, HighGain, Output, Count }; }
    namespace fshift { enum : int { Shift = fx::CommonCount, Stereo, Feedback, Count }; }
    namespace ring   { enum : int { Freq = fx::CommonCount, Shape, Spread, Count }; }
    namespace chorus { enum : int { Rate = fx::CommonCount, Depth, Delay, Feedback, Tone, Width, Count }; }
    namespace dimension { enum : int { Mode = fx::CommonCount, Width, Count }; }
    namespace flanger { enum : int { Rate = fx::CommonCount, Depth, Delay, Feedback, Stereo, Count }; }
    namespace phaser { enum : int { Rate = fx::CommonCount, Depth, Feedback, Stages, Center, Stereo, Count }; }
    namespace sdelay { enum : int { TimeL = fx::CommonCount, TimeR, Sync, DivL, DivR, Feedback, PingPong, LowCut, HighCut, Count }; }
    namespace tdelay { enum : int { Time = fx::CommonCount, Sync, Div, Feedback, Wow, Flutter, Saturation, Tone, Count }; }
    namespace ddelay { enum : int { Time = fx::CommonCount, Sync, Div, Feedback, Crush, Spread, Count }; }
    namespace reverb { enum : int { Size = fx::CommonCount, Decay, Predelay, Damping, Diffusion, Modulation, Width, LowCut, Program, Count }; }
    namespace conv   { enum : int { Impulse = fx::CommonCount, Predelay, LowCut, HighCut, Length, Count }; }
    namespace stereo { enum : int { Width = fx::CommonCount, Haas, BassMono, Count }; }
    namespace limiter { enum : int { Input = fx::CommonCount, Ceiling, Release, Count }; }

    // effects added 2026-11-21
    namespace bbdchorus { enum : int { Mode = fx::CommonCount, Depth, Noise, Tone, Width, Count }; }
    namespace bbddelay  { enum : int { Time = fx::CommonCount, Sync, Div, Feedback, Stages, Mod, Noise, Count }; }
    namespace shimmer   { enum : int { Size = fx::CommonCount, Decay, Amount, Pitch, Damping, Predelay, Count }; }
    namespace rotary    { enum : int { Speed = fx::CommonCount, Accel, Balance, Drive, Width, Count }; }
    namespace resonator { enum : int { Note = fx::CommonCount, Chord, Decay, Tone, Spread, Count }; }
    namespace transient { enum : int { Attack = fx::CommonCount, Sustain, Output, Count }; }
    namespace spectral  { enum : int { Blur = fx::CommonCount, Freeze, Tilt, Count }; }

    namespace arp { enum : int { Mode, Pattern, Octaves, Rate, Gate, Swing, Latch, OctaveMode, VelMode, Probability, Count }; }
    namespace seq { enum : int { Rate, Length, Swing, Gate, Follow, VelScale, Count }; }

    // Effect type ordering (also the default rack order)
    enum FxType : int { FxDistortion, FxTube, FxTape, FxEq, FxMbComp, FxFreqShift, FxRingMod, FxChorus, FxDimension,
                        FxFlanger, FxPhaser, FxStereoDelay, FxTapeDelay, FxDigitalDelay, FxReverb, FxConvolution,
                        FxStereo, FxLimiter,
                        FxBbdChorus, FxBbdDelay, FxShimmer, FxRotary, FxResonator, FxTransient, FxSpectral, NumFx };

    constexpr int kFxParamCounts[NumFx] = { dist::Count, tube::Count, tape::Count, eq::Count, mbc::Count, fshift::Count,
                                            ring::Count, chorus::Count, dimension::Count, flanger::Count, phaser::Count,
                                            sdelay::Count, tdelay::Count, ddelay::Count, reverb::Count, conv::Count,
                                            stereo::Count, limiter::Count,
                                            bbdchorus::Count, bbddelay::Count, shimmer::Count, rotary::Count, resonator::Count,
                                            transient::Count, spectral::Count };

    /** Default rack order: the new effects sit with their kind (dynamics, modulation, delays, reverbs); stereo and
        limiter stay last. (FxType values are stable IDs; their numeric order is not the processing order.) */
    constexpr std::array<int, NumFx> defaultFxOrder()
    {
        return { FxDistortion, FxTube, FxTape, FxEq, FxMbComp, FxTransient, FxFreqShift, FxRingMod, FxChorus, FxBbdChorus,
                 FxDimension, FxFlanger, FxPhaser, FxRotary, FxResonator, FxStereoDelay, FxTapeDelay, FxDigitalDelay,
                 FxBbdDelay, FxSpectral, FxReverb, FxShimmer, FxConvolution, FxStereo, FxLimiter };
    }

    /** A stored rack order (presets / projects made before effects were added list fewer types) completed to all
        NumFx types: listed types keep their order, missing ones go to their default position; invalid or repeated
        entries are ignored. */
    std::array<int, NumFx> completeFxOrder (const std::vector<int>& stored);

    constexpr int kGlobalBase = 0;
    constexpr int kOscBase = kGlobalBase + global::Count;
    constexpr int kSubBase = kOscBase + kNumOscs * osc::Count;
    constexpr int kNoiseBase = kSubBase + sub::Count;
    constexpr int kFilterBase = kNoiseBase + noise::Count;
    constexpr int kEnvBase = kFilterBase + kNumFilters * flt::Count;
    constexpr int kLfoBase = kEnvBase + kNumEnvs * env::Count;
    constexpr int kMacroBase = kLfoBase + kNumLfos * lfo::Count;
    constexpr int kModAmountBase = kMacroBase + kNumMacros;
    constexpr int kFxBase = kModAmountBase + kNumModSlots;

    constexpr int fxBase (int type)
    {
        int b = kFxBase;
        for (int i = 0; i < type; ++i) b += kFxParamCounts[i];
        return b;
    }

    constexpr int kArpBase = fxBase (NumFx);
    constexpr int kSeqBase = kArpBase + arp::Count;
    constexpr int kNumParams = kSeqBase + seq::Count;

    constexpr int G (int p) { return kGlobalBase + p; }
    constexpr int O (int oscIndex, int p) { return kOscBase + oscIndex * osc::Count + p; }
    constexpr int SUB (int p) { return kSubBase + p; }
    constexpr int NOISE (int p) { return kNoiseBase + p; }
    constexpr int F (int filterIndex, int p) { return kFilterBase + filterIndex * flt::Count + p; }
    constexpr int E (int envIndex, int p) { return kEnvBase + envIndex * env::Count + p; }
    constexpr int L (int lfoIndex, int p) { return kLfoBase + lfoIndex * lfo::Count + p; }
    constexpr int MACRO (int i) { return kMacroBase + i; }
    constexpr int MODAMT (int slot) { return kModAmountBase + slot; }
    constexpr int FX (int type, int p) { return fxBase (type) + p; }
    constexpr int ARP (int p) { return kArpBase + p; }
    constexpr int SEQ (int p) { return kSeqBase + p; }
    /** Session parameters: global, never stored in or loaded from presets (MASTER VOL, GLOBAL FX BYPASS). */
    constexpr bool isSessionParam (int i) { return i == G (global::MasterOut) || i == G (global::FxBypass)
                                                   || i == G (global::MpeOn) || i == G (global::MpeBendRange)
                                                   || i == G (global::Morph); }
} // namespace pid

// ---------------------------------------------------------------------------------
enum class ParamType { Float, Int, Bool, Choice };

struct ParamDef
{
    int index = -1;
    juce::String id;          // stable machine id, e.g. "osc_a_level"
    juce::String name;        // display name, e.g. "OSC A Level"
    juce::String group;       // e.g. "OSC A"
    juce::String unit;        // "Hz", "s", "dB", "%", "ct", "st", "deg", "ms", "x", ""
    juce::String description; // short description for tooltips and the AI schema
    ParamType type = ParamType::Float;
    float minValue = 0.0f, maxValue = 1.0f, defaultValue = 0.0f;
    juce::NormalisableRange<float> range;
    juce::StringArray choices;
    bool modulatable = false;
    bool automatable = true;
    int versionHint = 1;      // juce::ParameterID version: 2 for parameters added after the first release

    float toNormalised (float v) const { return range.convertTo0to1 (juce::jlimit (minValue, maxValue, v)); }
    float fromNormalised (float n) const { return range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n)); }
    float snap (float v) const;
    bool isDiscrete() const { return type != ParamType::Float; }
};

class ParamRegistry
{
public:
    static const ParamRegistry& get();

    const std::vector<ParamDef>& all() const { return defs; }
    const ParamDef& operator[] (int index) const { return defs[(size_t) index]; }
    int size() const { return (int) defs.size(); }
    int indexOf (const juce::String& id) const;
    /** Number of parameters whose registration position differs from their declared index (must be 0). */
    static int layoutErrors();

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout() const;

    static juce::String formatValue (const ParamDef& def, float value, int maxChars = 12);
    static juce::StringArray syncDivisionNames();
    static double syncDivisionBeats (int index); // length in quarter-note beats

    /** Names used for the modulatable-destination list (indices into defs). */
    const std::vector<int>& modulatableIndices() const { return modulatable; }

private:
    ParamRegistry();
    std::vector<ParamDef> defs;
    std::unordered_map<std::string, int> lookup;
    std::vector<int> modulatable;
};

// ---------------------------------------------------------------------------------
// Modulation sources (shared by engine, GUI, presets and AI)
namespace modsrc
{
    enum : int
    {
        None = 0,
        Env1, Env2, Env3, Env4,
        Lfo1, Lfo2, Lfo3, Lfo4, Lfo5, Lfo6, Lfo7, Lfo8, Lfo9, Lfo10,
        Macro1, Macro2, Macro3, Macro4, Macro5, Macro6, Macro7, Macro8,
        Velocity, Note, ModWheel, Aftertouch, PitchBend, Random, Gate, Expression, Breath, Timbre, SeqMod,
        Count
    };

    juce::String name (int src);          // display name: "ENV 1", "LFO 3", "Macro 2", "Velocity"...
    juce::String id (int src);            // machine id:   "env1", "lfo3", "macro2", "velocity"...
    int fromId (const juce::String& id);  // -1 if unknown
    bool isBipolar (int src);             // native polarity
}

namespace choices
{
    extern const juce::StringArray oscModes, oscSources, warpModes, xmodModes, xmodSources, routes, loopModes, grainWindows,
        subShapes, noiseTypes, filterModels, filterModes, filterRoutings, voiceModes, glideModes, qualities,
        lfoShapes, lfoModes, fxRoutings, fxBuses, distTypes, dimensionModes, convImpulses, ringShapes, arpModes,
        arpPatterns, arpOctaveModes, arpVelModes, reverbPrograms, physExcites, physMaterials, pdShapes, vectorD, wseqPatterns,
        bbdModes, bbdStages, shimmerPitches, rotarySpeeds, resonatorChords;

    extern const juce::StringArray fxTypeNames; // display names for effect types
    extern const juce::StringArray fxTypeIds;   // machine ids, e.g. "distortion"
    juce::String filterModelDescription (int model);
}

} // namespace vsx
