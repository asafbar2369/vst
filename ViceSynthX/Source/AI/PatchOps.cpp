#include "PatchOps.h"
#include <cstring>
#include "AIProvider.h"
#include "../Synth/Macros.h"
#include "../Presets/PresetManager.h"
#include "../Presets/SoundRecipes.h"
#include "../Analysis/SoundMatcher.h"
#include "../Analysis/AudioFeatures.h"

namespace vsx
{
juce::String OpReport::summary() const
{
    juce::String s = juce::String (applied) + " change(s) applied";
    if (! warnings.isEmpty()) s << ", " << warnings.size() << " adjusted";
    if (! errors.isEmpty()) s << ", " << errors.size() << " rejected";
    return s;
}

namespace
{
    juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> props)
    {
        auto* o = new juce::DynamicObject();
        for (const auto& [k, v] : props) o->setProperty (k, v);
        return juce::var (o);
    }

    juce::var strArr (const juce::StringArray& a)
    {
        juce::Array<juce::var> r;
        for (const auto& s : a) r.add (s);
        return r;
    }

    juce::var typeOnly (const char* t) { return obj ({ { "type", t } }); }

    int oscFromLetter (const juce::var& v)
    {
        // strip an optional "OSC" / "OSC_" / "OSC " prefix (removeCharacters ("OSC ") also deleted the letter C itself,
        // so oscillator C could only be addressed as "3"; found with a live local model, 2026-11-25)
        auto s = v.toString().trim().toUpperCase();
        if (s.startsWith ("OSC")) s = s.substring (3).trimCharactersAtStart ("_ ").trim();
        if (s == "A" || s == "1") return 0;
        if (s == "B" || s == "2") return 1;
        if (s == "C" || s == "3") return 2;
        return -1;
    }

    juce::String matchName (const juce::StringArray& names, juce::String wanted)
    {
        // strip an optional prefix (fromFirstOccurrenceOf alone returns an empty string when the prefix is absent,
        // which rejected every plain name such as "Basic Shapes")
        for (const char* prefix : { "builtin:", "factory:" })
            if (wanted.startsWithIgnoreCase (prefix)) wanted = wanted.substring ((int) std::strlen (prefix));
        wanted = wanted.replaceCharacter ('_', ' ').trim();
        if (wanted.isEmpty()) return {};
        for (const auto& n : names) if (n.equalsIgnoreCase (wanted)) return n;
        for (const auto& n : names) if (n.containsIgnoreCase (wanted) || wanted.containsIgnoreCase (n)) return n;
        return {};
    }
}

PatchSpec PatchOps::initPatch()
{
    return Recipes::init ("AI Sound", "Analog");
}

juce::var PatchOps::responseSchema()
{
    const auto step = obj ({ { "type", "object" }, { "additionalProperties", false },
                             { "properties", obj ({ { "on", typeOnly ("boolean") },
                                                    { "notes", obj ({ { "type", "array" }, { "items", typeOnly ("integer") } }) },
                                                    { "velocity", typeOnly ("number") }, { "gate", typeOnly ("number") },
                                                    { "probability", typeOnly ("number") }, { "mod", typeOnly ("number") },
                                                    { "octave", typeOnly ("integer") }, { "tie", typeOnly ("boolean") },
                                                    { "ratchet", typeOnly ("integer") } }) } });

    const auto numArr = obj ({ { "type", "array" }, { "items", typeOnly ("number") } });
    const auto op = obj ({
        { "type", "object" }, { "additionalProperties", false },
        { "required", strArr ({ "op" }) },
        { "properties", obj ({
            { "op", obj ({ { "type", "string" }, { "enum", strArr ({ "set", "mod", "remove_mod", "clear_mods", "wavetable", "wavetable_harmonics",
                                                                        "sample", "multisample", "clear_sample", "fx_order", "lfo_curve",
                                                                        "seq_steps", "arp_steps" }) } }) },
            { "param", typeOnly ("string") },
            { "value", obj ({ { "anyOf", juce::Array<juce::var> { typeOnly ("number"), typeOnly ("string"), typeOnly ("boolean") } } }) },
            { "source", typeOnly ("string") }, { "dest", typeOnly ("string") }, { "amount", typeOnly ("number") },
            { "polarity", obj ({ { "type", "string" }, { "enum", strArr ({ "native", "bipolar", "unipolar" }) } }) },
            { "aux", typeOnly ("string") },
            { "osc", obj ({ { "type", "string" }, { "enum", strArr ({ "A", "B", "C" }) } }) },
            { "name", typeOnly ("string") },
            { "frames", obj ({ { "type", "array" }, { "items", numArr } }) },
            { "order", obj ({ { "type", "array" }, { "items", typeOnly ("string") } }) },
            { "lfo", typeOnly ("integer") },
            { "points", obj ({ { "type", "array" }, { "items", numArr } }) },
            { "steps", obj ({ { "type", "array" }, { "items", step } }) } }) } });

    const auto opList = obj ({ { "type", "array" }, { "items", op } });
    const auto variation = obj ({ { "type", "object" }, { "additionalProperties", false },
                                  { "required", strArr ({ "name", "operations" }) },
                                  { "properties", obj ({ { "name", typeOnly ("string") }, { "description", typeOnly ("string") },
                                                         { "operations", opList } }) } });
    return obj ({
        { "type", "object" }, { "additionalProperties", false },
        { "required", strArr ({ "reply", "action", "operations" }) },
        { "properties", obj ({
            { "reply", typeOnly ("string") },
            { "action", obj ({ { "type", "string" }, { "enum", strArr ({ "new_patch", "modify_patch", "answer_only", "ask_question" }) } }) },
            { "question", typeOnly ("string") },
            { "name", typeOnly ("string") },
            { "category", typeOnly ("string") },
            { "tags", obj ({ { "type", "array" }, { "items", typeOnly ("string") } }) },
            { "description", typeOnly ("string") },
            { "descriptors", obj ({ { "type", "array" }, { "items", typeOnly ("string") } }) },
            { "operations", opList },
            { "variations", obj ({ { "type", "array" }, { "items", variation } }) } }) } });
}

juce::String PatchOps::systemPrompt (bool compact)
{
    const auto& reg = ParamRegistry::get();
    juce::String p;
    if (compact)
    {
        // short header for small local contexts: the same contract, fewer words
        p << "You are the sound designer of the VICE SYNTH X synthesizer. Reply in English with ONE JSON object: reply (short "
             "explanation), action (new_patch = start from INIT | modify_patch = minimal changes to the current patch | "
             "answer_only | ask_question), question, name, category, tags, description, descriptors (darker, brighter, warmer, "
             "wider, narrower, softer-attack, punchier, longer-release, more-movement, more-space, drier), operations, "
             "optional variations (lists of operations). Never write code.\n"
             "OPERATIONS: {op:set, param, value} (real units; choices by name; use set to change any value); "
             "{op:mod, source:<a MOD SOURCES id>, dest:<parameter id>, amount -1..1, aux} (modulation routing only); "
             "{op:remove_mod, source, dest}; {op:clear_mods}; {op:wavetable, osc:A|B|C, name}; {op:sample, osc, name}; "
             "{op:multisample, osc, name}; {op:fx_order, order}; {op:seq_steps, steps:[{on, notes, velocity, gate, probability, ratchet}]}; "
             "{op:arp_steps, steps:[{on, velocity, octave, gate, ratchet}]}.\n"
             "INIT: OSC A analog saw, Filter 1 MG Ladder LP24 12 kHz, ENV1 amp 3 ms / 1 s / 100% / 150 ms, all else off.\n"
             "RULES: set an oscillator's mode before its mode settings; ENV1 = amplitude, ENV2 -> flt1_env_amount; turn effects "
             "on with fx_<id>_on; reverb / delay mix 0.15-0.45; moderate resonance; describe artist references as 'inspired by'.\n"
             "FORMAT EXAMPLE (write your own values; do not copy these): {\"reply\":\"Detuned saws through a slowly opening filter with chorus.\",\"action\":\"new_patch\",\"name\":\"Soft Saw Pad\","
             "\"category\":\"Pads\",\"operations\":[{\"op\":\"set\",\"param\":\"osc_a_unison\",\"value\":5},{\"op\":\"set\",\"param\":"
             "\"flt1_cutoff\",\"value\":1800},{\"op\":\"set\",\"param\":\"env1_attack\",\"value\":0.6},{\"op\":\"set\",\"param\":"
             "\"fx_chorus_on\",\"value\":true},{\"op\":\"mod\",\"source\":\"lfo1\",\"dest\":\"flt1_cutoff\",\"amount\":0.15}]}\n\n";
    }
    else
    p << "You are the built-in sound designer of VICE SYNTH X, a hybrid wavetable / virtual-analog / sample / granular / spectral "
         "synthesizer with analog circuit-modelled filters. You design playable presets by returning JSON operations that the "
         "synthesizer validates and applies. You never write code. Write all replies, questions, names and descriptions in English only. Hebrew AI support is disabled. Keep JSON keys, parameter ids, choice names and enum values exactly as listed here.\n\n"
         "RESPONSE: one JSON object with: reply (short explanation of your sound-design decisions for the musician), action "
         "(new_patch = start from the INIT patch; modify_patch = change the current patch with minimal operations; answer_only; "
         "ask_question = only when the request is genuinely ambiguous), question (when asking), name, category, tags, description, "
         "descriptors (perceptual goals you aimed for, from: darker, brighter, warmer, wider, narrower, softer-attack, punchier, "
         "longer-release, more-movement, more-space, drier), operations, and optional variations (each a list of operations applied "
         "on top of your main result).\n\n"
         "OPERATIONS:\n"
         "- {op:set, param:<id>, value:<number|choice name|bool>} values are in real units (Hz, seconds, dB, 0..1 for %).\n"
         "- {op:mod, source:<source id>, dest:<param id>, amount:-1..1, polarity:native|bipolar|unipolar, aux:<optional source that scales the amount, e.g. modwheel>}. "
         "amount is a fraction of the destination's full range (e.g. 0.1 on osc_a_fine = 20 cents of vibrato depth).\n"
         "- {op:remove_mod, source, dest}, {op:clear_mods}\n"
         "- {op:wavetable, osc:A|B|C, name:<factory wavetable>}\n"
         "- {op:wavetable_harmonics, osc, name, frames:[[h1,h2,...],...]} creates a custom wavetable: each frame lists harmonic "
         "amplitudes (fundamental first, up to 256 harmonics, up to 64 frames; position morphs between frames).\n"
         "- {op:sample, osc, name:<built-in sample>}, {op:multisample, osc, name:<built-in multisample>}, {op:clear_sample, osc}\n"
         "- {op:fx_order, order:[fx ids]}, {op:lfo_curve, lfo:1..10, points:[[x 0..1, y -1..1, tension -1..1],...]} (used when the LFO shape is Custom)\n"
         "- {op:seq_steps, steps:[{on, notes:[semitones relative to C4], velocity 0..1, gate 0..2, probability, mod -1..1, tie, ratchet 1..4}]}, "
         "{op:arp_steps, steps:[{on, velocity, octave, gate, ratchet 1..4}]} (ratchet: repeats within the step)\n\n"
         "INIT patch for new_patch: OSC A Analog saw (position 0.67) level 0.75, Filter 1 MG Ladder LP24 at 12 kHz, ENV1 amp "
         "(3 ms / 1 s / 100% / 150 ms), everything else at defaults (other oscillators, sub, noise, Filter 2, LFOs, effects off).\n\n"
         "SOUND DESIGN RULES: keep total oscillator level reasonable (unison already normalises), use ENV 1 for amplitude, ENV 2 for "
         "the filter via flt1_env_amount, set an oscillator's mode before mode-specific settings, turn effects on with fx_<name>_on, "
         "use reverb/delay mix around 0.15-0.45, avoid extreme resonance unless asked, prefer mod-wheel-scaled vibrato (aux:modwheel). "
         "When the user references an artist, era or instrument, translate it into concrete sonic characteristics; describe your result "
         "as 'inspired by' and never claim an exact reproduction. Built-in samples are original synthesised instruments, not "
         "recordings; say so when realism is requested. Variations should differ musically (filter, wavetable, modulation, FX).\n\n";

    if (compact)
    {
        // ---- compact tail: names and ranges only (measured with a real tokenizer, see Tests/TestAIPromptSize.cpp) ----
        p << "FILTER MODELS: " << choices::filterModels.joinIntoString (", ") << "\n";
        juce::StringArray srcs;
        for (int s = 1; s < modsrc::Count; ++s) srcs.add (modsrc::id (s));
        p << "MOD SOURCES: " << srcs.joinIntoString (", ") << "\n";
        juce::StringArray wt;
        for (const auto& n : WavetableFactory::names()) if (WavetableFactory::category (n) == "Classic") wt.add (n);
        p << "WAVETABLES (any of 500; e.g.): " << wt.joinIntoString (", ") << "\n";
        p << "SAMPLES: " << BuiltinSamples::singleNames().joinIntoString (", ") << "; MULTISAMPLES: " << BuiltinSamples::multiNames().joinIntoString (", ") << "\n";
        p << "EFFECTS: " << choices::fxTypeIds.joinIntoString (", ") << ". Each has fx_<id>_on (bool) and fx_<id>_mix (0..1).\n";
        p << "DIV = [" << ParamRegistry::syncDivisionNames().joinIntoString ("|") << "]\n\n";
        p << "PARAMETERS (id range|choices; real units, % = 0-1). osc_b_/osc_c_ mirror osc_a_, flt2_ mirrors flt1_, "
             "env2-4 mirror env1, lfo2-10 mirror lfo1. Other effect parameters stay at their defaults.\n";
        const juce::StringArray fullFx { "reverb", "stereodelay" };
        const juce::StringArray skipPrefixes { "wseq_", "vector_", "vec_" };
        const juce::StringArray skipOsc { "loop_mode", "loop_start", "loop_end", "loop_xfade", "root_note", "sample_end", "root_auto" };
        const auto divs = ParamRegistry::syncDivisionNames();
        for (const auto& d : reg.all())
        {
            const auto& id = d.id;
            if (d.group == "Matrix" || pid::isSessionParam (d.index)) continue;
            if (id.startsWith ("osc_b_") || id.startsWith ("osc_c_") || id.startsWith ("flt2_")) continue;
            if ((id.startsWith ("env") && ! id.startsWith ("env1_")) || (id.startsWith ("lfo") && ! id.startsWith ("lfo1_"))) continue;
            if (id.startsWith ("macro") && id != "macro1") continue;
            bool skip = false;
            for (const auto& sp : skipPrefixes) skip = skip || id.startsWith (sp);
            for (const auto& so : skipOsc) skip = skip || (id.startsWith ("osc_a_") && id.endsWith (so));
            if (skip) continue;
            if (id.startsWith ("fx_"))
            {
                const auto fx = id.fromFirstOccurrenceOf ("fx_", false, false).upToFirstOccurrenceOf ("_", false, false);
                if (! fullFx.contains (fx) || id.endsWith ("_on") || id.endsWith ("_mix")) continue;
            }
            juce::String type;
            switch (d.type)
            {
                case ParamType::Float: type = juce::String (d.minValue) + ".." + juce::String (d.maxValue); break;
                case ParamType::Int: type = juce::String ((int) d.minValue) + ".." + juce::String ((int) d.maxValue); break;
                case ParamType::Bool: type = "bool"; break;
                case ParamType::Choice: type = d.choices == divs ? juce::String ("DIV") : "[" + d.choices.joinIntoString ("|") + "]"; break;
            }
            p << id << " " << type << "\n";
        }
        return p;
    }

    p << "FILTER MODELS:\n";
    for (int m = 0; m < choices::filterModels.size(); ++m)
        p << "- " << choices::filterModels[m] << ": " << choices::filterModelDescription (m) << "\n";

    p << "\nMODULATION SOURCES: ";
    juce::StringArray srcs;
    for (int s = 1; s < modsrc::Count; ++s) srcs.add (modsrc::id (s));
    p << srcs.joinIntoString (", ") << " (env1 is the amp envelope; lfo/pitchbend/note/random/seqmod are bipolar).\n";
    p << "MACROS (engine-level, fixed functions applied on top of every patch; set macro1..macro8 to 0..1 to use them): ";
    for (int m = 0; m < kNumMacros; ++m) p << "macro" << (m + 1) << " = " << Macros::name (m) << " (" << Macros::describe (m).replace ("\n", " ") << "); ";
    p << "\n";
    {
        // 500 tables: the classic names in full, the generated families as category + examples (keeps the prompt short
        // for local models); any exact name from the library is accepted
        const auto all = WavetableFactory::names();
        juce::StringArray classic;
        for (const auto& n : all) if (WavetableFactory::category (n) == "Classic") classic.add (n);
        p << "FACTORY WAVETABLES (" << all.size() << "): " << classic.joinIntoString (", ");
        for (const auto& cat : WavetableFactory::categories())
        {
            if (cat == "Classic") continue;
            juce::StringArray ex;
            int count = 0;
            for (const auto& n : all)
                if (WavetableFactory::category (n) == cat) { if (count % 17 == 0 && ex.size() < 3) ex.add (n); ++count; }
            p << "; " << cat << " family (" << count << ", e.g. " << ex.joinIntoString (", ") << ")";
        }
        p << "\n";
    }
    p << "BUILT-IN SAMPLES: " << BuiltinSamples::singleNames().joinIntoString (", ") << "\n";
    p << "BUILT-IN MULTISAMPLES: " << BuiltinSamples::multiNames().joinIntoString (", ") << "\n";
    p << "EFFECT IDS: " << choices::fxTypeIds.joinIntoString (", ") << "\n";
    p << "PRESET CATEGORIES: " << FactoryPresets::categories().joinIntoString (", ") << "\n\n";

    p << "PARAMETERS (id | type | range or choices | unit | default | description). osc_b_*/osc_c_* mirror osc_a_*, "
         "flt2_* mirrors flt1_*, env2-env4 mirror env1 (env1 defaults are amp defaults), lfo2-lfo10 mirror lfo1, macro1-8 are 0..1. "
         "Modulatable destinations are marked [mod].\n";
    for (const auto& d : reg.all())
    {
        if (d.group == "Matrix") continue;
        const auto& id = d.id;
        if (id.startsWith ("osc_b_") || id.startsWith ("osc_c_") || id.startsWith ("flt2_")) continue;
        if ((id.startsWith ("env") && ! id.startsWith ("env1_")) || (id.startsWith ("lfo") && ! id.startsWith ("lfo1_"))) continue;
        if (id.startsWith ("macro") && id != "macro1") continue;
        juce::String type;
        switch (d.type)
        {
            case ParamType::Float: type = "float " + juce::String (d.minValue) + ".." + juce::String (d.maxValue); break;
            case ParamType::Int: type = "int " + juce::String ((int) d.minValue) + ".." + juce::String ((int) d.maxValue); break;
            case ParamType::Bool: type = "bool"; break;
            case ParamType::Choice: type = "choice [" + d.choices.joinIntoString ("|") + "]"; break;
        }
        const juce::String def = d.type == ParamType::Choice ? d.choices[(int) d.defaultValue]
                               : d.type == ParamType::Bool ? (d.defaultValue > 0.5f ? "on" : "off") : juce::String (d.defaultValue);
        p << id << " | " << type << " | " << (d.unit == "%" ? "0-1" : d.unit) << " | " << def << " | "
          << (d.modulatable ? "[mod] " : "") << d.description << "\n";
    }
    return p;
}

juce::String PatchOps::renderReport (const PatchSpec& patch, OfflineRenderer& renderer)
{
    const double noteSeconds = 1.5;
    const auto audio = renderer.render (patch, 60, 100, noteSeconds, 1.0, 1);
    const auto f = AudioFeatures::analyse (audio, renderer.getSampleRate(), noteSeconds);
    if (! f.valid || f.peakDb < -70.0f) return "The patch is SILENT at C4 (peak " + juce::String (f.peakDb, 1) + " dBFS). Check oscillator levels, filters and the amp envelope.";
    auto num = [] (float v, int d = 1) { return juce::String (v, d); };
    juce::StringArray l;
    l.add ("level: peak " + num (f.peakDb) + " dBFS, RMS " + num (f.rmsDb) + " dBFS, crest " + num (f.crestDb) + " dB");
    l.add (f.pitchConfidence > 0.5f ? "pitch: " + num (f.f0) + " Hz (C4 = 261.6 Hz), confidence " + num (f.pitchConfidence, 2)
                                     : juce::String ("pitch: no clear fundamental (noisy or inharmonic)"));
    l.add ("brightness: spectral centroid " + juce::String (juce::roundToInt (f.centroid)) + " Hz (start " + juce::String (juce::roundToInt (f.centroidStart))
           + " Hz, end " + juce::String (juce::roundToInt (f.centroidEnd)) + " Hz), 85 % roll-off " + juce::String (juce::roundToInt (f.rolloff)) + " Hz");
    l.add ("noisiness (spectral flatness 0 = tonal, 1 = noise): " + num (f.flatness, 2));
    l.add ("envelope: attack " + juce::String (juce::roundToInt (f.attack * 1000.0f)) + " ms, decay " + juce::String (juce::roundToInt (f.decay * 1000.0f))
           + " ms, sustain " + num (f.sustain, 2) + " of peak, release " + juce::String (juce::roundToInt (f.release * 1000.0f)) + " ms");
    if (f.tremoloDepth > 0.05f) l.add ("amplitude modulation: " + num (f.tremoloRate) + " Hz, depth " + num (f.tremoloDepth, 2));
    if (f.vibratoDepthCents > 3.0f) l.add ("vibrato: " + num (f.vibratoRate) + " Hz, " + num (f.vibratoDepthCents, 0) + " cents");
    l.add ("stereo: width (side/mid) " + num (f.stereoWidth, 2) + ", L/R correlation " + num (f.correlation, 2));
    if (f.peakDb > -1.0f) l.add ("WARNING: peaks above -1 dBFS - lower the level or drive");
    if (f.rmsDb < -36.0f) l.add ("WARNING: very quiet - raise oscillator level or master volume");
    if (f.correlation < 0.0f) l.add ("WARNING: negative L/R correlation - mono compatibility problem");
    return l.joinIntoString ("\n");
}

juce::String PatchOps::contextFor (const PatchSpec& current, const juce::String& referenceSummary)
{
    juce::String c = "CURRENT PATCH \"" + current.meta.name + "\" (category " + current.meta.category + "):\n" + current.summarise (160);
    if (referenceSummary.isNotEmpty())
        c << "\n\nREFERENCE AUDIO ANALYSIS (match this when asked):\n" << referenceSummary;
    return c;
}

// ---------------------------------------------------------------------------------
OpReport PatchOps::apply (PatchSpec& spec, const juce::var& operations)
{
    OpReport rep;
    const auto& reg = ParamRegistry::get();
    auto* ops = operations.getArray();
    if (ops == nullptr)
    {
        if (! operations.isVoid()) rep.errors.add ("'operations' is not a list.");
        return rep;
    }
    if (ops->size() > 600)
        rep.warnings.add ("Only the first 600 operations were considered.");

    int count = 0;
    for (const auto& o : *ops)
    {
        if (++count > 600) break;
        const auto kind = o["op"].toString().trim().toLowerCase();
        if (kind == "set")
        {
            auto id = o["param"].toString().trim().toLowerCase();
            int idx = reg.indexOf (id);
            if (idx < 0)
                for (const auto& d : reg.all())
                    if (d.name.equalsIgnoreCase (o["param"].toString().trim())) { idx = d.index; break; }
            if (idx < 0) { rep.errors.add ("Unknown parameter '" + o["param"].toString() + "'."); continue; }
            const auto& d = reg[idx];
            if (d.group == "Matrix") { rep.errors.add ("Use 'mod' operations to set modulation amounts (" + d.id + ")."); continue; }
            const auto& v = o["value"];
            if (! (v.isString() || v.isBool() || v.isInt() || v.isInt64() || v.isDouble()))
            {
                rep.errors.add (d.id + ": missing or invalid value.");
                continue;
            }
            float f = 0.0f;
            bool ok = true;
            if (d.type == ParamType::Choice)
            {
                if (v.isString())
                {
                    int ci = d.choices.indexOf (v.toString().trim(), true);
                    if (ci < 0)
                        for (int k = 0; k < d.choices.size(); ++k)
                            if (d.choices[k].containsIgnoreCase (v.toString().trim())) { ci = k; break; }
                    ok = ci >= 0;
                    f = (float) ci;
                }
                else if (v.isInt() || v.isDouble() || v.isInt64()) f = (float) (double) v;
                else ok = false;
                if (! ok) { rep.errors.add (d.id + ": '" + v.toString() + "' is not one of " + d.choices.joinIntoString (", ") + "."); continue; }
            }
            else if (d.type == ParamType::Bool)
            {
                if (v.isBool()) f = (bool) v ? 1.0f : 0.0f;
                else if (v.isString()) f = (v.toString().equalsIgnoreCase ("on") || v.toString().equalsIgnoreCase ("true") || v.toString() == "1") ? 1.0f : 0.0f;
                else f = (double) v >= 0.5 ? 1.0f : 0.0f;
            }
            else
            {
                if (v.isString())
                {
                    auto t = v.toString().trim();
                    f = t.retainCharacters ("0123456789.-+").getFloatValue();
                    if (d.unit == "Hz" && t.containsIgnoreCase ("k")) f *= 1000.0f;
                    if (d.unit == "s" && t.containsIgnoreCase ("ms")) f *= 0.001f;
                    if (d.unit == "%" && t.contains ("%")) f *= 0.01f;
                }
                else if (v.isBool()) f = (bool) v ? d.maxValue : d.minValue;
                else f = (float) (double) v;
                if (! std::isfinite (f)) { rep.errors.add (d.id + ": value is not a finite number."); continue; }
                if (d.unit == "%" && d.maxValue <= 1.0f && f > 1.0f && f <= 100.0f)
                {
                    rep.warnings.add (d.id + ": interpreted " + juce::String (f) + " as a percentage.");
                    f *= 0.01f;
                }
            }
            if (! std::isfinite (f)) { rep.errors.add (d.id + ": value is not a finite number."); continue; }
            if (f < d.minValue - 1.0e-6f || f > d.maxValue + 1.0e-6f)
                rep.warnings.add (d.id + ": " + juce::String (f) + " clamped to " + juce::String (juce::jlimit (d.minValue, d.maxValue, f)) + ".");
            spec.values[(size_t) idx] = d.snap (f);
            ++rep.applied;
        }
        else if (kind == "mod")
        {
            const int src = modsrc::fromId (o["source"].toString());
            const int dest = reg.indexOf (o["dest"].toString());
            if (src <= 0) { rep.errors.add ("Unknown modulation source '" + o["source"].toString() + "'."); continue; }
            if (dest < 0 || ! reg[dest].modulatable) { rep.errors.add ("'" + o["dest"].toString() + "' is not a modulation destination."); continue; }
            float amount = (float) (double) o.getProperty ("amount", 0.25);
            if (! std::isfinite (amount)) amount = 0.0f;
            if (std::abs (amount) > 1.0f) rep.warnings.add ("Modulation amount clamped to +/-1.");
            const auto pol = o["polarity"].toString();
            const int polarity = pol == "bipolar" ? 1 : pol == "unipolar" ? 2 : 0;
            const int aux = std::max (0, modsrc::fromId (o["aux"].toString()));
            if (spec.findMod (src, dest) < 0 && (int) spec.mods.size() >= kNumModSlots)
            {
                rep.errors.add ("All 32 modulation slots are in use.");
                continue;
            }
            spec.addMod (src, reg[dest].id, juce::jlimit (-1.0f, 1.0f, amount), polarity, aux);
            ++rep.applied;
        }
        else if (kind == "remove_mod")
        {
            spec.removeMod (modsrc::fromId (o["source"].toString()), reg.indexOf (o["dest"].toString()));
            ++rep.applied;
        }
        else if (kind == "clear_mods")
        {
            spec.mods.clear();
            for (int s = 0; s < kNumModSlots; ++s) spec.values[(size_t) pid::MODAMT (s)] = 0.0f;
            ++rep.applied;
        }
        else if (kind == "wavetable")
        {
            const int osc = oscFromLetter (o["osc"]);
            const auto name = matchName (WavetableFactory::names(), o["name"].toString());
            if (osc < 0) { rep.errors.add ("wavetable: unknown oscillator."); continue; }
            if (name.isEmpty()) { rep.errors.add ("Unknown factory wavetable '" + o["name"].toString() + "'."); continue; }
            spec.wavetableId[(size_t) osc] = "factory:" + name;
            spec.customWavetable[(size_t) osc].reset();
            ++rep.applied;
        }
        else if (kind == "wavetable_harmonics")
        {
            const int osc = oscFromLetter (o["osc"]);
            auto* frames = o["frames"].getArray();
            if (osc < 0 || frames == nullptr || frames->isEmpty()) { rep.errors.add ("wavetable_harmonics needs osc and frames."); continue; }
            std::vector<std::vector<float>> amps;
            for (int f = 0; f < frames->size() && f < 64; ++f)
            {
                std::vector<float> h;
                if (auto* fr = (*frames)[f].getArray())
                    for (int k = 0; k < fr->size() && k < 256; ++k)
                    {
                        const float a = (float) (double) (*fr)[k];
                        h.push_back (std::isfinite (a) ? juce::jlimit (-4.0f, 4.0f, a) : 0.0f);
                    }
                if (! h.empty()) amps.push_back (h);
            }
            if (amps.empty()) { rep.errors.add ("wavetable_harmonics: no valid frames."); continue; }
            const auto name = o["name"].toString().isNotEmpty() ? o["name"].toString().substring (0, 40) : juce::String ("AI Wavetable");
            auto wt = Wavetable::fromHarmonics (amps, name, "custom:" + name + "#" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
            spec.customWavetable[(size_t) osc] = wt;
            spec.wavetableId[(size_t) osc] = wt->sourceId;
            const auto modeId = Recipes::oscId (osc) + "_mode";
            const int mode = (int) spec.get (modeId);
            if (mode != 0 && mode != 4 && mode != 5)
            {
                spec.set (modeId, 0.0f);
                rep.warnings.add ("OSC " + o["osc"].toString() + " switched to Wavetable mode for the custom wavetable.");
            }
            ++rep.applied;
        }
        else if (kind == "sample" || kind == "multisample")
        {
            const int osc = oscFromLetter (o["osc"]);
            const bool multi = kind == "multisample";
            const auto name = matchName (multi ? BuiltinSamples::multiNames() : BuiltinSamples::singleNames(), o["name"].toString());
            if (osc < 0 || name.isEmpty()) { rep.errors.add ("Unknown built-in " + kind + " '" + o["name"].toString() + "'."); continue; }
            if (multi)
            {
                spec.multiId[(size_t) osc] = "builtin:" + name;
                if ((int) spec.get (Recipes::oscId (osc) + "_mode") != 3) spec.set (Recipes::oscId (osc) + "_mode", 3.0f);
            }
            else
            {
                spec.sampleId[(size_t) osc] = "builtin:" + name;
                spec.set (Recipes::oscId (osc) + "_source", 1.0f);
            }
            ++rep.applied;
        }
        else if (kind == "clear_sample")
        {
            const int osc = oscFromLetter (o["osc"]);
            if (osc < 0) { rep.errors.add ("clear_sample: unknown oscillator."); continue; }
            spec.sampleId[(size_t) osc].clear();
            spec.multiId[(size_t) osc].clear();
            ++rep.applied;
        }
        else if (kind == "fx_order")
        {
            auto* order = o["order"].getArray();
            if (order == nullptr) { rep.errors.add ("fx_order needs 'order'."); continue; }
            std::vector<int> listed;
            for (const auto& id : *order)
            {
                auto s = id.toString().trim().toLowerCase().removeCharacters (" _-");
                if (s.startsWith ("fx")) s = s.substring (2);
                const int t = choices::fxTypeIds.indexOf (s);
                if (t >= 0 && std::find (listed.begin(), listed.end(), t) == listed.end()) listed.push_back (t);
            }
            std::array<int, pid::NumFx> newOrder {};
            size_t n = 0;
            for (int t : listed) newOrder[n++] = t;
            for (int t : spec.fxOrder)
                if (std::find (listed.begin(), listed.end(), t) == listed.end()) newOrder[n++] = t;
            spec.fxOrder = newOrder;
            ++rep.applied;
        }
        else if (kind == "lfo_curve")
        {
            const int l = (int) o.getProperty ("lfo", 1) - 1;
            if (! juce::isPositiveAndBelow (l, kNumLfos)) { rep.errors.add ("lfo_curve: lfo must be 1..10."); continue; }
            auto c = LfoCurve::fromVar (o["points"]);
            spec.lfoCurves[(size_t) l] = c;
            ++rep.applied;
        }
        else if (kind == "seq_steps" || kind == "arp_steps")
        {
            auto* steps = o["steps"].getArray();
            if (steps == nullptr) { rep.errors.add (kind + " needs 'steps'."); continue; }
            for (int i = 0; i < steps->size() && i < 32; ++i)
            {
                const auto& s = (*steps)[i];
                if (kind == "seq_steps")
                {
                    auto& st = spec.seq.steps[(size_t) i];
                    st.on = (bool) s.getProperty ("on", true);
                    st.notes = { SeqStep::kNone, SeqStep::kNone, SeqStep::kNone, SeqStep::kNone };
                    if (auto* notes = s["notes"].getArray())
                        for (int k = 0; k < notes->size() && k < 4; ++k) st.notes[(size_t) k] = juce::jlimit (-48, 48, (int) (*notes)[k]);
                    if (st.notes[0] == SeqStep::kNone) st.notes[0] = 0;
                    st.velocity = juce::jlimit (0.0f, 1.0f, (float) s.getProperty ("velocity", 0.8f));
                    st.gate = juce::jlimit (0.05f, 2.0f, (float) s.getProperty ("gate", 0.5f));
                    st.probability = juce::jlimit (0.0f, 1.0f, (float) s.getProperty ("probability", 1.0f));
                    st.mod = juce::jlimit (-1.0f, 1.0f, (float) s.getProperty ("mod", 0.0f));
                    st.tie = (bool) s.getProperty ("tie", false);
                    st.ratchet = juce::jlimit (1, 4, (int) s.getProperty ("ratchet", 1));
                }
                else
                {
                    auto& st = spec.arp.steps[(size_t) i];
                    st.on = (bool) s.getProperty ("on", true);
                    st.velocity = juce::jlimit (0.0f, 1.0f, (float) s.getProperty ("velocity", 1.0f));
                    st.octave = juce::jlimit (-2, 2, (int) s.getProperty ("octave", 0));
                    st.gate = juce::jlimit (0.05f, 2.0f, (float) s.getProperty ("gate", 1.0f));
                    st.ratchet = juce::jlimit (1, 4, (int) s.getProperty ("ratchet", 1));
                }
            }
            if (kind == "arp_steps") spec.arp.length = juce::jlimit (1, 32, steps->size());
            else spec.set ("seq_length", (float) juce::jlimit (1, 32, steps->size()));
            ++rep.applied;
        }
        else
            rep.errors.add ("Unknown operation '" + kind + "' (ignored).");
    }
    return rep;
}

DesignResult PatchOps::fromResponse (const juce::var& response, const PatchSpec& current, const juce::String& prompt)
{
    DesignResult r;
    r.patch = current;
    r.reply = response["reply"].toString();
    if (containsHebrew (prompt) || containsHebrew (juce::JSON::toString (response)))
    {
        r.report.errors.add ("AI supports English only.");
        r.reply = "The AI response must be in English. No changes were made.";
        return r;
    }
    const auto action = response["action"].toString();
    const bool changesPatch = action == "new_patch" || action == "modify_patch";
    const bool opsMissing = response["operations"].getArray() == nullptr;
    // operations may be omitted only by answers / questions (some local models drop empty arrays)
    if (! response.isObject() || ! response["reply"].isString() || ! response["action"].isString()
        || (opsMissing && (changesPatch || ! response["operations"].isVoid()))
        || (! changesPatch && action != "answer_only" && action != "ask_question"))
    {
        r.report.errors.add ("Malformed AI response: invalid action, reply or operations.");
        r.reply = "The AI returned an invalid response. No changes were made.";
        return r;
    }
    r.question = response["question"].toString();
    if (auto* d = response["descriptors"].getArray())
        for (const auto& s : *d) r.descriptors.add (s.toString().toLowerCase());

    if (action == "answer_only" || action == "ask_question")
    {
        if (action == "ask_question" && r.question.isEmpty()) r.question = r.reply;
        return r;
    }

    PatchSpec spec = action == "new_patch" ? initPatch() : current;
    if (action == "new_patch")
    {
        spec.meta.name = "AI Sound";
        spec.meta.category = "Digital";
    }
    r.report = apply (spec, response["operations"]);
    if (! r.report.errors.isEmpty())
    {
        // Work on a copy and reject the transaction atomically. A valid prefix
        // must not change the user's patch when another operation is invalid.
        r.report.applied = 0;
        r.reply = "The AI returned invalid patch operations. No changes were made.";
        return r;
    }
    if (response["name"].toString().isNotEmpty()) spec.meta.name = response["name"].toString().substring (0, 60);
    if (response["category"].toString().isNotEmpty()) spec.meta.category = response["category"].toString().substring (0, 40);
    if (response["description"].toString().isNotEmpty()) spec.meta.description = response["description"].toString().substring (0, 400);
    if (auto* tags = response["tags"].getArray())
    {
        spec.meta.tags.clear();
        for (int i = 0; i < tags->size() && i < 12; ++i) spec.meta.tags.add ((*tags)[i].toString().substring (0, 24));
    }
    spec.meta.source = "ai";
    spec.meta.author = "AI Sound Designer";
    spec.meta.prompt = prompt;
    spec.meta.uuid = juce::Uuid().toString();
    r.patch = spec;
    r.hasPatch = r.report.applied > 0 || action == "new_patch";

    if (auto* vars = response["variations"].getArray())
        for (int i = 0; i < vars->size() && i < 8; ++i)
        {
            PatchSpec v = spec;
            if (! (*vars)[i].isObject() || (*vars)[i]["operations"].getArray() == nullptr)
            {
                r.report.errors.add ("Variation " + juce::String (i + 1) + ": invalid operations list.");
                continue;
            }
            auto rep = apply (v, (*vars)[i]["operations"]);
            if (! rep.errors.isEmpty())
            {
                for (const auto& e : rep.errors) r.report.errors.add ("Variation " + juce::String (i + 1) + ": " + e);
                continue;
            }
            v.meta.name = (*vars)[i]["name"].toString().isNotEmpty() ? (*vars)[i]["name"].toString().substring (0, 60)
                                                                      : spec.meta.name + " Var " + juce::String (i + 1);
            v.meta.description = (*vars)[i]["description"].toString();
            v.meta.uuid = juce::Uuid().toString();
            r.variations.push_back (v);
        }
    return r;
}

} // namespace vsx

namespace vsx
{
DesignResult PatchOps::designWithRepair (AIProvider& provider, const AIRequest& req, const juce::String& apiKey,
                                         const PatchSpec& current, const juce::String& prompt, const std::atomic<bool>& cancel,
                                         AIResult& last, int& repairs)
{
    repairs = 0;
    last = provider.send (req, apiKey, cancel);
    if (! last.ok || cancel.load()) return {};
    auto dr = fromResponse (last.json, current, prompt);
    if (dr.hasPatch || dr.report.errors.isEmpty() || ! last.json["operations"].isArray()) return dr;

    // one repair round: the model sees its own answer and the validator's messages
    AIRequest fix = req;
    fix.history.push_back ({ "user", req.userMessage });
    fix.history.push_back ({ "assistant", last.raw.isNotEmpty() ? last.raw.substring (0, 6000) : juce::JSON::toString (last.json, true) });
    fix.userMessage = "TOOL RESULT validate: your operations were rejected, nothing was applied. Errors: "
                      + dr.report.errors.joinIntoString ("; ").substring (0, 800)
                      + ". Return the complete corrected response. Use op:set to change a parameter value; op:mod needs a source from "
                        "MOD SOURCES and a parameter id as dest; use only listed names.";
    auto second = provider.send (fix, apiKey, cancel);
    repairs = 1;
    if (! second.ok || cancel.load()) return dr;
    auto dr2 = fromResponse (second.json, current, prompt);
    last = second;
    if (! dr2.hasPatch && dr2.report.errors.isEmpty()) return dr; // e.g. answer_only: keep the first report
    return dr2;
}
} // namespace vsx
