#include "OscSourceMenu.h"
#include "../Synth/SampleBank.h"
#include "../Synth/Wavetable.h"

namespace vsx
{
using namespace pid;

namespace
{
    struct Shape { const char* name; float position; float pulseWidth; };
    constexpr Shape kShapes[] = { { "Sine", 0.0f, 0.5f }, { "Triangle", 1.0f / 3.0f, 0.5f }, { "Saw", 2.0f / 3.0f, 0.5f },
                                  { "Square", 1.0f, 0.5f }, { "Pulse 25%", 1.0f, 0.25f } };
    enum { kShapeBase = 1, kFactoryBase = 100, kUserBase = 400, kSampleBase = 700, kMultiBase = 800, kLoadSample = 950, kLoadFolder = 951 };

    int currentShape (ViceSynthProcessor& p, int o)
    {
        if ((int) p.getParam (O (o, osc::Mode)) != 1) return -1;
        const float pos = p.getParam (O (o, osc::Position)), pw = p.getParam (O (o, osc::PulseWidth));
        for (int i = 0; i < (int) std::size (kShapes); ++i)
            if (std::abs (pos - kShapes[i].position) < 0.02f && (pos < 0.99f || std::abs (pw - kShapes[i].pulseWidth) < 0.02f)) return i;
        return -1;
    }
}

void OscSourceMenu::applyAnalogShape (ViceSynthProcessor& p, int o, int shape)
{
    if (! juce::isPositiveAndBelow (shape, (int) std::size (kShapes))) return;
    p.setParam (O (o, osc::On), 1.0f);
    p.setParam (O (o, osc::Mode), 1.0f);
    p.setParam (O (o, osc::Position), kShapes[shape].position);
    p.setParam (O (o, osc::PulseWidth), kShapes[shape].pulseWidth);
}

void OscSourceMenu::show (ViceSynthProcessor& proc, int o, juce::Component& target, std::unique_ptr<juce::FileChooser>& chooser)
{
    auto& patch = proc.getPatch();
    const int mode = (int) proc.getParam (O (o, osc::Mode));
    const auto& res = patch.osc[(size_t) o];
    const auto wtId = res.wavetableId, sId = res.sampleId, mId = res.multiId;

    juce::PopupMenu m, factory, user, samples, multis;
    m.addSectionHeader ("OSC " + juce::String (o + 1) + " WAVEFORM");
    const int cur = currentShape (proc, o);
    for (int i = 0; i < (int) std::size (kShapes); ++i) m.addItem (kShapeBase + i, juce::String ("Analog ") + kShapes[i].name, true, cur == i);

    const auto fNames = WavetableFactory::names();
    for (int i = 0; i < fNames.size(); ++i) factory.addItem (kFactoryBase + i, fNames[i], true, mode == 0 && wtId == "factory:" + fNames[i]);
    const auto uNames = WavetableLibrary::instance().userNames();
    for (int i = 0; i < uNames.size() && i < 300; ++i)
        user.addItem (kUserBase + i, juce::File (uNames[i]).getFileNameWithoutExtension(), true, mode == 0 && wtId == "user:" + uNames[i]);
    if (uNames.isEmpty()) user.addItem (-1, "(no user wavetables yet)", false);
    const auto sNames = BuiltinSamples::singleNames();
    for (int i = 0; i < sNames.size(); ++i) samples.addItem (kSampleBase + i, sNames[i], true, mode == 2 && sId == "builtin:" + sNames[i]);
    const auto mNames = BuiltinSamples::multiNames();
    for (int i = 0; i < mNames.size(); ++i) multis.addItem (kMultiBase + i, mNames[i], true, mode == 3 && mId == "builtin:" + mNames[i]);
    m.addSeparator();
    m.addSubMenu ("Wavetables", factory, true, nullptr, mode == 0 && wtId.startsWith ("factory:"));
    m.addSubMenu ("User wavetables", user, true, nullptr, mode == 0 && wtId.startsWith ("user:"));
    m.addSubMenu ("Built-in samples", samples);
    m.addSubMenu ("Built-in multisamples", multis);
    m.addSeparator();
    m.addItem (kLoadSample, "LOAD SAMPLE (WAV / AIFF / FLAC / SFZ)...");
    m.addItem (kLoadFolder, "Load folder as multisample...");

    juce::Component::SafePointer<juce::Component> safeTarget (&target);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                     [&proc, o, fNames, uNames, sNames, mNames, safeTarget, &chooser] (int r)
    {
        if (r <= 0 || safeTarget == nullptr) return;
        auto& pt = proc.getPatch();
        const int curMode = (int) proc.getParam (O (o, osc::Mode));
        const bool keepsWavetable = curMode == 0 || curMode == 4 || curMode == 5;
        if (r >= kShapeBase && r < kShapeBase + (int) std::size (kShapes)) applyAnalogShape (proc, o, r - kShapeBase);
        else if (r >= kFactoryBase && r < kUserBase)
        {
            pt.setWavetable (o, "factory:" + fNames[r - kFactoryBase], true);
            proc.setParam (O (o, osc::On), 1.0f);
            if (! keepsWavetable) proc.setParam (O (o, osc::Mode), 0.0f);
        }
        else if (r >= kUserBase && r < kSampleBase)
        {
            pt.setWavetable (o, "user:" + uNames[r - kUserBase], true);
            proc.setParam (O (o, osc::On), 1.0f);
            if (! keepsWavetable) proc.setParam (O (o, osc::Mode), 0.0f);
        }
        else if (r >= kSampleBase && r < kMultiBase)
        {
            pt.setSample (o, "builtin:" + sNames[r - kSampleBase], true);
            proc.setParam (O (o, osc::On), 1.0f);
            proc.setParam (O (o, osc::Source), 1.0f);
            if (curMode != 4 && curMode != 5) proc.setParam (O (o, osc::Mode), 2.0f);
        }
        else if (r >= kMultiBase && r < kLoadSample)
        {
            pt.setMultiSample (o, "builtin:" + mNames[r - kMultiBase], true);
            proc.setParam (O (o, osc::On), 1.0f);
            proc.setParam (O (o, osc::Mode), 3.0f);
        }
        else if (r == kLoadSample || r == kLoadFolder)
        {
            const bool folder = r == kLoadFolder;
            chooser = std::make_unique<juce::FileChooser> (folder ? "Choose a folder of note-named samples" : "Load sample across the full MIDI keyboard",
                                                           juce::File(), folder ? juce::String() : juce::String ("*.wav;*.aif;*.aiff;*.flac;*.sfz"));
            const int flags = juce::FileBrowserComponent::openMode
                              | (folder ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles);
            chooser->launchAsync (flags, [&proc, o, safeTarget] (const juce::FileChooser& fc)
            {
                if (safeTarget != nullptr && fc.getResult() != juce::File()) proc.importSampleFile (o, fc.getResult());
            });
        }
    });
}

// =================================================================================
OscSourceArrow::OscSourceArrow (ViceSynthProcessor& p, int o) : juce::Button ("Waveform"), proc (p), osc (o)
{
    setComponentID ("vsx_osc_source_" + juce::String (o));
    setTooltip ("Choose the waveform or source of OSC " + juce::String (o + 1) + " directly");
    setTitle ("OSC " + juce::String (o + 1) + " waveform");
    setWantsKeyboardFocus (true);
}

void OscSourceArrow::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Theme::display.brighter (over ? 0.25f : 0.1f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour ((over || down) ? Theme::neonPink : Theme::border.brighter (0.4f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    const auto c = r.getCentre().translated (0.0f, down ? 1.0f : 0.0f);
    juce::Path tri;
    tri.addTriangle (c.x - 4.5f, c.y - 2.0f, c.x + 4.5f, c.y - 2.0f, c.x, c.y + 3.5f);
    g.setColour (over ? juce::Colours::white : Theme::text);
    g.fillPath (tri);
}

void OscSourceArrow::clicked() { OscSourceMenu::show (proc, osc, *this, chooser); }
} // namespace vsx
