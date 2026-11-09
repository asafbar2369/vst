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

void ParamPanel::setLabels (const juce::StringArray& texts)
{
    for (int i = 0; i < juce::jmin (texts.size(), (int) cells.size()); ++i)
        cells[(size_t) i]->label->setText (texts[i], juce::dontSendNotification);
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

// ---------------------------------------------------------------------------
void ZoneView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (kPanelDark);
    g.fillRoundedRectangle (r, 4.0f);
    r.reduce (8.0f, 6.0f);
    const float labelW = 70.0f;
    auto keys = r.withTrimmedLeft (labelW);
    const float kw = keys.getWidth() / 128.0f;
    const int mode = proc.getCurrentMode();
    const float rowH = (keys.getHeight() - 16.0f) / 2.0f;

    for (int row = 0; row < 2; ++row)
    {
        const bool jdRow = row == 1;
        const float y = keys.getY() + float (row) * rowH;
        g.setColour (jdRow ? kJd : kD50);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (jdRow ? "JD-800" : "D-50", juce::Rectangle<float> (r.getX(), y, labelW, rowH), juce::Justification::centredLeft);
        for (int k = 0; k < 128; ++k)
        {
            bool toD50 = false, toJd = false;
            proc.getRouting (k, mode, toD50, toJd);
            const bool on = jdRow ? toJd : toD50;
            const bool black = juce::MidiMessage::isMidiNoteBlack (k);
            g.setColour (on ? (jdRow ? kJd : kD50).withAlpha (black ? 0.7f : 1.0f) : juce::Colour (0xff30343a));
            g.fillRect (keys.getX() + float (k) * kw, y + 2.0f, std::max (1.0f, kw - 0.5f), rowH - 4.0f);
        }
        const int t = proc.getTranspose (jdRow);
        if (t != 0)
        {
            g.setColour (kText);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("transpose " + juce::String (t > 0 ? "+" : "") + juce::String (t),
                        juce::Rectangle<float> (keys.getRight() - 110.0f, y, 108.0f, rowH), juce::Justification::centredRight);
        }
    }
    // Octave labels C-1 .. C9 (MIDI note 60 = C4)
    g.setColour (kText.withAlpha (0.6f));
    g.setFont (juce::FontOptions (10.0f));
    for (int k = 0; k < 128; k += 12)
        g.drawText ("C" + juce::String (k / 12 - 1), juce::Rectangle<float> (keys.getX() + float (k) * kw, keys.getBottom() - 14.0f, 30.0f, 14.0f),
                    juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------
ComboPage::ComboPage (DigitalComboProcessor& p) : proc (p), zones (p)
{
    using namespace params::id;
    mixer = std::make_unique<ParamPanel> (proc.apvts, juce::StringArray { d50Level, d50Pan, d50Transpose, d50KeyLow, d50KeyHigh,
                                                                          jdLevel, jdPan, jdTranspose, jdKeyLow, jdKeyHigh },
                                          5, "ENGINE MIXER AND KEY ZONES");
    mixer->setLabels ({ "D-50 Level (dB)", "D-50 Pan", "D-50 Transpose", "D-50 Lowest Key", "D-50 Highest Key",
                        "JD-800 Level (dB)", "JD-800 Pan", "JD-800 Transpose", "JD-800 Lowest Key", "JD-800 Highest Key" });
    addAndMakeVisible (*mixer);
    addAndMakeVisible (zones);

    for (auto* b : { &savePatch, &loadPatch, &openPatchFolder })
        addAndMakeVisible (*b);
    patchFolderLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (patchFolderLabel);

    savePatch.onClick = [this]
    {
        auto folder = proc.getPatchFolder();
        folder.createDirectory();
        auto name = proc.getCurrentPatchName().isNotEmpty() ? proc.getCurrentPatchName() : juce::String ("New Combo");
        chooser = std::make_unique<juce::FileChooser> ("Save Digital Combo patch", folder.getChildFile (juce::File::createLegalFileName (name) + ".dcpatch"), "*.dcpatch");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  f = f.withFileExtension ("dcpatch");
                                  status = proc.savePatchFile (f, f.getFileNameWithoutExtension()) ? "Saved " + f.getFileName()
                                                                                                    : "Could not save " + f.getFileName();
                                  if (onPatchesChanged) onPatchesChanged();
                                  repaint();
                              });
    };
    loadPatch.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load Digital Combo patch", proc.getPatchFolder(), "*.dcpatch");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (! f.existsAsFile()) return;
                                  status = proc.loadPatchFile (f) ? "Loaded " + f.getFileName() : f.getFileName() + " is not a valid patch";
                                  repaint();
                              });
    };
    openPatchFolder.onClick = [this]
    {
        auto folder = proc.getPatchFolder();
        folder.createDirectory();
        folder.startAsProcess();
    };

    for (int e = 0; e < 2; ++e)
    {
        const bool jd = e == 1;
        chooseWaves[e].setButtonText ("Choose folder...");
        clearWaves[e].setButtonText ("Use built-in");
        chooseWaves[e].onClick = [this, jd] { chooseWaveFolder (jd); };
        clearWaves[e].onClick = [this, jd] { status = proc.setUserWaveFolder (jd, juce::File()); refresh(); };
        waveLabels[e].setFont (juce::FontOptions (12.0f));
        waveCounts[e].setFont (juce::FontOptions (12.0f, juce::Font::bold));
        waveCounts[e].setColour (juce::Label::textColourId, jd ? kJd : kD50);
        for (juce::Component* c : { (juce::Component*) &chooseWaves[e], (juce::Component*) &clearWaves[e],
                                    (juce::Component*) &waveLabels[e], (juce::Component*) &waveCounts[e] })
            addAndMakeVisible (*c);
    }
    waveInfo.setMultiLine (true);
    waveInfo.setReadOnly (true);
    waveInfo.setScrollbarsShown (true);
    waveInfo.setFont (juce::FontOptions (12.0f));
    waveInfo.setColour (juce::TextEditor::backgroundColourId, kPanelDark);
    waveInfo.setColour (juce::TextEditor::textColourId, kText);
    addAndMakeVisible (waveInfo);

    rendererBox.addItem ("Software (recommended)", 1);
    rendererBox.addItem ("Direct2D (Windows GPU)", 2);
    rendererBox.setTitle ("Editor drawing");
    rendererBox.setSelectedItemIndex (juce::jlimit (0, 1, proc.guiRenderer.load()), juce::dontSendNotification);
    rendererBox.onChange = [this] { proc.guiRenderer = rendererBox.getSelectedItemIndex(); };
    addAndMakeVisible (rendererBox);
    rendererLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (rendererLabel);
    refresh();
}

void ComboPage::chooseWaveFolder (bool jd)
{
    auto start = proc.getUserWaveFolder (jd);
    chooser = std::make_unique<juce::FileChooser> (jd ? "Folder with your JD-800 waveform files (001.wav ... 108.wav)"
                                                      : "Folder with your D-50 PCM files (001.wav ... 100.wav)",
                                                   start.isDirectory() ? start : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this, jd] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (! f.isDirectory()) return;
                              status = proc.setUserWaveFolder (jd, f);
                              refresh();
                          });
}

void ComboPage::refresh()
{
    patchFolderLabel.setText ("Patch folder: " + proc.getPatchFolder().getFullPathName(), juce::dontSendNotification);
    bool changed = false;
    for (int e = 0; e < 2; ++e)
    {
        const bool jd = e == 1;
        const auto folder = proc.getUserWaveFolder (jd);
        waveLabels[e].setText (juce::String (jd ? "JD-800 waveforms 1-108: " : "D-50 PCM waves 1-100: ")
                                   + (folder == juce::File() ? juce::String ("built-in substitutes") : folder.getFullPathName()),
                               juce::dontSendNotification);
        const int n = proc.getUserWaveCount (jd);
        waveCounts[e].setText (juce::String (n) + " from your files", juce::dontSendNotification);
        changed = changed || n != shownCounts[e];
        shownCounts[e] = n;
    }
    if (changed || waveInfo.isEmpty())
    {
        juce::String t;
        t << "Your own waves: put WAV or AIFF files in a folder and choose it above. Each file name must start with the "
             "wave number (for example \"001 Marimba.wav\" for D-50 PCM 1, \"012.wav\" for JD-800 waveform 12). "
             "The root key is read from the file's sampler data (otherwise middle C), and its sustain loop is used "
             "(otherwise the file plays once). Numbers without a file keep the built-in substitute. Nothing is copied "
             "into the plug-in: only the folder location is saved with your project.\n";
        if (status.isNotEmpty()) t << "\n" << status << "\n";
        for (auto& m : proc.getUserWaveMessages()) t << m << "\n";
        waveInfo.setText (t, false);
    }
    rendererBox.setSelectedItemIndex (juce::jlimit (0, 1, proc.guiRenderer.load()), juce::dontSendNotification);
    zones.repaint();
}

void ComboPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (4);
    r.removeFromTop (248); // mixer (170) + zones (74) + gap, as laid out in resized()
    g.setColour (kText);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("PATCH FILES", r.removeFromTop (20).reduced (6, 0), juce::Justification::centredLeft);
    r.removeFromTop (34);
    g.drawText ("YOUR OWN WAVES (optional)", r.removeFromTop (20).reduced (6, 0), juce::Justification::centredLeft);
    if (status.isNotEmpty())
    {
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (kText.withAlpha (0.7f));
        g.drawText (status, getLocalBounds().reduced (10, 6).removeFromBottom (18), juce::Justification::centredRight);
    }
}

void ComboPage::resized()
{
    auto r = getLocalBounds().reduced (4);
    mixer->setBounds (r.removeFromTop (170).reduced (2));
    zones.setBounds (r.removeFromTop (74).reduced (2));
    r.removeFromTop (4);
    r.removeFromTop (20); // "PATCH FILES" heading
    {
        auto row = r.removeFromTop (34).reduced (6, 4);
        savePatch.setBounds (row.removeFromLeft (120));
        row.removeFromLeft (6);
        loadPatch.setBounds (row.removeFromLeft (120));
        row.removeFromLeft (6);
        openPatchFolder.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (10);
        patchFolderLabel.setBounds (row);
    }
    r.removeFromTop (20); // "YOUR OWN WAVES" heading
    for (int e = 0; e < 2; ++e)
    {
        auto row = r.removeFromTop (32).reduced (6, 3);
        chooseWaves[e].setBounds (row.removeFromLeft (130));
        row.removeFromLeft (6);
        clearWaves[e].setBounds (row.removeFromLeft (100));
        row.removeFromLeft (10);
        waveCounts[e].setBounds (row.removeFromRight (150));
        waveLabels[e].setBounds (row);
    }
    auto bottom = r.removeFromBottom (32).reduced (6, 4);
    rendererLabel.setBounds (bottom.removeFromLeft (110));
    rendererBox.setBounds (bottom.removeFromLeft (220));
    waveInfo.setBounds (r.reduced (6, 4));
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
    comboPage = std::make_unique<ComboPage> (proc);
    comboPage->onPatchesChanged = [this] { refreshPatchBrowser(); };
    tabs.addTab ("COMBO: MIXER / ZONES / FILES", kPanel, comboPage.get(), false);
    tabs.setTabBarDepth (28);
    addAndMakeVisible (tabs);

    presetBox.setTitle ("Patch browser");
    presetBox.setTextWhenNothingSelected ("Patches...");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= kUserPatchIdBase)
        {
            const int i = id - kUserPatchIdBase;
            if (juce::isPositiveAndBelow (i, listedPatches.size()))
                lastFileMessage = proc.loadPatchFile (listedPatches[i]) ? "Loaded " + listedPatches[i].getFileName()
                                                                         : listedPatches[i].getFileName() + " is not a valid patch";
        }
        else if (id > 0)
        {
            const int idx = id - 1;
            if (idx != proc.getCurrentProgram() || proc.getCurrentPatchName() != proc.getProgramName (idx))
                proc.setCurrentProgram (idx);
            shownProgram = idx;
        }
    };
    refreshPatchBrowser();
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

    // Opaque: the whole editor is painted, so hosts never show through (no transparent areas).
    setOpaque (true);
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

void DigitalComboEditor::refreshPatchBrowser()
{
    const auto files = proc.findUserPatches();
    if (files == listedPatches && presetBox.getNumItems() > 0)
        return;
    listedPatches = files;
    const int selected = presetBox.getSelectedId();
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory (original sounds)");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.addSectionHeading ("Your patches (" + proc.getPatchFolder().getFileName() + " folder)");
    if (files.isEmpty())
        presetBox.addItem ("(none saved yet - use COMBO > Save patch)", kUserPatchIdBase - 1);
    presetBox.setItemEnabled (kUserPatchIdBase - 1, false);
    for (int i = 0; i < files.size(); ++i)
        presetBox.addItem (files[i].getFileNameWithoutExtension(), kUserPatchIdBase + i);
    if (selected > 0) presetBox.setSelectedId (selected, juce::dontSendNotification);
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
                         jdTones[0].get(), jdTones[1].get(), jdTones[2].get(), jdTones[3].get(), jdCommon.get(),
                         comboPage != nullptr ? &comboPage->getMixer() : nullptr })
        if (panel != nullptr && panel->getNumControls() == panel->getIds().size())
            all.addArray (panel->getIds());
    return all;
}

void DigitalComboEditor::timerCallback()
{
    const auto& s = proc.sysexStats;
    juce::String text;
    const auto patchName = proc.getCurrentPatchName().isNotEmpty() ? proc.getCurrentPatchName() : juce::String ("(not saved)");
    text << "PATCH: " << patchName.substring (0, 24).paddedRight (' ', 24)
         << " D-50: " << proc.getD50PatchName().paddedRight (' ', 18) << " JD-800: " << proc.getJdName().paddedRight (' ', 16)
         << "  VOICES D50:" << proc.getD50ActiveVoices() << " JD:" << proc.getJdActiveVoices()
         << "  SYSEX ok:" << s.accepted.load() << " bad:" << s.rejected.load() << " ign:" << s.ignored.load()
         << " out:" << s.replies.load();
    if (lastFileMessage.isNotEmpty()) text << "  | " << lastFileMessage;
    display.setText (text, juce::dontSendNotification);

    if (proc.getCurrentProgram() != shownProgram)
    {
        shownProgram = proc.getCurrentProgram();
        presetBox.setSelectedId (shownProgram + 1, juce::dontSendNotification);
    }

    // Apply the chosen renderer to this editor's window (Windows: 0 = software/GDI, 1 = Direct2D).
    if (auto* peer = getPeer())
    {
        const int wanted = proc.guiRenderer.load();
        if (juce::isPositiveAndBelow (wanted, peer->getAvailableRenderingEngines().size())
            && peer->getCurrentRenderingEngine() != wanted)
            peer->setCurrentRenderingEngine (wanted);
    }

    if (++timerTicks % 20 == 0) // every 2 s: pick up patch files saved outside the plug-in
        refreshPatchBrowser();
    if (comboPage != nullptr && comboPage->isShowing())
        comboPage->refresh();
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
