#include "SynthPanels.h"

namespace vsx
{
using namespace pid;

namespace
{
    juce::String letter (int o) { return juce::String::charToString ((juce::juce_wchar) ('A' + o)); }

    void addKnobs (juce::Component& parent, std::vector<std::unique_ptr<ParamKnob>>& v, ViceSynthProcessor& p,
                   std::initializer_list<std::pair<int, const char*>> list, juce::Colour c)
    {
        for (const auto& [idx, label] : list)
        {
            v.push_back (std::make_unique<ParamKnob> (p, idx, label, c));
            parent.addAndMakeVisible (*v.back());
        }
    }

    void layoutKnobs (std::vector<std::unique_ptr<ParamKnob>>& v, juce::Rectangle<int> r, int slots)
    {
        const int w = r.getWidth() / std::max (1, slots);
        for (size_t i = 0; i < v.size(); ++i)
            v[i]->setBounds (r.getX() + (int) i * w, r.getY(), w, r.getHeight());
    }
}

// =================================================================================
OscPanel::OscPanel (ViceSynthProcessor& p, int o)
    : Section ("OSC " + letter (o), Theme::accentFor (o)), proc (p), osc (o),
      onButton (p, O (o, osc::On), "", Theme::accentFor (o)), mode (p, O (o, osc::Mode)), warpMode (p, O (o, osc::WarpMode)),
      xmodMode (p, O (o, osc::XModMode)), xmodSource (p, O (o, osc::XModSource)), route (p, O (o, osc::Route)),
      loopMode (p, O (o, osc::LoopMode)), grainWindow (p, O (o, osc::GrainWindow)), source (p, O (o, osc::Source)),
      display (p, o)
{
    setInterceptsMouseClicks (false, true);
    const auto c = Theme::accentFor (o);
    for (auto* comp : std::initializer_list<juce::Component*> { &onButton, &mode, &resourceButton, &display, &warpMode, &xmodMode, &xmodSource,
                                                               &route, &loopMode, &grainWindow, &source })
        addAndMakeVisible (comp);
    resourceButton.onClick = [this] { showResourceMenu(); };
    resourceButton.setTooltip ("Choose a wavetable, sample or multisample, or load your own files.");
    display.onOpenEditor = [this] { if (onOpenWavetableEditor) onOpenWavetableEditor (osc); };

    addKnobs (*this, row1, p, { { O (o, osc::Octave), "Oct" }, { O (o, osc::Semi), "Semi" }, { O (o, osc::Fine), "Fine" },
                                { O (o, osc::Position), "Pos" }, { O (o, osc::Level), "Level" }, { O (o, osc::Pan), "Pan" },
                                { O (o, osc::Phase), "Phase" }, { O (o, osc::PhaseRand), "Rand" } }, c);
    addKnobs (*this, row2, p, { { O (o, osc::Unison), "Unison" }, { O (o, osc::Detune), "Detune" }, { O (o, osc::Blend), "Blend" },
                                { O (o, osc::Spread), "Spread" }, { O (o, osc::WarpAmount), "Warp" }, { O (o, osc::XModAmount), "X-Mod" },
                                { O (o, osc::Fold), "Fold" } }, c);
    addKnobs (*this, rowAnalog, p, { { O (o, osc::PulseWidth), "PW" } }, c);
    addKnobs (*this, rowSample, p, { { O (o, osc::LoopStart), "Loop S" }, { O (o, osc::LoopEnd), "Loop E" }, { O (o, osc::LoopXfade), "X-Fade" },
                                     { O (o, osc::RootNote), "Root" } }, c);
    addKnobs (*this, rowGrain, p, { { O (o, osc::GrainSize), "Size" }, { O (o, osc::GrainDensity), "Density" }, { O (o, osc::GrainSpray), "Spray" },
                                    { O (o, osc::GrainPitchRand), "P.Rand" }, { O (o, osc::GrainSpread), "Spread" } }, c);
    addKnobs (*this, rowSpectral, p, { { O (o, osc::SpecTilt), "Tilt" }, { O (o, osc::SpecOddEven), "Odd/Ev" }, { O (o, osc::SpecStretch), "Stretch" },
                                       { O (o, osc::SpecFormant), "Formant" }, { O (o, osc::SpecPartials), "Partials" }, { O (o, osc::SpecSpeed), "Speed" } }, c);
    resourceChanged();
    updateModeRows();
}

void OscPanel::updateModeRows()
{
    const int m = (int) proc.getParam (O (osc, osc::Mode));
    lastMode = m;
    for (auto& k : rowAnalog) k->setVisible (m == 1);
    for (auto& k : rowSample) k->setVisible (m == 2 || m == 3);
    loopMode.setVisible (m == 2 || m == 3);
    for (auto& k : rowGrain) k->setVisible (m == 4);
    grainWindow.setVisible (m == 4);
    for (auto& k : rowSpectral) k->setVisible (m == 5);
    source.setVisible (m == 4 || m == 5);
    resized();
    resourceChanged();
}

void OscPanel::refresh()
{
    if ((int) proc.getParam (O (osc, osc::Mode)) != lastMode) updateModeRows();
    display.refresh();
}

void OscPanel::resourceChanged()
{
    auto& r = proc.getPatch().osc[(size_t) osc];
    const int m = (int) proc.getParam (O (osc, osc::Mode));
    juce::String text;
    if (m == 3) text = r.multiId.isNotEmpty() ? r.multiId.fromFirstOccurrenceOf (":", false, false).fromLastOccurrenceOf ("/", false, false) : "Choose multisample";
    else if (m == 2 || ((m == 4 || m == 5) && proc.getParam (O (osc, osc::Source)) > 0.5f))
        text = r.sampleId.isNotEmpty() ? r.sampleId.fromFirstOccurrenceOf (":", false, false).fromLastOccurrenceOf ("/", false, false) : "Choose sample";
    else if (m == 1) text = "Virtual analog";
    else
    {
        auto wt = r.wavetable.getShared();
        text = wt != nullptr ? wt->name : r.wavetableId.fromFirstOccurrenceOf (":", false, false);
    }
    resourceButton.setButtonText (text + "  \xe2\x96\xbe");
    display.repaint();
}

void OscPanel::showResourceMenu()
{
    auto& patch = proc.getPatch();
    juce::PopupMenu m, factory, user, samples, multis;
    const auto fNames = WavetableFactory::names();
    for (int i = 0; i < fNames.size(); ++i) factory.addItem (100 + i, fNames[i]);
    const auto uNames = WavetableLibrary::instance().userNames();
    for (int i = 0; i < uNames.size() && i < 200; ++i) user.addItem (400 + i, juce::File (uNames[i]).getFileNameWithoutExtension());
    if (uNames.isEmpty()) user.addItem (-1, "(Documents/VICE SYNTH X/Wavetables is empty)", false);
    const auto sNames = BuiltinSamples::singleNames();
    for (int i = 0; i < sNames.size(); ++i) samples.addItem (700 + i, sNames[i]);
    const auto mNames = BuiltinSamples::multiNames();
    for (int i = 0; i < mNames.size(); ++i) multis.addItem (800 + i, mNames[i]);
    m.addSubMenu ("Factory wavetables", factory);
    m.addSubMenu ("User wavetables", user);
    m.addSubMenu ("Built-in samples", samples);
    m.addSubMenu ("Built-in multisamples", multis);
    m.addSeparator();
    m.addItem (1, "Load audio file as wavetable...");
    m.addItem (2, "Load sample (WAV / AIFF / FLAC)...");
    m.addItem (3, "Load SFZ multisample...");
    m.addItem (4, "Load folder of samples as multisample...");
    m.addSeparator();
    m.addItem (5, "Edit / draw wavetable...");
    m.addItem (6, "Export wavetable as WAV...");
    m.addItem (7, "Clear sample");

    juce::Component::SafePointer<OscPanel> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&resourceButton), [safe, fNames, uNames, sNames, mNames] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& self = *safe;
        auto& pt = self.proc.getPatch();
        const int o = self.osc;
        const int curMode = (int) self.proc.getParam (O (o, osc::Mode));
        auto useSample = [&]
        {
            self.proc.setParam (O (o, osc::Source), 1.0f);
            if (curMode != 4 && curMode != 5) self.proc.setParam (O (o, osc::Mode), 2.0f);
        };
        if (r >= 100 && r < 400) { pt.setWavetable (o, "factory:" + fNames[r - 100], true); if (curMode != 0 && curMode != 4 && curMode != 5) self.proc.setParam (O (o, osc::Mode), 0.0f); }
        else if (r >= 400 && r < 700) { pt.setWavetable (o, "user:" + uNames[r - 400], true); if (curMode != 0 && curMode != 4 && curMode != 5) self.proc.setParam (O (o, osc::Mode), 0.0f); }
        else if (r >= 700 && r < 800) { pt.setSample (o, "builtin:" + sNames[r - 700], true); useSample(); }
        else if (r >= 800 && r < 900) { pt.setMultiSample (o, "builtin:" + mNames[r - 800], true); self.proc.setParam (O (o, osc::Mode), 3.0f); }
        else if (r == 5) { if (self.onOpenWavetableEditor) self.onOpenWavetableEditor (o); }
        else if (r == 7) pt.clearSample (o);
        else
        {
            const bool folder = r == 4;
            const bool save = r == 6;
            const juce::String pattern = r == 3 ? "*.sfz" : "*.wav;*.aif;*.aiff;*.flac";
            self.chooser = std::make_unique<juce::FileChooser> (save ? "Export wavetable" : "Choose file",
                                                                 save ? WavetableLibrary::instance().userFolder().getChildFile ("My Wavetable.wav") : juce::File(), pattern);
            const int flags = save ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting)
                                   : folder ? (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories)
                                            : (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles);
            self.chooser->launchAsync (flags, [safe, r] (const juce::FileChooser& fc)
            {
                if (safe == nullptr || fc.getResult() == juce::File()) return;
                auto& s2 = *safe;
                auto& p2 = s2.proc.getPatch();
                const auto f = fc.getResult();
                const int oo = s2.osc;
                if (r == 1) { p2.setWavetable (oo, "user:" + f.getFullPathName(), true); s2.proc.setParam (O (oo, osc::Mode), 0.0f); }
                else if (r == 2) { p2.setSample (oo, "file:" + f.getFullPathName(), true); s2.proc.setParam (O (oo, osc::Source), 1.0f); if ((int) s2.proc.getParam (O (oo, osc::Mode)) < 2) s2.proc.setParam (O (oo, osc::Mode), 2.0f); }
                else if (r == 3) { p2.setMultiSample (oo, "sfz:" + f.getFullPathName(), true); s2.proc.setParam (O (oo, osc::Mode), 3.0f); }
                else if (r == 4) { p2.setMultiSample (oo, "folder:" + f.getFullPathName(), true); s2.proc.setParam (O (oo, osc::Mode), 3.0f); }
                else if (r == 6) { if (auto wt = p2.osc[(size_t) oo].wavetable.getShared()) wt->exportToWav (f.withFileExtension ("wav")); }
            });
        }
    });
}

void OscPanel::resized()
{
    auto r = content();
    auto header = getLocalBounds().reduced (8, 4).removeFromTop (18);
    onButton.setBounds (header.removeFromLeft (70).withTrimmedLeft (52));
    header.removeFromLeft (4);
    mode.setBounds (r.getX() + 0, r.getY(), 104, 22);
    resourceButton.setBounds (r.getX() + 108, r.getY(), r.getWidth() - 108, 22);
    r.removeFromTop (26);
    display.setBounds (r.removeFromTop (84));
    r.removeFromTop (4);
    const int kh = 50;
    layoutKnobs (row1, r.removeFromTop (kh), 8);
    layoutKnobs (row2, r.removeFromTop (kh), 8);
    auto combos = r.removeFromTop (22);
    const int cw = combos.getWidth() / 4;
    warpMode.setBounds (combos.removeFromLeft (cw).reduced (1, 0));
    xmodMode.setBounds (combos.removeFromLeft (cw).reduced (1, 0));
    xmodSource.setBounds (combos.removeFromLeft (cw).reduced (1, 0));
    route.setBounds (combos.reduced (1, 0));
    r.removeFromTop (4);
    auto row3 = r.removeFromTop (kh);
    const int m = lastMode;
    if (m == 1) layoutKnobs (rowAnalog, row3, 8);
    else if (m == 2 || m == 3) { loopMode.setBounds (row3.removeFromLeft (row3.getWidth() / 4).reduced (2, 14)); layoutKnobs (rowSample, row3, 4); }
    else if (m == 4)
    {
        auto left = row3.removeFromLeft (row3.getWidth() / 4);
        source.setBounds (left.removeFromTop (row3.getHeight() / 2).reduced (2, 2));
        grainWindow.setBounds (left.reduced (2, 2));
        layoutKnobs (rowGrain, row3, 5);
    }
    else if (m == 5)
    {
        source.setBounds (row3.removeFromLeft (row3.getWidth() / 5).reduced (2, 14));
        layoutKnobs (rowSpectral, row3, 6);
    }
}

// =================================================================================
SubNoisePanel::SubNoisePanel (ViceSynthProcessor& p)
    : Section ("SUB  /  NOISE", Theme::electricPurple), subOn (p, SUB (sub::On), "SUB"), noiseOn (p, NOISE (noise::On), "NOISE"),
      subShape (p, SUB (sub::Shape)), subRoute (p, SUB (sub::Route)), noiseType (p, NOISE (noise::Type)), noiseRoute (p, NOISE (noise::Route))
{
    for (auto* c : std::initializer_list<juce::Component*> { &subOn, &noiseOn, &subShape, &subRoute, &noiseType, &noiseRoute }) addAndMakeVisible (c);
    addKnobs (*this, subKnobs, p, { { SUB (sub::Octave), "Oct" }, { SUB (sub::Level), "Level" }, { SUB (sub::Pan), "Pan" } }, Theme::electricPurple);
    addKnobs (*this, noiseKnobs, p, { { NOISE (noise::Level), "Level" }, { NOISE (noise::Color), "Color" }, { NOISE (noise::Pan), "Pan" },
                                      { NOISE (noise::KeyTrack), "Key" }, { NOISE (noise::Width), "Width" } }, Theme::electricPurple);
}

void SubNoisePanel::resized()
{
    auto r = content();
    auto subArea = r.removeFromLeft (r.getWidth() * 4 / 10);
    auto col = subArea.removeFromLeft (90);
    subOn.setBounds (col.removeFromTop (20));
    subShape.setBounds (col.removeFromTop (22).reduced (0, 1));
    subRoute.setBounds (col.removeFromTop (22).reduced (0, 1));
    layoutKnobs (subKnobs, subArea.reduced (4, 0), 3);
    r.removeFromLeft (8);
    auto col2 = r.removeFromLeft (90);
    noiseOn.setBounds (col2.removeFromTop (20));
    noiseType.setBounds (col2.removeFromTop (22).reduced (0, 1));
    noiseRoute.setBounds (col2.removeFromTop (22).reduced (0, 1));
    layoutKnobs (noiseKnobs, r.reduced (4, 0), 5);
}

// =================================================================================
FilterPanel::FilterPanel (ViceSynthProcessor& p, int f)
    : Section ("ANALOG FILTER " + juce::String (f + 1), f == 0 ? Theme::sunsetOrange : Theme::neonPink), proc (p), index (f),
      onButton (p, F (f, flt::On), "", Theme::sunsetOrange), model (p, F (f, flt::Model)), mode (p, F (f, flt::Mode)), display (p, f)
{
    for (auto* c : std::initializer_list<juce::Component*> { &onButton, &model, &mode, &display }) addAndMakeVisible (c);
    if (f == 0)
    {
        routing = std::make_unique<ParamCombo> (p, G (global::FilterRouting));
        addAndMakeVisible (*routing);
    }
    const auto c = f == 0 ? Theme::sunsetOrange : Theme::neonPink;
    addKnobs (*this, knobs, p, { { F (f, flt::Cutoff), "Cutoff" }, { F (f, flt::Resonance), "Reso" }, { F (f, flt::Drive), "Drive" },
                                 { F (f, flt::EnvAmount), "Env" }, { F (f, flt::KeyTrack), "Key" }, { F (f, flt::Drift), "Drift" },
                                 { F (f, flt::Tolerance), "Toler." }, { F (f, flt::Character), "Char" }, { F (f, flt::Mix), "Mix" },
                                 { F (f, flt::Gain), "Gain" } }, c);
    model.setTooltip ("Filter model. " + choices::filterModelDescription (0));
    model.onChange = [this] { model.setTooltip (choices::filterModelDescription (model.getSelectedItemIndex())); };
}

void FilterPanel::resized()
{
    auto r = content();
    auto header = getLocalBounds().reduced (8, 4).removeFromTop (18);
    onButton.setBounds (header.removeFromLeft (150).withTrimmedLeft (128));
    auto top = r.removeFromTop (22);
    model.setBounds (top.removeFromLeft (150));
    top.removeFromLeft (4);
    mode.setBounds (top.removeFromLeft (110));
    if (routing != nullptr) { top.removeFromLeft (4); routing->setBounds (top.removeFromRight (100)); }
    r.removeFromTop (4);
    display.setBounds (r.removeFromLeft (190).reduced (0, 2));
    r.removeFromLeft (6);
    const int h = r.getHeight() / 2;
    std::vector<std::unique_ptr<ParamKnob>>& k = knobs;
    const int w = r.getWidth() / 5;
    for (size_t i = 0; i < k.size(); ++i)
        k[i]->setBounds (r.getX() + (int) (i % 5) * w, r.getY() + (int) (i / 5) * h, w, h);
}

// =================================================================================
ModulationSection::ModulationSection (ViceSynthProcessor& p) : proc (p), envEditor (p, 0), lfoEditor (p, 0)
{
    addAndMakeVisible (envBox);
    addAndMakeVisible (lfoBox);
    addAndMakeVisible (macroBox);
    addAndMakeVisible (envEditor);
    addAndMakeVisible (lfoEditor);
    for (int e = 0; e < kNumEnvs; ++e)
    {
        auto* b = envTabs.add (new juce::TextButton (juce::String (e + 1)));
        b->setClickingTogglesState (false);
        b->onClick = [this, e] { selectEnv (e); };
        addAndMakeVisible (b);
        envKnobs.emplace_back();
        addKnobs (*this, envKnobs.back(), p, { { E (e, env::Delay), "Delay" }, { E (e, env::Attack), "Attack" }, { E (e, env::Hold), "Hold" },
                                              { E (e, env::Decay), "Decay" }, { E (e, env::Sustain), "Sustain" }, { E (e, env::Release), "Release" } }, Theme::neonPink);
    }
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto* b = lfoTabs.add (new juce::TextButton (juce::String (l + 1)));
        b->onClick = [this, l] { selectLfo (l); };
        addAndMakeVisible (b);
        LfoControls c;
        c.shape = std::make_unique<ParamCombo> (p, L (l, lfo::Shape));
        c.div = std::make_unique<ParamCombo> (p, L (l, lfo::SyncDiv));
        c.mode = std::make_unique<ParamCombo> (p, L (l, lfo::Mode));
        c.sync = std::make_unique<ParamToggle> (p, L (l, lfo::Sync), "SYNC");
        addAndMakeVisible (*c.shape); addAndMakeVisible (*c.div); addAndMakeVisible (*c.mode); addAndMakeVisible (*c.sync);
        addKnobs (*this, c.knobs, p, { { L (l, lfo::Rate), "Rate" }, { L (l, lfo::Phase), "Phase" }, { L (l, lfo::Delay), "Delay" },
                                       { L (l, lfo::Fade), "Fade" }, { L (l, lfo::Smooth), "Smooth" } }, Theme::cyberCyan);
        lfoControls.push_back (std::move (c));
    }
    for (int m = 0; m < kNumMacros; ++m)
    {
        macroKnobs.push_back (std::make_unique<ParamKnob> (p, MACRO (m), "M" + juce::String (m + 1), Theme::sunsetOrange));
        addAndMakeVisible (*macroKnobs.back());
    }
    std::vector<int> sources;
    for (int s = modsrc::Env1; s <= modsrc::Env4; ++s) sources.push_back (s);
    for (int s = modsrc::Lfo1; s <= modsrc::Lfo10; ++s) sources.push_back (s);
    for (int s = modsrc::Macro1; s <= modsrc::Macro8; ++s) sources.push_back (s);
    for (int s : { (int) modsrc::Velocity, (int) modsrc::Note, (int) modsrc::ModWheel, (int) modsrc::Aftertouch, (int) modsrc::PitchBend,
                   (int) modsrc::Random, (int) modsrc::Expression, (int) modsrc::Timbre, (int) modsrc::SeqMod })
        sources.push_back (s);
    for (int s : sources)
    {
        auto* chip = chips.add (new ModSourceChip (s));
        if (s >= modsrc::Env1 && s <= modsrc::Env4) chip->liveValue = [&p, s] { return p.getEngine().envDisplay[(size_t) (s - modsrc::Env1)].load(); };
        else if (s >= modsrc::Lfo1 && s <= modsrc::Lfo10) chip->liveValue = [&p, s] { return p.getEngine().lfoDisplay[(size_t) (s - modsrc::Lfo1)].load(); };
        else if (s >= modsrc::Macro1 && s <= modsrc::Macro8) chip->liveValue = [&p, s] { return p.getParam (MACRO (s - modsrc::Macro1)); };
        addAndMakeVisible (chip);
    }
    selectEnv (0);
    selectLfo (0);
}

void ModulationSection::selectEnv (int e)
{
    currentEnv = e;
    envEditor.setEnvelope (e);
    for (int i = 0; i < kNumEnvs; ++i)
    {
        envTabs[i]->setToggleState (i == e, juce::dontSendNotification);
        for (auto& k : envKnobs[(size_t) i]) k->setVisible (i == e);
    }
    resized();
}

void ModulationSection::selectLfo (int l)
{
    currentLfo = l;
    lfoEditor.setLfo (l);
    for (int i = 0; i < kNumLfos; ++i)
    {
        lfoTabs[i]->setToggleState (i == l, juce::dontSendNotification);
        auto& c = lfoControls[(size_t) i];
        const bool v = i == l;
        c.shape->setVisible (v); c.div->setVisible (v); c.mode->setVisible (v); c.sync->setVisible (v);
        for (auto& k : c.knobs) k->setVisible (v);
    }
    resized();
}

void ModulationSection::refresh()
{
    envEditor.repaint();
    lfoEditor.repaint();
    for (auto* c : chips) c->repaint();
}

void ModulationSection::paint (juce::Graphics&) {}

void ModulationSection::resized()
{
    auto r = getLocalBounds();
    auto chipRow = r.removeFromTop (22);
    const int cw = chipRow.getWidth() / std::max (1, chips.size());
    for (int i = 0; i < chips.size(); ++i) chips[i]->setBounds (chipRow.getX() + i * cw, chipRow.getY(), cw - 2, 20);
    r.removeFromTop (4);

    auto envArea = r.removeFromLeft (r.getWidth() * 36 / 100);
    r.removeFromLeft (6);
    auto macroArea = r.removeFromRight (r.getWidth() * 30 / 100);
    r.removeFromRight (6);
    auto lfoArea = r;

    envBox.setBounds (envArea);
    auto ec = envBox.getBounds().reduced (8).withTrimmedTop (18);
    auto tabs = envBox.getBounds().reduced (8, 3).removeFromTop (18).withTrimmedLeft (100);
    for (int i = 0; i < envTabs.size(); ++i) envTabs[i]->setBounds (tabs.removeFromLeft (26).reduced (1, 0));
    envEditor.setBounds (ec.removeFromTop (ec.getHeight() - 52));
    for (auto& k : envKnobs[(size_t) currentEnv]) juce::ignoreUnused (k);
    layoutKnobs (envKnobs[(size_t) currentEnv], ec.withTrimmedTop (2), 6);

    lfoBox.setBounds (lfoArea);
    auto lc = lfoBox.getBounds().reduced (8).withTrimmedTop (18);
    auto ltabs = lfoBox.getBounds().reduced (8, 3).removeFromTop (18).withTrimmedLeft (54);
    for (int i = 0; i < lfoTabs.size(); ++i) lfoTabs[i]->setBounds (ltabs.removeFromLeft (24).reduced (1, 0));
    auto& c = lfoControls[(size_t) currentLfo];
    auto ctrl = lc.removeFromTop (22);
    const int w4 = ctrl.getWidth() / 4;
    c.shape->setBounds (ctrl.removeFromLeft (w4).reduced (1, 0));
    c.mode->setBounds (ctrl.removeFromLeft (w4).reduced (1, 0));
    c.sync->setBounds (ctrl.removeFromLeft (w4 * 2 / 3));
    c.div->setBounds (ctrl.reduced (1, 0));
    lc.removeFromTop (3);
    lfoEditor.setBounds (lc.removeFromTop (lc.getHeight() - 52));
    layoutKnobs (c.knobs, lc.withTrimmedTop (2), 5);

    macroBox.setBounds (macroArea);
    auto mc = macroBox.getBounds().reduced (8).withTrimmedTop (18);
    const int mw = mc.getWidth() / 4, mh = mc.getHeight() / 2;
    for (int i = 0; i < (int) macroKnobs.size(); ++i)
        macroKnobs[(size_t) i]->setBounds (mc.getX() + (i % 4) * mw, mc.getY() + (i / 4) * mh, mw, mh);
}

// =================================================================================
Wheel::Wheel (ViceSynthProcessor& p, bool pitch) : proc (p), isPitch (pitch)
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange (pitch ? -1.0 : 0.0, 1.0, 0.0);
    setValue (0.0, juce::dontSendNotification);
    setColour (juce::Slider::trackColourId, pitch ? Theme::cyberCyan : Theme::neonPink);
    setTooltip (pitch ? "Pitch wheel (springs back)" : "Modulation wheel (MIDI CC 1)");
    onValueChange = [this]
    {
        if (isPitch) proc.injectMidi (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) (8192 + getValue() * 8191))));
        else proc.injectMidi (juce::MidiMessage::controllerEvent (1, 1, juce::jlimit (0, 127, (int) (getValue() * 127.0))));
    };
}

void Wheel::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e);
    if (isPitch) setValue (0.0, juce::sendNotificationSync);
}

BottomBar::BottomBar (ViceSynthProcessor& p)
    : Section ("PERFORMANCE", Theme::electricPurple), proc (p), pitchWheel (p, true), modWheel (p, false),
      keyboard (p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
      voiceMode (p, G (global::VoiceMode)), glideMode (p, G (global::GlideMode))
{
    for (auto* c : std::initializer_list<juce::Component*> { &pitchWheel, &modWheel, &keyboard, &voiceMode, &glideMode }) addAndMakeVisible (c);
    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (18.0f);
    addKnobs (*this, knobs, p, { { G (global::Polyphony), "Poly" }, { G (global::GlideTime), "Glide" }, { G (global::Transpose), "Transp" },
                                 { G (global::MasterTune), "Tune" }, { G (global::BendUp), "Bend+" }, { G (global::BendDown), "Bend-" },
                                 { G (global::AmpVelocity), "Vel" }, { G (global::OscDrift), "Drift" } }, Theme::electricPurple);
}

void BottomBar::resized()
{
    auto r = content();
    pitchWheel.setBounds (r.removeFromLeft (26));
    r.removeFromLeft (4);
    modWheel.setBounds (r.removeFromLeft (26));
    r.removeFromLeft (8);
    auto right = r.removeFromRight (470);
    keyboard.setBounds (r);
    right.removeFromLeft (8);
    auto combos = right.removeFromLeft (96);
    voiceMode.setBounds (combos.removeFromTop (24).reduced (0, 1));
    combos.removeFromTop (4);
    glideMode.setBounds (combos.removeFromTop (24).reduced (0, 1));
    const int w = right.getWidth() / 4, h = right.getHeight() / 2;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (right.getX() + (int) (i % 4) * w, right.getY() + (int) (i / 4) * h, w, h);
}

void BottomBar::paint (juce::Graphics& g)
{
    Section::paint (g);
    auto r = getLocalBounds().reduced (8, 4).removeFromTop (16);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f, true));
    const bool playing = proc.hostPlaying.load();
    g.drawText (juce::String (proc.hostBpm.load(), 1) + " BPM  " + (playing ? juce::String::fromUTF8 ("\xe2\x96\xb6 HOST PLAYING") : juce::String ("STOPPED"))
                    + "   |   voices " + juce::String (proc.getEngine().activeVoiceCount.load()),
                r, juce::Justification::centredRight);
}

} // namespace vsx
