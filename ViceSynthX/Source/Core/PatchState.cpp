#include "PatchState.h"

namespace vsx
{
// ---------------------------------------------------------------------------------
float LfoCurve::evaluate (float x) const
{
    if (points.empty()) return 0.0f;
    if (x <= points.front().x) return points.front().y;
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        const auto& a = points[i];
        const auto& b = points[i + 1];
        if (x <= b.x)
        {
            const float span = std::max (1.0e-6f, b.x - a.x);
            float t = (x - a.x) / span;
            if (std::abs (a.tension) > 1.0e-3f)
            {
                const float k = a.tension * 8.0f;
                t = (std::exp (k * t) - 1.0f) / (std::exp (k) - 1.0f);
            }
            return a.y + (b.y - a.y) * t;
        }
    }
    return points.back().y;
}

juce::var LfoCurve::toVar() const
{
    juce::Array<juce::var> arr;
    for (const auto& p : points)
        arr.add (juce::Array<juce::var> { p.x, p.y, p.tension });
    return arr;
}

LfoCurve LfoCurve::fromVar (const juce::var& v)
{
    LfoCurve c;
    if (auto* arr = v.getArray())
    {
        c.points.clear();
        for (const auto& p : *arr)
            if (auto* pa = p.getArray(); pa != nullptr && pa->size() >= 2)
                c.points.push_back ({ juce::jlimit (0.0f, 1.0f, (float) (*pa)[0]), juce::jlimit (-1.0f, 1.0f, (float) (*pa)[1]),
                                      pa->size() > 2 ? juce::jlimit (-1.0f, 1.0f, (float) (*pa)[2]) : 0.0f });
        std::sort (c.points.begin(), c.points.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });
        if (c.points.size() < 2)
            c = LfoCurve();
    }
    return c;
}

SeqPattern defaultSeqPattern()
{
    SeqPattern sp;
    const int defaultNotes[] = { 0, 0, 12, 0, 7, 0, 10, 12, 0, 0, 12, 0, 7, 3, 5, 7 };
    for (int i = 0; i < 16; ++i)
    {
        auto& st = sp.steps[(size_t) i];
        st.on = (i % 2 == 0) || i == 7 || i == 15;
        st.notes = { defaultNotes[i], SeqStep::kNone, SeqStep::kNone, SeqStep::kNone };
        st.velocity = (i % 4 == 0) ? 1.0f : 0.7f;
        st.mod = std::sin ((float) i / 16.0f * kTwoPi);
    }
    return sp;
}

// ---------------------------------------------------------------------------------
PatchState::PatchState()
{
    resetToDefaults();
}

PatchState::~PatchState()
{
    alive->store (false);
    loader.removeAllJobs (true, -1);
    cancelPendingUpdate();
}

void PatchState::resetToDefaults()
{
    for (auto& revisions : resourceRevision) for (auto& r : revisions) ++r;
    for (auto& s : slots)
    {
        s.source = modsrc::None; s.dest = -1; s.polarity = ModSlotState::Native; s.aux = modsrc::None; s.curve = 0.0f;
    }
    for (int i = 0; i < pid::NumFx; ++i)
        fxOrder[(size_t) i] = i;
    for (int l = 0; l < kNumLfos; ++l)
        setLfoCurve (l, LfoCurve());

    ArpPattern ap;
    setArpPattern (ap);

    setSeqPattern (defaultSeqPattern());

    userIrPath.clear();
    for (int o = 0; o < kNumOscs; ++o)
    {
        osc[(size_t) o].sampleId.clear();
        osc[(size_t) o].multiId.clear();
        osc[(size_t) o].sample.set (nullptr);
        osc[(size_t) o].multi.set (nullptr);
        setWavetable (o, "factory:Basic Shapes", false);
    }
}

void PatchState::setLfoCurve (int lfo, const LfoCurve& curve)
{
    if (! juce::isPositiveAndBelow (lfo, kNumLfos)) return;
    lfoCurves[(size_t) lfo] = curve;
    auto table = std::make_shared<LfoTable>();
    for (int i = 0; i <= LfoTable::kSize; ++i)
        table->v[(size_t) i] = curve.evaluate ((float) i / (float) LfoTable::kSize);
    lfoTables[(size_t) lfo].set (table);
}

void PatchState::notifyChanged (int oscIndex)
{
    std::weak_ptr<std::atomic<bool>> weakAlive = alive;
    auto fn = [this, weakAlive, oscIndex]
    {
        if (auto a = weakAlive.lock(); a != nullptr && a->load() && onResourceChanged)
            onResourceChanged (oscIndex);
    };
    if (juce::MessageManager::existsAndIsCurrentThread())
        fn();
    else if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        juce::MessageManager::callAsync (fn);
}

bool PatchState::setWavetable (int o, const juce::String& sourceId, bool async)
{
    if (! juce::isPositiveAndBelow (o, kNumOscs)) return false;
    const auto revision = ++resourceRevision[(size_t) o][0];
    auto load = [this, o, sourceId, revision, async]
    {
        juce::String err;
        auto resource = WavetableLibrary::instance().load (sourceId, err);
        const bool ok = resource != nullptr;
        auto publish = [this, o, sourceId, revision, resource, err]
        {
            if (resourceRevision[(size_t) o][0] != revision) return;
            auto& target = osc[(size_t) o];
            if (resource != nullptr) { target.wavetable.set (resource); target.wavetableId = sourceId; }
            target.lastError = err;
            notifyChanged (o);
        };
        if (async) enqueueCompletion (std::move (publish)); else publish();
        return ok;
    };
    if (! async) return load();
    ++pendingLoads;
    loader.addJob ([load] { load(); });
    return true;
}

bool PatchState::setWavetableObject (int o, std::shared_ptr<const Wavetable> wt)
{
    if (! juce::isPositiveAndBelow (o, kNumOscs) || wt == nullptr) return false;
    ++resourceRevision[(size_t) o][0];
    WavetableLibrary::instance().storeCustom (wt);
    osc[(size_t) o].wavetableId = wt->sourceId;
    osc[(size_t) o].wavetable.set (wt);
    notifyChanged (o);
    return true;
}

bool PatchState::setSample (int o, const juce::String& sourceId, bool async, std::function<void (bool)> done)
{
    if (! juce::isPositiveAndBelow (o, kNumOscs)) return false;
    const auto revision = ++resourceRevision[(size_t) o][1];
    auto load = [this, o, sourceId, revision, async, done]
    {
        juce::String err;
        auto resource = SampleLibrary::instance().loadSample (sourceId, err);
        const bool ok = resource != nullptr;
        auto publish = [this, o, sourceId, revision, resource, err, done]
        {
            if (resourceRevision[(size_t) o][1] != revision) return;
            auto& target = osc[(size_t) o];
            if (resource != nullptr)
            {
                target.sample.set (resource);
                target.sampleId = sourceId;
            }
            target.lastError = err;
            if (done) done (resource != nullptr);
            notifyChanged (o);
        };
        if (async) enqueueCompletion (std::move (publish)); else publish();
        return ok;
    };
    if (! async) return load();
    ++pendingLoads;
    loader.addJob ([load] { load(); });
    return true;
}

bool PatchState::setMultiSample (int o, const juce::String& sourceId, bool async, std::function<void (bool)> done)
{
    if (! juce::isPositiveAndBelow (o, kNumOscs)) return false;
    const auto revision = ++resourceRevision[(size_t) o][2];
    auto load = [this, o, sourceId, revision, async, done]
    {
        juce::String err;
        auto resource = SampleLibrary::instance().loadMulti (sourceId, err);
        const bool ok = resource != nullptr;
        auto publish = [this, o, sourceId, revision, resource, err, done]
        {
            if (resourceRevision[(size_t) o][2] != revision) return;
            auto& target = osc[(size_t) o];
            if (resource != nullptr)
            {
                target.multi.set (resource);
                target.multiId = sourceId;
            }
            target.lastError = err;
            if (done) done (resource != nullptr);
            notifyChanged (o);
        };
        if (async) enqueueCompletion (std::move (publish)); else publish();
        return ok;
    };
    if (! async) return load();
    ++pendingLoads;
    loader.addJob ([load] { load(); });
    return true;
}


void PatchState::clearSample (int o)
{
    if (! juce::isPositiveAndBelow (o, kNumOscs)) return;
    ++resourceRevision[(size_t) o][1];
    osc[(size_t) o].sampleId.clear();
    osc[(size_t) o].sample.set (nullptr);
    notifyChanged (o);
}

void PatchState::waitForLoads (int timeoutMs)
{
    const auto start = juce::Time::getMillisecondCounter();
    while (pendingLoads.load() > 0 && (int) (juce::Time::getMillisecondCounter() - start) < timeoutMs)
    {
        handleAsyncUpdate();
        if (pendingLoads.load() > 0) juce::Thread::sleep (5);
    }
}

void PatchState::enqueueCompletion (std::function<void()> completion)
{
    {
        std::lock_guard<std::mutex> lock (completionMutex);
        completions.push_back (std::move (completion));
    }
    triggerAsyncUpdate();
}

void PatchState::handleAsyncUpdate()
{
    std::vector<std::function<void()>> ready;
    {
        std::lock_guard<std::mutex> lock (completionMutex);
        ready.swap (completions);
    }
    for (auto& publish : ready) { publish(); --pendingLoads; }
}

void PatchState::collectGarbage()
{
    for (auto& o : osc)
    {
        o.wavetable.collectGarbage();
        o.sample.collectGarbage();
        o.multi.collectGarbage();
    }
    for (auto& t : lfoTables) t.collectGarbage();
    arpPattern.collectGarbage();
    seqPattern.collectGarbage();
}

// ---------------------------------------------------------------------------------
juce::var PatchState::toVar() const
{
    auto* root = new juce::DynamicObject();
    const auto& reg = ParamRegistry::get();

    juce::Array<juce::var> mods;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const auto& s = slots[(size_t) i];
        if (! s.active()) continue;
        auto* m = new juce::DynamicObject();
        m->setProperty ("slot", i);
        m->setProperty ("source", modsrc::id (s.source.load()));
        m->setProperty ("dest", reg[s.dest.load()].id);
        m->setProperty ("polarity", s.polarity.load() == ModSlotState::Bipolar ? "bipolar"
                                    : s.polarity.load() == ModSlotState::Unipolar ? "unipolar" : "native");
        if (s.aux.load() != modsrc::None) m->setProperty ("aux", modsrc::id (s.aux.load()));
        if (std::abs (s.curve.load()) > 1.0e-4f) m->setProperty ("curve", s.curve.load());
        mods.add (juce::var (m));
    }
    root->setProperty ("modMatrix", mods);

    juce::Array<juce::var> order;
    for (int i = 0; i < pid::NumFx; ++i)
        order.add (choices::fxTypeIds[fxOrder[(size_t) i].load()]);
    root->setProperty ("fxOrder", order);

    juce::Array<juce::var> oscs;
    for (const auto& o : osc)
    {
        auto* d = new juce::DynamicObject();
        d->setProperty ("wavetable", o.wavetableId);
        if (o.sampleId.isNotEmpty()) d->setProperty ("sample", o.sampleId);
        if (o.multiId.isNotEmpty()) d->setProperty ("multisample", o.multiId);
        if (auto wt = o.wavetable.getShared(); wt != nullptr && wt->sourceId.startsWith ("custom:"))
        {
            // embed custom (AI- or user-drawn) wavetables so presets stay portable
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

    {
        const auto ap = getArpPattern();
        auto* a = new juce::DynamicObject();
        a->setProperty ("length", ap.length);
        juce::Array<juce::var> steps;
        for (const auto& st : ap.steps)
            steps.add (juce::Array<juce::var> { st.on, st.velocity, st.octave, st.gate });
        a->setProperty ("steps", steps);
        root->setProperty ("arpPattern", juce::var (a));
    }
    {
        const auto sp = getSeqPattern();
        juce::Array<juce::var> steps;
        for (const auto& st : sp.steps)
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
            steps.add (juce::var (s));
        }
        root->setProperty ("seqPattern", steps);
    }
    if (userIrPath.isNotEmpty())
        root->setProperty ("userImpulse", userIrPath);
    return juce::var (root);
}

void PatchState::fromVar (const juce::var& v, bool asyncLoads)
{
    for (auto& revisions : resourceRevision) for (auto& r : revisions) ++r;
    for (auto& resource : osc)
    {
        resource.sampleId.clear(); resource.multiId.clear();
        resource.sample.set (nullptr); resource.multi.set (nullptr);
        resource.lastError.clear();
    }
    const auto& reg = ParamRegistry::get();

    for (auto& s : slots) { s.source = modsrc::None; s.dest = -1; s.polarity = 0; s.aux = modsrc::None; s.curve = 0.0f; }
    if (auto* mods = v["modMatrix"].getArray())
    {
        for (const auto& m : *mods)
        {
            const int slot = (int) m.getProperty ("slot", -1);
            const int src = modsrc::fromId (m["source"].toString());
            const int dest = reg.indexOf (m["dest"].toString());
            if (! juce::isPositiveAndBelow (slot, kNumModSlots) || src <= 0 || dest < 0 || ! reg[dest].modulatable)
                continue;
            auto& s = slots[(size_t) slot];
            const auto pol = m["polarity"].toString();
            s.polarity = pol == "bipolar" ? ModSlotState::Bipolar : pol == "unipolar" ? ModSlotState::Unipolar : ModSlotState::Native;
            const int aux = modsrc::fromId (m["aux"].toString());
            s.aux = aux > 0 ? aux : modsrc::None;
            s.curve = juce::jlimit (-1.0f, 1.0f, (float) m.getProperty ("curve", 0.0f));
            s.dest = dest;
            s.source = src;
        }
    }

    if (auto* order = v["fxOrder"].getArray(); order != nullptr && order->size() == pid::NumFx)
    {
        std::array<int, pid::NumFx> o {};
        std::array<bool, pid::NumFx> seen {};
        bool ok = true;
        for (int i = 0; i < pid::NumFx; ++i)
        {
            const int t = choices::fxTypeIds.indexOf ((*order)[i].toString());
            if (t < 0 || seen[(size_t) t]) { ok = false; break; }
            seen[(size_t) t] = true;
            o[(size_t) i] = t;
        }
        for (int i = 0; i < pid::NumFx; ++i) fxOrder[(size_t) i] = ok ? o[(size_t) i] : i;
    }
    else
        for (int i = 0; i < pid::NumFx; ++i) fxOrder[(size_t) i] = i;

    if (auto* oscs = v["oscillators"].getArray())
    {
        for (int i = 0; i < kNumOscs && i < oscs->size(); ++i)
        {
            const auto& d = (*oscs)[i];
            const auto wtId = d["wavetable"].toString();
            if (wtId.startsWith ("custom:") && d.hasProperty ("wavetableData"))
            {
                juce::MemoryOutputStream mo;
                juce::Base64::convertFromBase64 (mo, d["wavetableData"].toString());
                const int numFrames = (int) d["wavetableFrames"];
                const size_t expected = (size_t) numFrames * Wavetable::kSize * sizeof (float);
                if (numFrames > 0 && mo.getDataSize() == expected)
                {
                    juce::MemoryInputStream mi (mo.getData(), mo.getDataSize(), false);
                    std::vector<std::vector<float>> frames ((size_t) numFrames, std::vector<float> (Wavetable::kSize));
                    for (auto& fr : frames) for (auto& s : fr) s = mi.readFloat();
                    auto wt = Wavetable::fromFrames (frames, d["wavetableName"].toString(), wtId, true);
                    setWavetableObject (i, wt);
                }
                else
                    setWavetable (i, "factory:Basic Shapes", asyncLoads);
            }
            else
                setWavetable (i, wtId.isNotEmpty() ? wtId : "factory:Basic Shapes", asyncLoads);

            const auto sId = d["sample"].toString();
            if (sId.isNotEmpty()) { osc[(size_t) i].sampleId = sId; setSample (i, sId, asyncLoads); }
            else { osc[(size_t) i].sampleId.clear(); osc[(size_t) i].sample.set (nullptr); }

            const auto mId = d["multisample"].toString();
            if (mId.isNotEmpty()) { osc[(size_t) i].multiId = mId; setMultiSample (i, mId, asyncLoads); }
            else { osc[(size_t) i].multiId.clear(); osc[(size_t) i].multi.set (nullptr); }
        }
    }

    if (auto* curves = v["lfoCurves"].getArray())
        for (int l = 0; l < kNumLfos; ++l)
            setLfoCurve (l, l < curves->size() ? LfoCurve::fromVar ((*curves)[l]) : LfoCurve());

    if (v.hasProperty ("arpPattern"))
    {
        ArpPattern ap;
        ap.length = juce::jlimit (1, 32, (int) v["arpPattern"].getProperty ("length", 16));
        if (auto* steps = v["arpPattern"]["steps"].getArray())
            for (int i = 0; i < 32 && i < steps->size(); ++i)
                if (auto* s = (*steps)[i].getArray(); s != nullptr && s->size() >= 4)
                    ap.steps[(size_t) i] = { (bool) (*s)[0], juce::jlimit (0.0f, 1.0f, (float) (*s)[1]),
                                             juce::jlimit (-2, 2, (int) (*s)[2]), juce::jlimit (0.0f, 2.0f, (float) (*s)[3]) };
        setArpPattern (ap);
    }

    if (auto* steps = v["seqPattern"].getArray())
    {
        SeqPattern sp;
        for (int i = 0; i < 32 && i < steps->size(); ++i)
        {
            const auto& s = (*steps)[i];
            auto& st = sp.steps[(size_t) i];
            st.on = (bool) s.getProperty ("on", false);
            st.notes = { SeqStep::kNone, SeqStep::kNone, SeqStep::kNone, SeqStep::kNone };
            if (auto* notes = s["notes"].getArray())
                for (int n = 0; n < 4 && n < notes->size(); ++n)
                    st.notes[(size_t) n] = juce::jlimit (-48, 48, (int) (*notes)[n]);
            if (st.notes[0] == SeqStep::kNone) st.notes[0] = 0;
            st.velocity = juce::jlimit (0.0f, 1.0f, (float) s.getProperty ("velocity", 0.8f));
            st.gate = juce::jlimit (0.05f, 2.0f, (float) s.getProperty ("gate", 0.5f));
            st.probability = juce::jlimit (0.0f, 1.0f, (float) s.getProperty ("probability", 1.0f));
            st.mod = juce::jlimit (-1.0f, 1.0f, (float) s.getProperty ("mod", 0.0f));
            st.tie = (bool) s.getProperty ("tie", false);
        }
        setSeqPattern (sp);
    }

    userIrPath = v["userImpulse"].toString();
    ++irGeneration;
}

juce::StringArray PatchState::missingResources() const
{
    juce::StringArray missing;
    auto check = [&] (const juce::String& id, const juce::String& prefix)
    {
        if (id.startsWith (prefix))
        {
            const juce::File f (id.fromFirstOccurrenceOf (prefix, false, false));
            if (! f.exists()) missing.add (f.getFullPathName());
        }
    };
    for (const auto& o : osc)
    {
        check (o.wavetableId, "user:");
        check (o.sampleId, "file:");
        check (o.multiId, "sfz:");
        check (o.multiId, "folder:");
    }
    if (userIrPath.isNotEmpty() && ! juce::File (userIrPath).existsAsFile())
        missing.add (userIrPath);
    return missing;
}

} // namespace vsx
