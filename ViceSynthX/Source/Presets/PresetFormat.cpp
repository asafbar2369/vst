#include "PresetFormat.h"

namespace vsx
{
PatchSpec::PatchSpec()
{
    const auto& reg = ParamRegistry::get();
    for (int i = 0; i < reg.size(); ++i)
        values[(size_t) i] = reg[i].defaultValue;
    for (int i = 0; i < pid::NumFx; ++i) fxOrder[(size_t) i] = i;
    for (auto& w : wavetableId) w = "factory:Basic Shapes";
    seq = defaultSeqPattern();
    meta.uuid = juce::Uuid().toString();
    meta.created = meta.modified = juce::Time::getCurrentTime();
}

float PatchSpec::get (const juce::String& id) const
{
    const int idx = ParamRegistry::get().indexOf (id);
    return idx >= 0 ? values[(size_t) idx] : 0.0f;
}

bool PatchSpec::set (const juce::String& id, float value)
{
    const auto& reg = ParamRegistry::get();
    const int idx = reg.indexOf (id);
    if (idx < 0) { jassertfalse; return false; }
    values[(size_t) idx] = reg[idx].snap (value);
    return true;
}

bool PatchSpec::setChoice (const juce::String& id, const juce::String& choiceName)
{
    const auto& reg = ParamRegistry::get();
    const int idx = reg.indexOf (id);
    if (idx < 0) { jassertfalse; return false; }
    const int ci = reg[idx].choices.indexOf (choiceName, true);
    if (ci < 0) { jassertfalse; return false; }
    values[(size_t) idx] = (float) ci;
    return true;
}

int PatchSpec::findMod (int source, int dest) const
{
    for (size_t i = 0; i < mods.size(); ++i)
        if (mods[i].source == source && mods[i].dest == dest) return (int) i;
    return -1;
}

PatchSpec& PatchSpec::addMod (int source, const juce::String& destId, float amount, int polarity, int aux, float curve)
{
    const auto& reg = ParamRegistry::get();
    const int dest = reg.indexOf (destId);
    if (dest < 0 || ! reg[dest].modulatable || source <= 0 || source >= modsrc::Count) { jassertfalse; return *this; }
    int slot = findMod (source, dest);
    if (slot < 0)
    {
        if ((int) mods.size() >= kNumModSlots) return *this;
        mods.push_back ({});
        slot = (int) mods.size() - 1;
    }
    auto& m = mods[(size_t) slot];
    m.source = source; m.dest = dest; m.polarity = polarity; m.aux = aux; m.curve = curve;
    m.amount = juce::jlimit (-1.0f, 1.0f, amount);
    values[(size_t) pid::MODAMT (slot)] = m.amount;
    return *this;
}

PatchSpec& PatchSpec::addMod (const juce::String& source, const juce::String& destId, float amount, int polarity)
{
    return addMod (modsrc::fromId (source), destId, amount, polarity);
}

void PatchSpec::removeMod (int source, int dest)
{
    const int slot = findMod (source, dest);
    if (slot < 0) return;
    mods.erase (mods.begin() + slot);
    // re-pack amounts into their slots
    for (int i = 0; i < kNumModSlots; ++i)
        values[(size_t) pid::MODAMT (i)] = i < (int) mods.size() ? mods[(size_t) i].amount : 0.0f;
}

// ---------------------------------------------------------------------------------
juce::var PatchSpec::stateToVar() const
{
    const auto& reg = ParamRegistry::get();
    auto* root = new juce::DynamicObject();

    juce::Array<juce::var> m;
    for (size_t i = 0; i < mods.size() && i < (size_t) kNumModSlots; ++i)
    {
        const auto& r = mods[i];
        if (r.source <= 0 || r.dest < 0) continue;
        auto* o = new juce::DynamicObject();
        o->setProperty ("slot", (int) i);
        o->setProperty ("source", modsrc::id (r.source));
        o->setProperty ("dest", reg[r.dest].id);
        o->setProperty ("polarity", r.polarity == 1 ? "bipolar" : r.polarity == 2 ? "unipolar" : "native");
        if (r.aux > 0) o->setProperty ("aux", modsrc::id (r.aux));
        if (std::abs (r.curve) > 1.0e-4f) o->setProperty ("curve", r.curve);
        m.add (juce::var (o));
    }
    root->setProperty ("modMatrix", m);

    juce::Array<juce::var> order;
    for (int t : fxOrder) order.add (choices::fxTypeIds[t]);
    root->setProperty ("fxOrder", order);

    juce::Array<juce::var> oscs;
    for (int o = 0; o < kNumOscs; ++o)
    {
        auto* d = new juce::DynamicObject();
        d->setProperty ("wavetable", wavetableId[(size_t) o]);
        if (sampleId[(size_t) o].isNotEmpty()) d->setProperty ("sample", sampleId[(size_t) o]);
        if (multiId[(size_t) o].isNotEmpty()) d->setProperty ("multisample", multiId[(size_t) o]);
        if (const auto& wt = customWavetable[(size_t) o]; wt != nullptr && wavetableId[(size_t) o].startsWith ("custom:"))
        {
            juce::MemoryOutputStream mo;
            for (int f = 0; f < wt->numFrames; ++f)
                for (float v : wt->rawFrame (f)) mo.writeFloat (v);
            d->setProperty ("wavetableFrames", wt->numFrames);
            d->setProperty ("wavetableData", juce::Base64::toBase64 (mo.getData(), mo.getDataSize()));
            d->setProperty ("wavetableName", wt->name);
        }
        oscs.add (juce::var (d));
    }
    root->setProperty ("oscillators", oscs);

    juce::Array<juce::var> curves;
    for (const auto& c : lfoCurves) curves.add (c.toVar());
    root->setProperty ("lfoCurves", curves);

    auto* a = new juce::DynamicObject();
    a->setProperty ("length", arp.length);
    juce::Array<juce::var> asteps;
    for (const auto& st : arp.steps) asteps.add (juce::Array<juce::var> { st.on, st.velocity, st.octave, st.gate });
    a->setProperty ("steps", asteps);
    root->setProperty ("arpPattern", juce::var (a));

    juce::Array<juce::var> ssteps;
    for (const auto& st : seq.steps)
    {
        auto* s = new juce::DynamicObject();
        s->setProperty ("on", st.on);
        juce::Array<juce::var> notes;
        for (int n : st.notes) if (n != SeqStep::kNone) notes.add (n);
        s->setProperty ("notes", notes);
        s->setProperty ("velocity", st.velocity);
        s->setProperty ("gate", st.gate);
        s->setProperty ("probability", st.probability);
        s->setProperty ("mod", st.mod);
        s->setProperty ("tie", st.tie);
        ssteps.add (juce::var (s));
    }
    root->setProperty ("seqPattern", ssteps);
    if (userIr.isNotEmpty()) root->setProperty ("userImpulse", userIr);
    return juce::var (root);
}

void PatchSpec::stateFromVar (const juce::var& v)
{
    const auto& reg = ParamRegistry::get();
    mods.clear();
    if (auto* arr = v["modMatrix"].getArray())
    {
        std::vector<std::pair<int, ModRoute>> bySlot;
        for (const auto& m : *arr)
        {
            ModRoute r;
            r.source = modsrc::fromId (m["source"].toString());
            r.dest = reg.indexOf (m["dest"].toString());
            if (r.source <= 0 || r.dest < 0 || ! reg[r.dest].modulatable) continue;
            const auto pol = m["polarity"].toString();
            r.polarity = pol == "bipolar" ? 1 : pol == "unipolar" ? 2 : 0;
            r.aux = std::max (0, modsrc::fromId (m["aux"].toString()));
            r.curve = juce::jlimit (-1.0f, 1.0f, (float) m.getProperty ("curve", 0.0f));
            bySlot.push_back ({ (int) m.getProperty ("slot", (int) bySlot.size()), r });
        }
        std::sort (bySlot.begin(), bySlot.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
        // keep slot positions so amounts (parameters) stay aligned
        for (const auto& [slot, r] : bySlot)
        {
            if (slot < 0 || slot >= kNumModSlots) continue;
            while ((int) mods.size() < slot) mods.push_back ({});
            if ((int) mods.size() == slot) mods.push_back (r);
        }
        for (size_t i = 0; i < mods.size(); ++i)
            mods[i].amount = values[(size_t) pid::MODAMT ((int) i)];
    }

    for (int i = 0; i < pid::NumFx; ++i) fxOrder[(size_t) i] = i;
    if (auto* order = v["fxOrder"].getArray(); order != nullptr && order->size() == pid::NumFx)
    {
        std::array<int, pid::NumFx> o {};
        std::array<bool, pid::NumFx> seen {};
        bool ok = true;
        for (int i = 0; i < pid::NumFx && ok; ++i)
        {
            const int t = choices::fxTypeIds.indexOf ((*order)[i].toString());
            if (t < 0 || seen[(size_t) t]) ok = false;
            else { seen[(size_t) t] = true; o[(size_t) i] = t; }
        }
        if (ok) fxOrder = o;
    }

    if (auto* oscs = v["oscillators"].getArray())
        for (int i = 0; i < kNumOscs && i < oscs->size(); ++i)
        {
            const auto& d = (*oscs)[i];
            wavetableId[(size_t) i] = d["wavetable"].toString().isNotEmpty() ? d["wavetable"].toString() : juce::String ("factory:Basic Shapes");
            sampleId[(size_t) i] = d["sample"].toString();
            multiId[(size_t) i] = d["multisample"].toString();
            customWavetable[(size_t) i].reset();
            if (wavetableId[(size_t) i].startsWith ("custom:") && d.hasProperty ("wavetableData"))
            {
                juce::MemoryOutputStream mo;
                juce::Base64::convertFromBase64 (mo, d["wavetableData"].toString());
                const int nf = (int) d["wavetableFrames"];
                if (nf > 0 && mo.getDataSize() == (size_t) nf * Wavetable::kSize * sizeof (float))
                {
                    juce::MemoryInputStream mi (mo.getData(), mo.getDataSize(), false);
                    std::vector<std::vector<float>> frames ((size_t) nf, std::vector<float> (Wavetable::kSize));
                    for (auto& fr : frames) for (auto& s : fr) s = mi.readFloat();
                    customWavetable[(size_t) i] = Wavetable::fromFrames (frames, d["wavetableName"].toString(), wavetableId[(size_t) i], true);
                }
                else
                    wavetableId[(size_t) i] = "factory:Basic Shapes";
            }
        }

    if (auto* curves = v["lfoCurves"].getArray())
        for (int l = 0; l < kNumLfos; ++l)
            lfoCurves[(size_t) l] = l < curves->size() ? LfoCurve::fromVar ((*curves)[l]) : LfoCurve();

    if (v.hasProperty ("arpPattern"))
    {
        arp = ArpPattern();
        arp.length = juce::jlimit (1, 32, (int) v["arpPattern"].getProperty ("length", 16));
        if (auto* steps = v["arpPattern"]["steps"].getArray())
            for (int i = 0; i < 32 && i < steps->size(); ++i)
                if (auto* s = (*steps)[i].getArray(); s != nullptr && s->size() >= 4)
                    arp.steps[(size_t) i] = { (bool) (*s)[0], juce::jlimit (0.0f, 1.0f, (float) (*s)[1]),
                                              juce::jlimit (-2, 2, (int) (*s)[2]), juce::jlimit (0.0f, 2.0f, (float) (*s)[3]) };
    }
    if (auto* steps = v["seqPattern"].getArray())
    {
        seq = SeqPattern();
        for (int i = 0; i < 32 && i < steps->size(); ++i)
        {
            const auto& s = (*steps)[i];
            auto& st = seq.steps[(size_t) i];
            st.on = (bool) s.getProperty ("on", false);
            st.notes = { SeqStep::kNone, SeqStep::kNone, SeqStep::kNone, SeqStep::kNone };
            if (auto* notes = s["notes"].getArray())
                for (int n = 0; n < 4 && n < notes->size(); ++n) st.notes[(size_t) n] = juce::jlimit (-48, 48, (int) (*notes)[n]);
            if (st.notes[0] == SeqStep::kNone) st.notes[0] = 0;
            st.velocity = juce::jlimit (0.0f, 1.0f, (float) s.getProperty ("velocity", 0.8f));
            st.gate = juce::jlimit (0.05f, 2.0f, (float) s.getProperty ("gate", 0.5f));
            st.probability = juce::jlimit (0.0f, 1.0f, (float) s.getProperty ("probability", 1.0f));
            st.mod = juce::jlimit (-1.0f, 1.0f, (float) s.getProperty ("mod", 0.0f));
            st.tie = (bool) s.getProperty ("tie", false);
        }
    }
    userIr = v["userImpulse"].toString();
}

// ---------------------------------------------------------------------------------
juce::var PatchSpec::toPresetVar() const
{
    const auto& reg = ParamRegistry::get();
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", PresetFormat::kFormatId);
    root->setProperty ("version", PresetFormat::kVersion);

    auto* m = new juce::DynamicObject();
    m->setProperty ("name", meta.name);
    m->setProperty ("author", meta.author);
    m->setProperty ("category", meta.category);
    juce::Array<juce::var> tags;
    for (const auto& t : meta.tags) tags.add (t);
    m->setProperty ("tags", tags);
    m->setProperty ("description", meta.description);
    m->setProperty ("source", meta.source);
    if (meta.prompt.isNotEmpty()) m->setProperty ("prompt", meta.prompt);
    m->setProperty ("uuid", meta.uuid);
    m->setProperty ("created", meta.created.toISO8601 (true));
    m->setProperty ("modified", meta.modified.toISO8601 (true));
    root->setProperty ("meta", juce::var (m));

    auto* params = new juce::DynamicObject();
    for (const auto& d : reg.all())
    {
        const float v = values[(size_t) d.index];
        switch (d.type)
        {
            case ParamType::Choice: params->setProperty (d.id, d.choices[juce::jlimit (0, d.choices.size() - 1, (int) std::round (v))]); break;
            case ParamType::Bool: params->setProperty (d.id, v >= 0.5f); break;
            case ParamType::Int: params->setProperty (d.id, (int) std::round (v)); break;
            case ParamType::Float: params->setProperty (d.id, (double) std::round (v * 1.0e5f) / 1.0e5); break;
        }
    }
    root->setProperty ("params", juce::var (params));
    root->setProperty ("state", stateToVar());
    return juce::var (root);
}

bool PatchSpec::fromPresetVar (const juce::var& v, PatchSpec& out, juce::String& error)
{
    if (! v.isObject())
    {
        error = "Preset is not a JSON object.";
        return false;
    }
    const auto fmt = v["format"].toString();
    if (fmt.isNotEmpty() && fmt != PresetFormat::kFormatId)
    {
        error = "Not a VICE SYNTH X preset (format '" + fmt + "').";
        return false;
    }
    if (! v["params"].isObject())
    {
        error = "Preset has no parameter section.";
        return false;
    }
    if ((int) v.getProperty ("version", 1) > PresetFormat::kVersion)
        error = "Preset was saved by a newer version; unknown settings were ignored.";

    const auto& reg = ParamRegistry::get();
    PatchSpec p;
    const auto& mv = v["meta"];
    p.meta.name = mv.getProperty ("name", "Untitled").toString();
    p.meta.author = mv.getProperty ("author", "").toString();
    p.meta.category = mv.getProperty ("category", "").toString();
    p.meta.description = mv.getProperty ("description", "").toString();
    p.meta.source = mv.getProperty ("source", "user").toString();
    p.meta.prompt = mv.getProperty ("prompt", "").toString();
    p.meta.uuid = mv.getProperty ("uuid", juce::Uuid().toString()).toString();
    p.meta.created = juce::Time::fromISO8601 (mv["created"].toString());
    p.meta.modified = juce::Time::fromISO8601 (mv["modified"].toString());
    if (auto* tags = mv["tags"].getArray())
        for (const auto& t : *tags) p.meta.tags.add (t.toString());

    if (auto* obj = v["params"].getDynamicObject())
    {
        for (const auto& prop : obj->getProperties())
        {
            const int idx = reg.indexOf (prop.name.toString());
            if (idx < 0) continue;
            const auto& d = reg[idx];
            const auto& val = prop.value;
            float f = d.defaultValue;
            if (val.isString())
            {
                if (d.type == ParamType::Choice)
                {
                    const int ci = d.choices.indexOf (val.toString(), true);
                    f = ci >= 0 ? (float) ci : d.defaultValue;
                }
                else if (d.type == ParamType::Bool)
                    f = (val.toString().equalsIgnoreCase ("true") || val.toString().equalsIgnoreCase ("on")) ? 1.0f : 0.0f;
                else
                    f = val.toString().getFloatValue();
            }
            else if (val.isBool())
                f = (bool) val ? 1.0f : 0.0f;
            else if (val.isDouble() || val.isInt() || val.isInt64())
                f = (float) (double) val;
            if (! std::isfinite (f)) f = d.defaultValue;
            p.values[(size_t) idx] = d.snap (f);
        }
    }
    p.stateFromVar (v["state"]);
    out = std::move (p);
    return true;
}

juce::String PatchSpec::toJson() const
{
    return juce::JSON::toString (toPresetVar(), false, 6);
}

bool PatchSpec::fromJson (const juce::String& json, PatchSpec& out, juce::String& error)
{
    juce::var v;
    const auto res = juce::JSON::parse (json, v);
    if (res.failed())
    {
        error = "Invalid JSON: " + res.getErrorMessage();
        return false;
    }
    return fromPresetVar (v, out, error);
}

juce::String PatchSpec::summarise (int maxItems) const
{
    const auto& reg = ParamRegistry::get();
    juce::StringArray lines;
    for (const auto& d : reg.all())
    {
        if (d.group == "Matrix") continue;
        const float v = values[(size_t) d.index];
        if (std::abs (v - d.defaultValue) > 1.0e-4f * std::max (1.0f, std::abs (d.maxValue - d.minValue)))
            lines.add (d.id + "=" + ParamRegistry::formatValue (d, v));
        if (lines.size() >= maxItems) break;
    }
    juce::String s = "Non-default parameters: " + (lines.isEmpty() ? juce::String ("(none)") : lines.joinIntoString (", "));
    if (! mods.empty())
    {
        juce::StringArray ms;
        for (size_t i = 0; i < mods.size(); ++i)
            if (mods[i].source > 0 && mods[i].dest >= 0)
                ms.add (modsrc::id (mods[i].source) + "->" + reg[mods[i].dest].id + " " + juce::String (values[(size_t) pid::MODAMT ((int) i)], 2));
        s << "\nModulation: " << ms.joinIntoString (", ");
    }
    juce::StringArray res;
    for (int o = 0; o < kNumOscs; ++o)
    {
        juce::String r = "OSC " + juce::String::charToString ((juce::juce_wchar) ('A' + o)) + " wavetable=" + wavetableId[(size_t) o];
        if (sampleId[(size_t) o].isNotEmpty()) r << " sample=" << sampleId[(size_t) o];
        if (multiId[(size_t) o].isNotEmpty()) r << " multisample=" << multiId[(size_t) o];
        res.add (r);
    }
    s << "\nResources: " << res.joinIntoString ("; ");
    juce::StringArray order;
    for (int t : fxOrder) order.add (choices::fxTypeIds[t]);
    s << "\nFX order: " << order.joinIntoString (" > ");
    return s;
}

} // namespace vsx
