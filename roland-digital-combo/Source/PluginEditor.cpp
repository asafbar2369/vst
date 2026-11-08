#include "PluginEditor.h"

namespace
{
const juce::Colour kPanel (0xff23262b), kPanelDark (0xff16181b), kText (0xffe8e4dc), kD50 (0xff4fb3c8), kJd (0xffe0a040);
}

ComboLookAndFeel::ComboLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kText);
    setColour (juce::ComboBox::backgroundColourId, kPanelDark);
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555a62));
    setColour (juce::ToggleButton::textColourId, kText);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3f46));
    setColour (juce::TextButton::textColourOffId, kText);
    setColour (juce::TabbedButtonBar::tabTextColourId, kText);
    setColour (juce::TabbedComponent::backgroundColourId, kPanel);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colour (0xff555a62));
}

void ComboLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, start, end, true);
    g.setColour (juce::Colour (0xff3a3f46));
    g.strokePath (track, juce::PathStrokeType (2.5f));
    juce::Path value;
    value.addCentredArc (c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, start, angle, true);
    g.setColour (col);
    g.strokePath (value, juce::PathStrokeType (2.5f));

    g.setColour (juce::Colour (0xff0f1012));
    g.fillEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f);
    g.setColour (juce::Colour (0xff4a4f57));
    g.drawEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f, 1.0f);
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.2f, -r * 0.68f, 2.4f, r * 0.42f, 1.0f);
    g.setColour (kText);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

// ---------------------------------------------------------------------------
ParamPanel::ParamPanel (juce::AudioProcessorValueTreeState& state, const juce::StringArray& paramIds, int cols, juce::String t)
    : ids (paramIds), columns (juce::jmax (1, cols)), title (std::move (t))
{
    for (auto& pid : ids)
    {
        auto* param = state.getParameter (pid);
        jassert (param != nullptr);
        if (param == nullptr)
            continue;
        auto cell = std::make_unique<Cell>();
        cell->label = std::make_unique<juce::Label> (pid, DigitalComboEditor::shortName (param->getName (64)));
        cell->label->setJustificationType (juce::Justification::centred);
        cell->label->setFont (juce::FontOptions (11.0f));
        cell->label->setMinimumHorizontalScale (0.6f);
        addAndMakeVisible (*cell->label);

        auto* asChoice = dynamic_cast<juce::AudioParameterChoice*> (param);
        auto* asInt = dynamic_cast<juce::AudioParameterInt*> (param);
        const bool intOnOff = asInt != nullptr && asInt->getRange().getStart() == 0 && asInt->getRange().getEnd() == 1
                              && param->getText (1.0f, 16).equalsIgnoreCase ("ON");
        const bool isSwitch = dynamic_cast<juce::AudioParameterBool*> (param) != nullptr || intOnOff
                              || (asChoice != nullptr && asChoice->choices == juce::StringArray { "Off", "On" });
        // Enumerations (named values) are shown as drop-down lists
        static const juce::StringArray listIds { "jd_seqA", "jd_seqB", "jd_distType", "jd_revType", "d50_uc_structure",
                                                 "d50_pt_keyMode", "d50_uc_lfo1Wave", "d50_uc_mute" };
        const bool isList = asInt != nullptr && ! isSwitch
                            && (pid.endsWith ("_wave") || pid.endsWith ("_filterMode") || listIds.contains (pid));
        if (isSwitch)
        {
            cell->toggle = std::make_unique<juce::ToggleButton> ("ON");
            cell->toggle->setTitle (param->getName (64));
            addAndMakeVisible (*cell->toggle);
            cell->bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, pid, *cell->toggle);
        }
        else if (asChoice != nullptr || isList)
        {
            cell->combo = std::make_unique<juce::ComboBox> (pid);
            if (asChoice != nullptr)
                cell->combo->addItemList (asChoice->choices, 1);
            else
                for (int v = asInt->getRange().getStart(); v <= asInt->getRange().getEnd(); ++v)
                    cell->combo->addItem (param->getText (asInt->convertTo0to1 ((float) v), 64), v - asInt->getRange().getStart() + 1);
            cell->combo->setTitle (param->getName (64));
            addAndMakeVisible (*cell->combo);
            cell->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, pid, *cell->combo);
        }
        else
        {
            cell->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            cell->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
            cell->slider->setTitle (param->getName (64));
            cell->slider->setColour (juce::Slider::rotarySliderFillColourId, pid.startsWith ("d50") ? kD50 : kJd);
            addAndMakeVisible (*cell->slider);
            cell->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, pid, *cell->slider);
        }
        cells.push_back (std::move (cell));
    }
}

void ParamPanel::paint (juce::Graphics& g)
{
    g.setColour (kPanelDark.withAlpha (0.6f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
    if (title.isNotEmpty())
    {
        g.setColour (ids.size() > 0 && ids[0].startsWith ("d50") ? kD50 : kJd);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, getLocalBounds().removeFromTop (16).reduced (8, 0), juce::Justification::centredLeft);
    }
}

void ParamPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    if (title.isNotEmpty()) area.removeFromTop (14);
    const int rows = ((int) cells.size() + columns - 1) / columns;
    if (rows == 0) return;
    const int cw = area.getWidth() / columns;
    const int ch = area.getHeight() / rows;
    for (int i = 0; i < (int) cells.size(); ++i)
    {
        auto r = juce::Rectangle<int> (area.getX() + (i % columns) * cw, area.getY() + (i / columns) * ch, cw, ch).reduced (2);
        auto& c = *cells[(size_t) i];
        c.label->setBounds (r.removeFromTop (14));
        if (c.slider)
        {
            c.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmax (40, r.getWidth() - 4), 15);
            c.slider->setBounds (r);
        }
        else if (c.combo) c.combo->setBounds (r.withSizeKeepingCentre (r.getWidth(), 24));
        else if (c.toggle) c.toggle->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 60), 24));
    }
}

// ---------------------------------------------------------------------------
AreaEditor::AreaEditor (AreaEditorSpec s) : spec (std::move (s))
{
    blockBox.addItemList (spec.blockNames, 1);
    blockBox.setTitle ("Block");
    blockBox.onChange = [this] { selectBlock (blockBox.getSelectedItemIndex()); };
    addAndMakeVisible (blockBox);
    addAndMakeVisible (nameLabel);
    nameEditor.setTitle ("Name");
    nameEditor.onReturnKey = [this] { commitName(); };
    nameEditor.onFocusLost = [this] { commitName(); };
    addAndMakeVisible (nameEditor);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    selectBlock (0);
    startTimerHz (10);
}

AreaEditor::~AreaEditor()
{
    stopTimer();
}

void AreaEditor::selectBlock (int index)
{
    block = juce::jlimit (0, (int) spec.blocks.size() - 1, index);
    blockBox.setSelectedItemIndex (block, juce::dontSendNotification);
    rows.clear();
    content.removeAllChildren();
    const int nameLen = spec.nameLength (block);
    nameEditor.setVisible (nameLen > 0);
    nameLabel.setVisible (nameLen > 0);
    nameEditor.setInputRestrictions (nameLen, spec.allowedNameChars);

    const auto [start, size] = spec.blocks[(size_t) block];
    for (int off = start; off < start + size; ++off)
    {
        if (spec.hidden (off))
            continue;
        Row r;
        r.offset = off;
        r.label = std::make_unique<juce::Label> (juce::String(), juce::String (off - start) + "  " + spec.paramName (off));
        r.label->setFont (juce::FontOptions (12.0f));
        r.slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        r.slider->setRange ((double) spec.minFor (off), (double) juce::jmax (spec.minFor (off) + 1, spec.maxFor (off)), 1.0);
        r.slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 96, 18);
        r.slider->setTitle (spec.paramName (off));
        r.slider->setColour (juce::Slider::trackColourId, spec.colour.withAlpha (0.7f));
        r.slider->textFromValueFunction = [this, off] (double v) { return spec.format (off, (int) v); };
        r.slider->setValue (spec.get (off), juce::dontSendNotification);
        r.slider->updateText(); // show the document's value names even when the value is unchanged (e.g. 0)
        r.slider->onValueChange = [this, off, sl = r.slider.get()]
        {
            if (! refreshing) spec.set (off, (int) sl->getValue());
        };
        content.addAndMakeVisible (*r.label);
        content.addAndMakeVisible (*r.slider);
        rows.push_back (std::move (r));
    }
    timerCallback();
    resized();
}

juce::Slider* AreaEditor::sliderForOffset (int offset) noexcept
{
    for (auto& r : rows)
        if (r.offset == offset) return r.slider.get();
    return nullptr;
}

void AreaEditor::commitName()
{
    if (spec.nameLength (block) > 0)
        spec.setName (block, nameEditor.getText());
}

void AreaEditor::timerCallback()
{
    const juce::ScopedValueSetter<bool> svs (refreshing, true);
    for (auto& r : rows)
        if (! r.slider->isMouseButtonDown())
            r.slider->setValue (spec.get (r.offset), juce::dontSendNotification);
    if (spec.nameLength (block) > 0 && ! nameEditor.hasKeyboardFocus (true))
        nameEditor.setText (spec.getName (block), false);
}

void AreaEditor::paint (juce::Graphics& g)
{
    g.setColour (kPanelDark.withAlpha (0.6f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
}

void AreaEditor::resized()
{
    auto area = getLocalBounds().reduced (6);
    auto top = area.removeFromTop (28);
    blockBox.setBounds (top.removeFromLeft (300).reduced (2));
    top.removeFromLeft (16);
    nameLabel.setBounds (top.removeFromLeft (50));
    nameEditor.setBounds (top.removeFromLeft (220).reduced (2));
    area.removeFromTop (4);
    viewport.setBounds (area);

    const int columns = 2, rowH = 24;
    const int colW = juce::jmax (200, (viewport.getWidth() - viewport.getScrollBarThickness()) / columns);
    const int numRows = juce::jmax (1, ((int) rows.size() + columns - 1) / columns);
    content.setSize (colW * columns, numRows * rowH);
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        auto cell = juce::Rectangle<int> ((i / numRows) * colW, (i % numRows) * rowH, colW, rowH).reduced (4, 2);
        rows[(size_t) i].label->setBounds (cell.removeFromLeft (cell.getWidth() * 45 / 100));
        rows[(size_t) i].slider->setBounds (cell);
    }
}

static AreaEditorSpec makeD50Spec (DigitalComboProcessor& proc)
{
    AreaEditorSpec s;
    s.blockNames = { "Upper Partial-1  [00-00-00]", "Upper Partial-2  [00-00-40]", "Upper Common  [00-01-00]",
                     "Lower Partial-1  [00-01-40]", "Lower Partial-2  [00-02-00]", "Lower Common  [00-02-40]",
                     "Patch  [00-03-00]" };
    for (int b = 0; b < 7; ++b) s.blocks.push_back ({ b * d50::kBlockSize, d50::kBlockSize });
    auto info = [] (int off) -> const d50::ParamInfo&
    {
        const int local = off % d50::kBlockSize;
        switch (d50::blockForTempOffset (off))
        {
            case d50::Block::UpperCommon: case d50::Block::LowerCommon: return d50::commonParam (local);
            case d50::Block::Patch: return d50::patchParam (local);
            case d50::Block::UpperPartial1: case d50::Block::UpperPartial2:
            case d50::Block::LowerPartial1: case d50::Block::LowerPartial2:
            default: return d50::partialParam (local);
        }
    };
    s.paramName = [info] (int off) { return juce::String (info (off).name); };
    s.minFor = [] (int) { return 0; };
    s.maxFor = [info] (int off) { return (int) info (off).maxValue; };
    s.hidden = [info] (int off)
    {
        const juce::String n (info (off).name);
        return n == "Extension" || n.startsWith ("Tone Name") || n.startsWith ("Patch Name");
    };
    s.format = [] (int off, int v) { return params::formatD50Value (off, v); };
    s.get = [&proc] (int off) { return (int) proc.getD50Byte (off); };
    s.set = [&proc] (int off, int v) { proc.setD50ByteFromUi (off, v); };
    s.nameLength = [] (int b) { return (b == 2 || b == 5) ? 10 : (b == 6 ? 18 : 0); };
    s.getName = [&proc] (int b) { return proc.getD50Name (b * d50::kBlockSize, b == 6 ? 18 : 10); };
    s.setName = [&proc] (int b, const juce::String& t) { proc.setD50Name (b * d50::kBlockSize, b == 6 ? 18 : 10, t); };
    s.allowedNameChars = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-";
    s.colour = kD50;
    return s;
}

static AreaEditorSpec makeJdSpec (DigitalComboProcessor& proc)
{
    using namespace jd800;
    AreaEditorSpec s;
    s.blockNames = { "Patch Common  [00-00-00]", "Patch Effect  [00-00-32]", "Tone A  [00-00-60]", "Tone B  [00-01-28]",
                     "Tone C  [00-01-70]", "Tone D  [00-02-38]" };
    s.blocks = { { kCommonOffset, kCommonSize }, { kEffectOffset, kEffectSize }, { kToneOffset[0], kToneSize },
                 { kToneOffset[1], kToneSize }, { kToneOffset[2], kToneSize }, { kToneOffset[3], kToneSize } };
    s.paramName = [] (int off) { return juce::String (paramForOffset (off).name); };
    s.minFor = [] (int off) { return (int) paramForOffset (off).minValue; };
    s.maxFor = [] (int off) { return (int) paramForOffset (off).maxValue; };
    s.hidden = [] (int off)
    {
        const juce::String n (paramForOffset (off).name);
        return n == "<dummy>" || n.startsWith ("Patch name");
    };
    s.format = [] (int off, int v) { return params::formatJdValue (off, v); };
    s.get = [&proc] (int off) { return (int) proc.getJdByte (off); };
    s.set = [&proc] (int off, int v) { proc.setJdByteFromUi (off, v); };
    s.nameLength = [] (int b) { return b == 0 ? 16 : 0; };
    s.getName = [&proc] (int) { return proc.getJdName(); };
    s.setName = [&proc] (int, const juce::String& t) { proc.setJdName (t); };
    s.allowedNameChars = {};
    s.colour = kJd;
    return s;
}

// ---------------------------------------------------------------------------
juce::String DigitalComboEditor::shortName (const juce::String& full)
{
    static const char* prefixes[] = { "D-50 Upper P1 ", "D-50 Upper P2 ", "D-50 Upper ", "D-50 ",
                                      "JD Tone A ", "JD Tone B ", "JD Tone C ", "JD Tone D ", "JD " };
    for (auto* p : prefixes)
        if (full.startsWith (p))
            return full.substring ((int) std::strlen (p));
    return full;
}

static juce::StringArray d50Ids (const juce::String& prefix)
{
    juce::StringArray s;
    for (auto& b : params::d50Bindings())
        if (b.id.startsWith (prefix)) s.add (b.id);
    return s;
}

DigitalComboEditor::DigitalComboEditor (DigitalComboProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::KeyboardComponentBase::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    auto& st = proc.apvts;
    using namespace params::id;

    globalPanel = std::make_unique<ParamPanel> (st, juce::StringArray { mode, splitPoint, splitSwap, layerBalance,
                                                                        masterVolume, sysexRx, sysexChannel, jdUnit }, 8);
    addAndMakeVisible (*globalPanel);

    d50P1 = std::make_unique<ParamPanel> (st, d50Ids ("d50_up1_"), 10, "UPPER PARTIAL 1");
    d50P2 = std::make_unique<ParamPanel> (st, d50Ids ("d50_up2_"), 10, "UPPER PARTIAL 2");
    d50PartialPage.addAndMakeVisible (*d50P1);
    d50PartialPage.addAndMakeVisible (*d50P2);

    d50Common = std::make_unique<ParamPanel> (st, d50Ids ("d50_uc_"), 6, "UPPER COMMON");
    d50Patch = std::make_unique<ParamPanel> (st, d50Ids ("d50_pt_"), 9, "PATCH");
    d50CommonPage.addAndMakeVisible (*d50Common);
    d50CommonPage.addAndMakeVisible (*d50Patch);

    for (int t = 0; t < 4; ++t)
    {
        juce::StringArray ids;
        for (auto& s : params::jdToneSuffixes()) ids.add (params::jdToneId (t, s));
        jdTones[t] = std::make_unique<ParamPanel> (st, ids, 11, "TONE " + juce::String::charToString (juce::juce_wchar ('A' + t)));
        jdTonePage.addChildComponent (*jdTones[t]);
        toneButtons[t].setButtonText (juce::String::charToString (juce::juce_wchar ('A' + t)));
        toneButtons[t].setClickingTogglesState (true);
        toneButtons[t].setRadioGroupId (1001);
        toneButtons[t].onClick = [this, t] { showJdTone (t); };
        jdTonePage.addAndMakeVisible (toneButtons[t]);
    }
    juce::StringArray jdPatchIds (params::jdCommonIds());
    jdPatchIds.addArray (params::jdEffectIds());
    jdCommon = std::make_unique<ParamPanel> (st, jdPatchIds, 10, "JD-800 PATCH COMMON / EFFECTS (Group A: DS-PH-SP-EN, Group B: CHO-DLY-REV)");
    jdFxPage.addAndMakeVisible (*jdCommon);
    showJdTone (0);

    d50PartialPage.layout = [this] (juce::Rectangle<int> r)
    {
        d50P1->setBounds (r.removeFromTop (r.getHeight() / 2).reduced (2));
        d50P2->setBounds (r.reduced (2));
    };
    d50CommonPage.layout = [this] (juce::Rectangle<int> r)
    {
        d50Common->setBounds (r.removeFromTop (r.getHeight() * 2 / 3).reduced (2));
        d50Patch->setBounds (r.reduced (2));
    };
    jdTonePage.layout = [this] (juce::Rectangle<int> r)
    {
        auto bar = r.removeFromTop (30);
        for (auto& b : toneButtons) b.setBounds (bar.removeFromLeft (50).reduced (3));
        for (auto& t : jdTones) t->setBounds (r.reduced (2));
    };
    jdFxPage.layout = [this] (juce::Rectangle<int> r)
    {
        jdCommon->setBounds (r.reduced (2));
    };

    tabs.addTab ("D-50 PARTIALS", kPanel, &d50PartialPage, false);
    tabs.addTab ("D-50 COMMON / PATCH", kPanel, &d50CommonPage, false);
    fullEditor = std::make_unique<AreaEditor> (makeD50Spec (proc));
    tabs.addTab ("D-50 FULL EDIT", kPanel, fullEditor.get(), false);
    tabs.addTab ("JD-800 TONES", kPanel, &jdTonePage, false);
    tabs.addTab ("JD-800 COMMON / EFFECTS", kPanel, &jdFxPage, false);
    jdFullEditor = std::make_unique<AreaEditor> (makeJdSpec (proc));
    tabs.addTab ("JD-800 FULL EDIT", kPanel, jdFullEditor.get(), false);
    tabs.setTabBarDepth (28);
    addAndMakeVisible (tabs);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTitle ("Preset");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx != proc.getCurrentProgram()) proc.setCurrentProgram (idx);
        shownProgram = idx;
    };
    addAndMakeVisible (presetBox);

    loadSyx.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load D-50 / JD-800 SysEx (.syx)", juce::File(), "*.syx;*.SYX");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (! f.existsAsFile()) return;
                                  juce::MemoryBlock mb;
                                  if (f.getSize() > 4 * 1024 * 1024 || ! f.loadFileAsData (mb))
                                  {
                                      lastFileMessage = "Could not read " + f.getFileName();
                                      return;
                                  }
                                  const int n = proc.importSysExData (mb.getData(), mb.getSize());
                                  lastFileMessage = f.getFileName() + ": " + juce::String (n) + " SysEx messages accepted";
                              });
    };
    saveSyx.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save D-50 + JD-800 patches as SysEx", juce::File(), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  if (! f.hasFileExtension ("syx")) f = f.withFileExtension ("syx");
                                  auto mb = proc.exportD50TempAsSysEx();       // D-50 temporary area (2 x DT1)
                                  const auto jdMb = proc.exportJdTempAsSysEx(); // JD-800 patch (2 x DT1)
                                  mb.append (jdMb.getData(), jdMb.getSize());
                                  lastFileMessage = f.replaceWithData (mb.getData(), mb.getSize())
                                                        ? "Saved " + f.getFileName() : "Could not write " + f.getFileName();
                              });
    };
    addAndMakeVisible (loadSyx);
    addAndMakeVisible (saveSyx);

    display.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    display.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0d1a12));
    display.setColour (juce::Label::textColourId, juce::Colour (0xff8cf0a8));
    display.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (display);

    keyboard.setAvailableRange (24, 108);
    keyboard.setKeyWidth (14.0f);
    addAndMakeVisible (keyboard);

    setResizable (false, false);
    setSize (1120, 760);
    startTimerHz (10);
    timerCallback();
}

DigitalComboEditor::~DigitalComboEditor()
{
    stopTimer();
    tabs.clearTabs(); // pages are members declared after the tab component
    setLookAndFeel (nullptr);
}

void DigitalComboEditor::showJdTone (int t)
{
    currentJdTone = juce::jlimit (0, 3, t);
    for (int i = 0; i < 4; ++i)
    {
        jdTones[i]->setVisible (i == currentJdTone);
        toneButtons[i].setToggleState (i == currentJdTone, juce::dontSendNotification);
    }
}

juce::StringArray DigitalComboEditor::getAttachedParameterIds() const
{
    juce::StringArray all;
    for (auto* panel : { globalPanel.get(), d50P1.get(), d50P2.get(), d50Common.get(), d50Patch.get(),
                         jdTones[0].get(), jdTones[1].get(), jdTones[2].get(), jdTones[3].get(), jdCommon.get() })
        if (panel != nullptr && panel->getNumControls() == panel->getIds().size())
            all.addArray (panel->getIds());
    return all;
}

void DigitalComboEditor::timerCallback()
{
    const auto& s = proc.sysexStats;
    juce::String text;
    text << "D-50: " << proc.getD50PatchName().paddedRight (' ', 18) << " JD-800: " << proc.getJdName().paddedRight (' ', 16)
         << "  VOICES D50:" << proc.getD50ActiveVoices() << " JD:" << proc.getJdActiveVoices()
         << "  SYSEX ok:" << s.accepted.load() << " bad:" << s.rejected.load() << " ign:" << s.ignored.load()
         << " out:" << s.replies.load();
    if (lastFileMessage.isNotEmpty()) text << "  | " << lastFileMessage;
    display.setText (text, juce::dontSendNotification);

    if (proc.getCurrentProgram() != shownProgram)
    {
        shownProgram = proc.getCurrentProgram();
        presetBox.setSelectedItemIndex (shownProgram, juce::dontSendNotification);
    }
}

void DigitalComboEditor::paint (juce::Graphics& g)
{
    g.fillAll (kPanel);
    auto top = getLocalBounds().removeFromTop (44);
    g.setColour (kPanelDark);
    g.fillRect (top);
    g.setColour (kText);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("DIGITAL COMBO", top.reduced (14, 0), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (kText.withAlpha (0.6f));
    g.drawText ("D-50 + JD-800 style engines  -  independent, unofficial, no Roland ROM",
                top.reduced (14, 0).withTrimmedLeft (190), juce::Justification::centredLeft);
    g.setColour (kD50);
    g.fillRect (0, 43, getWidth() / 2, 2);
    g.setColour (kJd);
    g.fillRect (getWidth() / 2, 43, getWidth() - getWidth() / 2, 2);
}

void DigitalComboEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (44);
    saveSyx.setBounds (top.removeFromRight (90).reduced (6, 8));
    loadSyx.setBounds (top.removeFromRight (90).reduced (6, 8));
    presetBox.setBounds (top.removeFromRight (240).reduced (6, 9));

    area.removeFromTop (4);
    display.setBounds (area.removeFromTop (24).reduced (8, 0));
    globalPanel->setBounds (area.removeFromTop (84).reduced (6, 2));
    keyboard.setBounds (area.removeFromBottom (78).reduced (6, 4));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f); // 24..108 = 50 white keys, fill the width
    tabs.setBounds (area.reduced (6, 2));
}
