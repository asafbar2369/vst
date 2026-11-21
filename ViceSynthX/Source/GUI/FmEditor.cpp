#include "FmEditor.h"
#include "../Synth/Oscillator.h"

namespace vsx
{
using namespace pid;

// =================================================================================
std::array<juce::Point<float>, fm::kOps> FmAlgorithmDiagram::layout (int a)
{
    const auto& al = fm::algorithm (a);
    std::array<juce::Point<float>, fm::kOps> pos {};
    std::array<bool, fm::kOps> placed {};
    float column = 0.0f;
    std::function<float (int, int)> place = [&] (int op, int depth) -> float
    {
        if (placed[(size_t) op]) return pos[(size_t) op].x;
        placed[(size_t) op] = true;
        pos[(size_t) op].y = (float) depth;
        float sum = 0.0f;
        int n = 0;
        for (int m = fm::kOps - 1; m > op; --m)
            if (((al.modulators[(size_t) op] >> m) & 1) && ! placed[(size_t) m]) { sum += place (m, depth + 1); ++n; }
        pos[(size_t) op].x = n > 0 ? sum / (float) n : column++;
        return pos[(size_t) op].x;
    };
    for (int i = 0; i < fm::kOps; ++i)
        if ((al.carriers >> i) & 1) place (i, 0);
    for (int i = 0; i < fm::kOps; ++i)
        if (! placed[(size_t) i]) place (i, 1); // not reached for valid algorithms
    return pos;
}

void FmAlgorithmDiagram::paint (juce::Graphics& g)
{
    Theme::displayWell (g, getLocalBounds().toFloat());
    const auto& al = fm::algorithm (algo);
    const auto pos = layout (algo);
    float maxX = 0.0f, maxY = 0.0f;
    for (auto p : pos) { maxX = std::max (maxX, p.x); maxY = std::max (maxY, p.y); }
    auto area = getLocalBounds().toFloat().reduced (14.0f, 12.0f).withTrimmedBottom (14.0f).withTrimmedTop (14.0f);
    const float bw = std::min (40.0f, area.getWidth() / (maxX + 1.5f)), bh = std::min (26.0f, area.getHeight() / (maxY + 1.8f));
    auto centre = [&] (int op)
    {
        const auto p = pos[(size_t) op];
        const float x = area.getX() + (maxX < 1.0e-3f ? area.getWidth() * 0.5f : (p.x + 0.5f) / (maxX + 1.0f) * area.getWidth());
        const float y = area.getBottom() - bh * 0.5f - p.y * (area.getHeight() - bh) / std::max (1.0f, maxY);
        return juce::Point<float> (x, y);
    };
    // modulation paths
    g.setColour (Theme::textDim.withAlpha (0.8f));
    for (int t = 0; t < fm::kOps; ++t)
        for (int m = 0; m < fm::kOps; ++m)
            if ((al.modulators[(size_t) t] >> m) & 1)
            {
                const auto a = centre (m), b = centre (t);
                g.drawLine (a.x, a.y + bh * 0.5f, b.x, b.y - bh * 0.5f, 1.6f);
            }
    // carrier bus
    const float busY = getHeight() - 10.0f;
    float busL = 1.0e9f, busR = -1.0e9f;
    g.setColour (Theme::neonPink.withAlpha (0.8f));
    for (int i = 0; i < fm::kOps; ++i)
        if ((al.carriers >> i) & 1)
        {
            const auto c = centre (i);
            g.drawLine (c.x, c.y + bh * 0.5f, c.x, busY, 1.6f);
            busL = std::min (busL, c.x); busR = std::max (busR, c.x);
        }
    if (busR >= busL) g.drawLine (busL, busY, busR, busY, 2.0f);
    // feedback loop
    {
        const auto s = centre (al.feedbackSource), t = centre (al.feedbackTarget);
        juce::Path p;
        const float x = std::max (s.x, t.x) + bw * 0.5f + 7.0f;
        p.startNewSubPath (s.x + bw * 0.5f, s.y);
        p.lineTo (x, s.y);
        p.lineTo (x, t.y - bh * 0.5f - 6.0f);
        p.lineTo (t.x, t.y - bh * 0.5f - 6.0f);
        p.lineTo (t.x, t.y - bh * 0.5f);
        g.setColour (Theme::sunsetOrange.withAlpha (0.85f));
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
    // operator boxes
    for (int i = 0; i < fm::kOps; ++i)
    {
        const auto c = centre (i);
        const auto r = juce::Rectangle<float> (bw, bh).withCentre (c);
        const bool carrier = (al.carriers >> i) & 1;
        const bool on = (enabled >> i) & 1;
        const auto accent = carrier ? Theme::neonPink : Theme::cyberCyan;
        g.setColour (Theme::deepBlack);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? accent.withAlpha (0.35f) : Theme::outline.withAlpha (0.3f));
        g.fillRoundedRectangle (r.reduced (1.0f), 4.0f);
        g.setColour (on ? accent : Theme::outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.2f);
        g.setColour (on ? juce::Colours::white : Theme::textDim);
        g.setFont (Theme::font (13.0f, true));
        g.drawText (juce::String (i + 1), r, juce::Justification::centred);
    }
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.0f, true));
    g.drawText ("ALGORITHM " + juce::String (algo + 1), getLocalBounds().reduced (8, 4).removeFromTop (16), juce::Justification::left);
}

// =================================================================================
class FmEditor::OpRow : public juce::Component
{
public:
    OpRow (FmEditor& e, int opIndex) : ed (e), op (opIndex)
    {
        label.setText ("OP" + juce::String (op + 1), juce::dontSendNotification);
        label.setFont (Theme::font (13.0f, true));
        label.setColour (juce::Label::textColourId, Theme::text);
        addAndMakeVisible (label);
        for (auto* b : { &mute, &solo, &mode })
        {
            b->setColour (juce::TextButton::buttonOnColourId, Theme::neonPink.withAlpha (0.7f));
            addAndMakeVisible (b);
        }
        mute.setComponentID ("vsx_fm_mute_" + juce::String (op));
        solo.setComponentID ("vsx_fm_solo_" + juce::String (op));
        mute.setClickingTogglesState (false);
        mute.setTooltip ("Operator on / off (mute)");
        solo.setTooltip ("Solo this operator (click again to release)");
        mode.setTooltip ("Ratio: follows the key. Fixed: constant frequency (1 / 10 / 100 / 1000 Hz x fine).");
        mute.onClick = [this] { ed.toggleMute (op); };
        solo.onClick = [this] { ed.setSolo (ed.solo == op ? -1 : op); };
        mode.onClick = [this] { auto& m = ed.voice.op[(size_t) op].fixedMode; m = m ? 0 : 1; ed.pushEdit(); ed.syncControls(); };
        freq.setFont (Theme::font (11.0f));
        freq.setColour (juce::Label::textColourId, Theme::cyberCyan);
        freq.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (freq);
        struct F { int lo, hi; const char* tip; std::function<uint8_t& ()> ref; };
        auto O_ = [this] () -> fm::Operator& { return ed.voice.op[(size_t) op]; };
        const std::vector<F> f {
            { 0, 31, "Coarse (ratio: 0 = 0.5, 1-31; fixed: 1 / 10 / 100 / 1000 Hz)", [O_] () -> uint8_t& { return O_().coarse; } },
            { 0, 99, "Fine", [O_] () -> uint8_t& { return O_().fine; } },
            { 0, 14, "Detune (7 = centre)", [O_] () -> uint8_t& { return O_().detune; } },
            { 0, 99, "Output level", [O_] () -> uint8_t& { return O_().outputLevel; } },
            { 0, 7, "Velocity sensitivity", [O_] () -> uint8_t& { return O_().velSens; } },
            { 0, 3, "Amplitude-modulation sensitivity (LFO)", [O_] () -> uint8_t& { return O_().ampModSens; } },
            { 0, 99, "Rate 1", [O_] () -> uint8_t& { return O_().rate[0]; } },
            { 0, 99, "Rate 2", [O_] () -> uint8_t& { return O_().rate[1]; } },
            { 0, 99, "Rate 3", [O_] () -> uint8_t& { return O_().rate[2]; } },
            { 0, 99, "Rate 4 (release)", [O_] () -> uint8_t& { return O_().rate[3]; } },
            { 0, 99, "Level 1", [O_] () -> uint8_t& { return O_().level[0]; } },
            { 0, 99, "Level 2", [O_] () -> uint8_t& { return O_().level[1]; } },
            { 0, 99, "Level 3 (sustain)", [O_] () -> uint8_t& { return O_().level[2]; } },
            { 0, 99, "Level 4 (start / release)", [O_] () -> uint8_t& { return O_().level[3]; } },
            { 0, 99, "Keyboard level scaling break point (39 = C3)", [O_] () -> uint8_t& { return O_().breakPoint; } },
            { 0, 99, "Left scaling depth", [O_] () -> uint8_t& { return O_().leftDepth; } },
            { 0, 99, "Right scaling depth", [O_] () -> uint8_t& { return O_().rightDepth; } },
            { 0, 3, "Left curve (0 -LIN, 1 -EXP, 2 +EXP, 3 +LIN)", [O_] () -> uint8_t& { return O_().leftCurve; } },
            { 0, 3, "Right curve (0 -LIN, 1 -EXP, 2 +EXP, 3 +LIN)", [O_] () -> uint8_t& { return O_().rightCurve; } },
            { 0, 7, "Keyboard rate scaling", [O_] () -> uint8_t& { return O_().rateScale; } },
        };
        for (const auto& x : f) fields.push_back (&ed.makeField (*this, x.lo, x.hi, x.tip, x.ref));
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 2);
        label.setBounds (r.removeFromLeft (40));
        mute.setBounds (r.removeFromLeft (36).reduced (1, 0));
        solo.setBounds (r.removeFromLeft (24).reduced (1, 0));
        mode.setBounds (r.removeFromLeft (48).reduced (1, 0));
        freq.setBounds (r.removeFromLeft (56));
        const int w = r.getWidth() / (int) fields.size();
        for (auto* s : fields) s->setBounds (r.removeFromLeft (w).reduced (1, 0));
    }
    void sync()
    {
        const auto& o = ed.voice.op[(size_t) op];
        const bool on = (ed.muteMask >> op) & 1;
        mute.setButtonText (on ? "ON" : "OFF");
        mute.setToggleState (on, juce::dontSendNotification);
        solo.setButtonText ("S");
        solo.setToggleState (ed.solo == op, juce::dontSendNotification);
        mode.setButtonText (o.fixedMode ? "FIXED" : "RATIO");
        mode.setToggleState (o.fixedMode != 0, juce::dontSendNotification);
        juce::String f;
        if (o.fixedMode) f = juce::String (std::pow (10.0, (double) (o.coarse & 3)) * std::pow (10.0, o.fine / 100.0), 1) + " Hz";
        else f = "x" + juce::String ((o.coarse == 0 ? 0.5 : (double) o.coarse) * (1.0 + o.fine / 100.0), 2);
        freq.setText (f, juce::dontSendNotification);
        const bool carrier = (fm::algorithm (ed.voice.algorithm).carriers >> op) & 1;
        label.setColour (juce::Label::textColourId, carrier ? Theme::neonPink : Theme::cyberCyan);
    }
    std::vector<juce::Slider*> fields;
private:
    FmEditor& ed;
    int op;
    juce::Label label, freq;
    juce::TextButton mute, solo, mode;
};

// =================================================================================
namespace
{
    /** Compact numeric field: value printed in a box with a proportional fill; drag vertically or use the wheel. */
    class NumberBox : public juce::Slider
    {
    public:
        NumberBox() : juce::Slider (juce::Slider::LinearBarVertical, juce::Slider::NoTextBox) {}
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f, 1.0f);
            g.setColour (Theme::display);
            g.fillRoundedRectangle (r, 3.0f);
            const double range = getMaximum() - getMinimum();
            const float frac = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
            g.setColour (Theme::electricPurple.withAlpha (isMouseOverOrDragging() ? 0.55f : 0.38f));
            g.fillRoundedRectangle (r.withWidth (std::max (2.0f, r.getWidth() * frac)), 3.0f);
            g.setColour (isMouseOverOrDragging() ? Theme::neonPink : Theme::outline);
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
            g.setColour (Theme::text);
            g.setFont (Theme::font (std::min (13.0f, (float) getHeight() * 0.55f), true));
            g.drawText (juce::String (juce::roundToInt (getValue())), getLocalBounds(), juce::Justification::centred);
        }
    };
}

juce::Slider& FmEditor::makeField (juce::Component& parent, int lo, int hi, const juce::String& tip, std::function<uint8_t& ()> ref)
{
    auto* s = new NumberBox();
    s->setRange (lo, hi, 1.0);
    s->setDoubleClickReturnValue (true, (double) lo);
    s->setTooltip (tip);
    s->setColour (juce::Slider::trackColourId, Theme::electricPurple.withAlpha (0.45f));
    s->setColour (juce::Slider::backgroundColourId, Theme::display);
    s->setColour (juce::Slider::textBoxTextColourId, Theme::text);
    s->setColour (juce::Slider::textBoxOutlineColourId, Theme::outline);
    s->setVelocityBasedMode (false);
    s->setMouseDragSensitivity (std::max (60, (hi - lo) * 3));
    s->setSliderSnapsToMousePosition (false);
    const int idx = (int) allFields.size();
    s->onValueChange = [this, idx]
    {
        if (syncing) return;
        fieldRefs[(size_t) idx]() = (uint8_t) juce::roundToInt (allFields[(size_t) idx]->getValue());
        pushEdit();
        for (auto* r : rows) r->sync();
        diagram.setAlgorithm (voice.algorithm, voice.opEnabled);
    };
    parent.addAndMakeVisible (s);
    allFields.push_back (s);
    fieldRefs.push_back (std::move (ref));
    if (&parent == this) globalFields.add (s);
    else ownedRowFields.add (s);
    return *s;
}

FmEditor::FmEditor (ViceSynthProcessor& p) : proc (p)
{
    setComponentID ("vsx_fm_editor");
    for (int o = 0; o < kNumOscs; ++o)
    {
        auto* b = oscButtons.add (new juce::TextButton ("OSC " + juce::String (o + 1)));
        b->setColour (juce::TextButton::buttonOnColourId, Theme::neonPink);
        b->onClick = [this, o] { selectOscillator (o); };
        addAndMakeVisible (b);
    }
    useFm.setComponentID ("vsx_fm_use");
    useFm.setTooltip ("Switch the selected oscillator to FM mode");
    useFm.onClick = [this]
    {
        proc.setParam (O (osc, osc::On), 1.0f);
        proc.setParam (O (osc, osc::Mode), (float) (int) OscMode::FM);
    };
    loadCart.setComponentID ("vsx_fm_load_cartridge");
    loadCart.setTooltip ("Load a .syx cartridge (32-voice bank or single voice). Your files are read locally and never uploaded.");
    loadCart.setColour (juce::TextButton::buttonColourId, Theme::neonPink.withAlpha (0.3f));
    loadCart.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load six-operator FM cartridge", juce::File(), "*.syx;*.SYX");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<FmEditor> (this)] (const juce::FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult() != juce::File()) safe->loadCartridge (fc.getResult());
        });
    };
    factoryB.setTooltip ("Original VICE SYNTH X FM voices (not taken from any hardware ROM)");
    factoryB.onClick = [this]
    {
        cartridge = fm::factoryVoices();
        cartName = "Factory FM voices";
        cartList.updateContent();
        cartList.repaint();
        status = juce::String ((int) cartridge.size()) + " original factory voices";
        statusLabel.setText (status, juce::dontSendNotification);
        cartList.grabKeyboardFocus();
    };
    initB.setTooltip ("Reset this oscillator's FM voice to a single sine carrier");
    initB.onClick = [this] { voice = fm::Patch::init(); muteMask = 0x3F; solo = -1; pushEdit(); syncControls(); };
    saveVoiceB.setTooltip ("Save the current voice as a single-voice .syx file");
    saveBankB.setTooltip ("Save the loaded voice list (padded to 32) as a .syx bank; the current voice replaces the selected slot");
    for (auto* b : { &saveVoiceB, &saveBankB })
        b->onClick = [this, bank = b == &saveBankB]
        {
            chooser = std::make_unique<juce::FileChooser> (bank ? "Save FM bank" : "Save FM voice", juce::File(), "*.syx");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safe = juce::Component::SafePointer<FmEditor> (this), bank] (const juce::FileChooser& fc)
            {
                if (safe == nullptr || fc.getResult() == juce::File()) return;
                auto& self = *safe;
                juce::MemoryBlock mb;
                if (bank)
                {
                    auto voices = self.cartridge;
                    const int sel = self.cartList.getSelectedRow();
                    if (juce::isPositiveAndBelow (sel, (int) voices.size())) voices[(size_t) sel] = self.voice;
                    else voices.insert (voices.begin(), self.voice);
                    mb = fm::makeBankSysex (voices);
                }
                else mb = fm::makeVoiceSysex (self.voice);
                const bool ok = fc.getResult().withFileExtension ("syx").replaceWithData (mb.getData(), mb.getSize());
                self.status = ok ? "Saved " + fc.getResult().getFileName() : "Could not write " + fc.getResult().getFileName();
                self.statusLabel.setText (self.status, juce::dontSendNotification);
            });
        };
    newPatchB.setClickingTogglesState (true);
    newPatchB.setToggleState (true, juce::dontSendNotification);
    newPatchB.setColour (juce::TextButton::buttonOnColourId, Theme::cyberCyan.withAlpha (0.5f));
    newPatchB.setTooltip ("On: a chosen voice becomes a clean new patch (OSC 1, filters off). Off: it replaces only the selected oscillator.");
    algoPrev.onClick = [this] { voice.algorithm = (uint8_t) ((voice.algorithm + 31) % 32); pushEdit(); syncControls(); };
    algoNext.onClick = [this] { voice.algorithm = (uint8_t) ((voice.algorithm + 1) % 32); pushEdit(); syncControls(); };
    oscSyncB.setClickingTogglesState (true);
    lfoSyncB.setClickingTogglesState (true);
    oscSyncB.setTooltip ("Oscillator key sync: operators restart at zero phase on every note");
    lfoSyncB.setTooltip ("LFO key sync: the LFO restarts on every note");
    oscSyncB.onClick = [this] { voice.oscSync = oscSyncB.getToggleState() ? 1 : 0; pushEdit(); };
    lfoSyncB.onClick = [this] { voice.lfoSync = lfoSyncB.getToggleState() ? 1 : 0; pushEdit(); };
    for (auto* b : { &useFm, &loadCart, &factoryB, &initB, &saveVoiceB, &saveBankB, &newPatchB, &algoPrev, &algoNext, &oscSyncB, &lfoSyncB })
        addAndMakeVisible (b);

    nameEdit.setInputRestrictions (10);
    nameEdit.setFont (Theme::font (14.0f, true));
    nameEdit.setTooltip ("Voice name (10 characters)");
    nameEdit.onTextChange = [this] { if (! syncing) { voice.setName (nameEdit.getText()); pushEdit(); } };
    addAndMakeVisible (nameEdit);
    for (auto* l : { &algoLabel, &statusLabel, &cartLabel })
    {
        l->setFont (Theme::font (12.0f, l == &algoLabel));
        l->setColour (juce::Label::textColourId, l == &statusLabel ? Theme::cyberCyan : Theme::text);
        addAndMakeVisible (l);
    }
    algoLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (diagram);
    lfoWave.addItemList ({ "Triangle", "Saw Down", "Saw Up", "Square", "Sine", "Sample & Hold" }, 1);
    lfoWave.onChange = [this] { if (! syncing) { voice.lfoWave = (uint8_t) (lfoWave.getSelectedItemIndex()); pushEdit(); } };
    addAndMakeVisible (lfoWave);

    for (int i = 0; i < fm::kOps; ++i) addAndMakeVisible (rows.add (new OpRow (*this, i)));

    struct G { const char* name; int lo, hi; const char* tip; std::function<uint8_t& ()> ref; };
    const std::vector<G> globals {
        { "FB", 0, 7, "Feedback", [this] () -> uint8_t& { return voice.feedback; } },
        { "TRNSP", 0, 48, "Transpose (24 = no shift)", [this] () -> uint8_t& { return voice.transpose; } },
        { "P R1", 0, 99, "Pitch envelope rate 1", [this] () -> uint8_t& { return voice.pitchRate[0]; } },
        { "P R2", 0, 99, "Pitch envelope rate 2", [this] () -> uint8_t& { return voice.pitchRate[1]; } },
        { "P R3", 0, 99, "Pitch envelope rate 3", [this] () -> uint8_t& { return voice.pitchRate[2]; } },
        { "P R4", 0, 99, "Pitch envelope rate 4", [this] () -> uint8_t& { return voice.pitchRate[3]; } },
        { "P L1", 0, 99, "Pitch envelope level 1 (50 = no shift)", [this] () -> uint8_t& { return voice.pitchLevel[0]; } },
        { "P L2", 0, 99, "Pitch envelope level 2", [this] () -> uint8_t& { return voice.pitchLevel[1]; } },
        { "P L3", 0, 99, "Pitch envelope level 3", [this] () -> uint8_t& { return voice.pitchLevel[2]; } },
        { "P L4", 0, 99, "Pitch envelope level 4", [this] () -> uint8_t& { return voice.pitchLevel[3]; } },
        { "SPD", 0, 99, "LFO speed", [this] () -> uint8_t& { return voice.lfoSpeed; } },
        { "DLY", 0, 99, "LFO delay", [this] () -> uint8_t& { return voice.lfoDelay; } },
        { "PMD", 0, 99, "LFO pitch-modulation depth", [this] () -> uint8_t& { return voice.lfoPmd; } },
        { "AMD", 0, 99, "LFO amplitude-modulation depth", [this] () -> uint8_t& { return voice.lfoAmd; } },
        { "PMS", 0, 7, "Pitch-modulation sensitivity", [this] () -> uint8_t& { return voice.lfoPms; } },
    };
    for (const auto& gdef : globals)
    {
        globalNames.add (gdef.name);
        makeField (*this, gdef.lo, gdef.hi, gdef.tip, gdef.ref);
    }

    cartList.setRowHeight (20);
    cartList.setColour (juce::ListBox::backgroundColourId, Theme::display);
    cartList.setComponentID ("vsx_fm_cartridge_list");
    addAndMakeVisible (cartList);
    selectOscillator (0);
    startTimerHz (10);
}

FmEditor::~FmEditor() { stopTimer(); }

void FmEditor::selectOscillator (int o)
{
    osc = juce::jlimit (0, kNumOscs - 1, o);
    for (int i = 0; i < oscButtons.size(); ++i) oscButtons[i]->setToggleState (i == osc, juce::dontSendNotification);
    useFm.setButtonText ("USE FM ON OSC " + juce::String (osc + 1));
    fmAmount = std::make_unique<ParamKnob> (proc, O (osc, osc::FmAmount), "FM AMT", Theme::neonPink);
    fmAmount->setTooltipExtra ("Scales every modulator of this oscillator's FM voice (automatable).");
    addAndMakeVisible (*fmAmount);
    lastHex.clear();
    pullVoice();
    resized();
}

void FmEditor::pullVoice()
{
    voice = proc.getPatch().getFmVoice (osc);
    lastHex = voice.toHex();
    muteMask = voice.opEnabled;
    solo = -1;
    syncControls();
}

void FmEditor::pushEdit()
{
    voice.opEnabled = solo >= 0 ? (uint8_t) (1u << solo) : muteMask;
    voice.clamp();
    proc.getPatch().setFmVoice (osc, voice);
    lastHex = voice.toHex();
    diagram.setAlgorithm (voice.algorithm, voice.opEnabled);
}

void FmEditor::setSolo (int op)
{
    solo = juce::isPositiveAndBelow (op, fm::kOps) ? op : -1;
    pushEdit();
    syncControls();
}

void FmEditor::toggleMute (int op)
{
    if (! juce::isPositiveAndBelow (op, fm::kOps)) return;
    muteMask ^= (uint8_t) (1u << op);
    pushEdit();
    syncControls();
}

void FmEditor::syncControls()
{
    syncing = true;
    for (size_t i = 0; i < allFields.size(); ++i) allFields[i]->setValue ((double) fieldRefs[i](), juce::dontSendNotification);
    for (auto* r : rows) r->sync();
    nameEdit.setText (voice.getName(), false);
    algoLabel.setText ("ALG " + juce::String (voice.algorithm + 1) + " / 32", juce::dontSendNotification);
    oscSyncB.setToggleState (voice.oscSync != 0, juce::dontSendNotification);
    lfoSyncB.setToggleState (voice.lfoSync != 0, juce::dontSendNotification);
    lfoWave.setSelectedItemIndex (voice.lfoWave, juce::dontSendNotification);
    diagram.setAlgorithm (voice.algorithm, voice.opEnabled);
    syncing = false;
}

void FmEditor::timerCallback()
{
    // presets, undo or project recall can replace the voice behind the editor
    const auto current = proc.getPatch().getFmVoice (osc).toHex();
    if (current != lastHex) pullVoice();
    const bool isFm = (int) proc.getParam (O (osc, osc::Mode)) == (int) OscMode::FM && proc.getParam (O (osc, osc::On)) > 0.5f;
    useFm.setToggleState (isFm, juce::dontSendNotification);
    useFm.setButtonText (isFm ? "OSC " + juce::String (osc + 1) + " IS FM" : "USE FM ON OSC " + juce::String (osc + 1));
}

bool FmEditor::loadCartridge (const juce::File& f)
{
    const auto r = fm::loadSysexFile (f);
    if (! r.ok)
    {
        status = "Cannot load " + f.getFileName() + ": " + r.error;
        statusLabel.setText (status, juce::dontSendNotification);
        return false;
    }
    cartridge = r.voices;
    cartName = f.getFileNameWithoutExtension();
    status = juce::String ((int) cartridge.size()) + (cartridge.size() == 1 ? " voice" : " voices") + " from " + f.getFileName()
             + (r.checksumOk ? juce::String (" (checksum OK)") : juce::String (" - WARNING: checksum mismatch, data may be damaged"))
             + (r.warning.isNotEmpty() && r.checksumOk ? " - " + r.warning : juce::String());
    statusLabel.setText (status, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, r.checksumOk ? Theme::cyberCyan : Theme::sunsetOrange);
    cartList.updateContent();
    cartList.deselectAllRows();
    cartList.repaint();
    cartList.grabKeyboardFocus();
    return true;
}

void FmEditor::loadCartridgeVoice (int i)
{
    if (! juce::isPositiveAndBelow (i, (int) cartridge.size())) return;
    proc.loadFmVoice (osc, cartridge[(size_t) i], newPatchB.getToggleState());
    if (newPatchB.getToggleState() && osc != 0) selectOscillator (0);
    pullVoice();
}

void FmEditor::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) cartridge.size())) return;
    if (selected)
    {
        g.setColour (Theme::neonPink.withAlpha (0.25f));
        g.fillRect (0, 0, w, h);
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f));
    g.drawText (juce::String (row + 1).paddedLeft ('0', 2), 4, 0, 24, h, juce::Justification::centredLeft);
    g.setColour (selected ? juce::Colours::white : Theme::text);
    g.setFont (Theme::font (12.5f, selected));
    g.drawText (cartridge[(size_t) row].getName(), 30, 0, w - 70, h, juce::Justification::centredLeft);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    g.drawText ("ALG " + juce::String (cartridge[(size_t) row].algorithm + 1), w - 44, 0, 40, h, juce::Justification::centredRight);
}

void FmEditor::selectedRowsChanged (int row) { loadCartridgeVoice (row); } // arrow keys load immediately
void FmEditor::listBoxItemClicked (int row, const juce::MouseEvent&) { if (row == cartList.getSelectedRow()) loadCartridgeVoice (row); }

void FmEditor::paint (juce::Graphics& g)
{
    Theme::card (g, getLocalBounds().toFloat(), 10.0f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f, true));
    if (! opHeader.isEmpty())
    {
        auto h = opHeader;
        h.removeFromLeft (40 + 36 + 24 + 48 + 56);
        const char* names[] = { "CRS", "FINE", "DET", "LVL", "VEL", "AMS", "R1", "R2", "R3", "R4", "L1", "L2", "L3", "L4", "BRK", "L DP", "R DP", "L CV", "R CV", "RS" };
        const int w = h.getWidth() / 20;
        for (auto* n : names) g.drawText (n, h.removeFromLeft (w), juce::Justification::centred);
    }
    for (int i = 0; i < globalFields.size(); ++i)
    {
        const auto b = globalFields[i]->getBounds();
        g.drawText (globalNames[i], b.withY (b.getY() - 15).withHeight (14), juce::Justification::centred);
    }
    g.drawText ("LFO WAVE", lfoWave.getBounds().withY (lfoWave.getY() - 15).withHeight (14), juce::Justification::centred);
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.0f, true));
    g.drawText ("CARTRIDGE" + (cartName.isNotEmpty() ? ": " + cartName : juce::String()), cartLabel.getBounds(), juce::Justification::left);
    if (cartridge.empty())
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (11.5f));
        g.drawFittedText ("LOAD CARTRIDGE opens a .syx bank (32 voices)\nor single voice. FACTORY VOICES lists the\noriginal built-in voices. Arrow keys load\nthe next voice immediately.",
                          cartList.getBounds().reduced (8), juce::Justification::centredTop, 5);
    }
}

void FmEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (30);
    for (auto* b : oscButtons) b->setBounds (top.removeFromLeft (58).reduced (2, 0));
    top.removeFromLeft (6);
    useFm.setBounds (top.removeFromLeft (138).reduced (2, 0));
    top.removeFromLeft (6);
    nameEdit.setBounds (top.removeFromLeft (118).reduced (2, 1));
    top.removeFromLeft (6);
    for (auto* b : { &loadCart, &factoryB, &initB, &saveVoiceB, &saveBankB })
        b->setBounds (top.removeFromLeft (b == &loadCart ? 128 : 100).reduced (2, 0));
    r.removeFromTop (4);
    statusLabel.setBounds (r.removeFromTop (18));
    r.removeFromTop (4);

    auto left = r.removeFromLeft (std::min (280, r.getWidth() / 3));
    r.removeFromLeft (10);
    diagram.setBounds (left.removeFromTop (std::min (190, left.getHeight() / 2)));
    left.removeFromTop (4);
    auto algoRow = left.removeFromTop (26);
    algoPrev.setBounds (algoRow.removeFromLeft (30));
    algoNext.setBounds (algoRow.removeFromRight (30));
    algoLabel.setBounds (algoRow);
    left.removeFromTop (4);
    auto knobRow = left.removeFromTop (78);
    if (fmAmount != nullptr) fmAmount->setBounds (knobRow.removeFromLeft (80));
    auto syncCol = knobRow.reduced (6, 8);
    oscSyncB.setBounds (syncCol.removeFromTop (28).reduced (0, 2));
    lfoSyncB.setBounds (syncCol.removeFromTop (28).reduced (0, 2));
    left.removeFromTop (4);
    cartLabel.setBounds (left.removeFromTop (18));
    newPatchB.setBounds (left.removeFromBottom (26).reduced (0, 2));
    cartList.setBounds (left);

    opHeader = r.removeFromTop (16);
    const int rowH = std::max (24, std::min (36, (r.getHeight() - 90) / fm::kOps));
    for (auto* row : rows) row->setBounds (r.removeFromTop (rowH));
    r.removeFromTop (22);
    globalArea = r.removeFromTop (26);
    auto ga = globalArea;
    const int gw = ga.getWidth() / (globalFields.size() + 2);
    for (auto* s : globalFields) s->setBounds (ga.removeFromLeft (gw).reduced (2, 0));
    lfoWave.setBounds (ga.reduced (2, 0));
}
} // namespace vsx
