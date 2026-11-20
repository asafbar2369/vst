#include "PresetBrowser.h"

namespace vsx
{
PresetBrowser::PresetBrowser (ViceSynthProcessor& p) : Section ("PRESETS", Theme::neonPink), proc (p)
{
    search.setTextToShowWhenEmpty ("Search name, tag, category...", Theme::textDim);
    search.setFont (Theme::font (13.0f));
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);

    category.addItem ("All categories", 1);
    const auto cats = FactoryPresets::categories();
    for (int i = 0; i < cats.size(); ++i) category.addItem (cats[i], i + 2);
    category.setSelectedId (1, juce::dontSendNotification);
    category.onChange = [this] { refresh(); };
    addAndMakeVisible (category);

    auto setup = [this] (juce::TextButton& b, PresetManager::SourceFilter f)
    {
        b.setClickingTogglesState (false);
        b.onClick = [this, f] { setFilter (f); };
        addAndMakeVisible (b);
    };
    setup (allB, PresetManager::SourceFilter::All);
    setup (factB, PresetManager::SourceFilter::Factory);
    setup (userB, PresetManager::SourceFilter::User);
    setup (aiB, PresetManager::SourceFilter::AI);
    setup (favB, PresetManager::SourceFilter::Favorites);

    list.setRowHeight (34);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    for (auto* b : { &saveB, &importB, &exportB, &rescanB }) addAndMakeVisible (b);
    saveB.onClick = [this] { saveAs(); };
    rescanB.onClick = [this] { proc.getPresetManager().rescan(); refresh(); };
    exportB.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export preset pack", PresetManager::rootFolder().getChildFile (juce::File::createLegalFileName (proc.getCurrentMeta().name) + PresetFormat::kPackExtension), "*" + PresetFormat::kPackExtension);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File()) return;
            juce::String err;
            proc.getPresetManager().exportPack (proc.getCurrentPatch(), fc.getResult().withFileExtension (PresetFormat::kPackExtension), err);
        });
    };
    importB.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import preset or pack", juce::File(), "*" + PresetFormat::kPackExtension + ";*" + PresetFormat::kExtension);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            PatchSpec spec;
            juce::String err;
            auto& pm = proc.getPresetManager();
            const bool ok = f.hasFileExtension (PresetFormat::kPackExtension) ? pm.importPack (f, spec, err) : pm.loadFile (f, spec, err);
            if (ok)
            {
                if (! f.hasFileExtension (PresetFormat::kPackExtension)) pm.save (spec, false, err);
                proc.applyPatch (spec, "Import " + spec.meta.name);
                refresh();
            }
        });
    };
    setFilter (PresetManager::SourceFilter::All);
}

void PresetBrowser::setFilter (PresetManager::SourceFilter f)
{
    filter = f;
    allB.setToggleState (f == PresetManager::SourceFilter::All, juce::dontSendNotification);
    factB.setToggleState (f == PresetManager::SourceFilter::Factory, juce::dontSendNotification);
    userB.setToggleState (f == PresetManager::SourceFilter::User, juce::dontSendNotification);
    aiB.setToggleState (f == PresetManager::SourceFilter::AI, juce::dontSendNotification);
    favB.setToggleState (f == PresetManager::SourceFilter::Favorites, juce::dontSendNotification);
    refresh();
}

void PresetBrowser::refresh()
{
    const auto cat = category.getSelectedId() > 1 ? category.getText() : juce::String();
    filtered = proc.getPresetManager().search (search.getText(), cat, filter);
    list.updateContent();
    list.repaint();
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) filtered.size()) return;
    const auto& e = proc.getPresetManager().getEntries()[(size_t) filtered[(size_t) row]];
    const bool current = e.name == proc.getCurrentMeta().name;
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (2.0f, 1.0f);
    if (selected || current)
    {
        g.setColour ((current ? Theme::neonPink : Theme::electricPurple).withAlpha (0.25f));
        g.fillRoundedRectangle (r, 5.0f);
    }
    g.setColour (e.favorite ? Theme::sunsetOrange : Theme::textDim.withAlpha (0.35f));
    g.setFont (Theme::font (15.0f));
    g.drawText (juce::String::fromUTF8 ("\xe2\x98\x85"), r.removeFromRight (22.0f), juce::Justification::centred);
    g.setColour (current ? juce::Colours::white : Theme::text);
    g.setFont (Theme::font (13.0f, true));
    g.drawText (e.name, r.reduced (6.0f, 0.0f).withTrimmedBottom (13.0f), juce::Justification::bottomLeft, true);
    g.setColour (e.source == "ai" ? Theme::cyberCyan : e.source == "user" ? Theme::sunsetOrange : Theme::textDim);
    g.setFont (Theme::font (10.0f));
    g.drawText (e.category + "  |  " + e.source.toUpperCase(), r.reduced (6.0f, 0.0f).withTrimmedTop (18.0f), juce::Justification::topLeft, true);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& ev)
{
    if (row < 0 || row >= (int) filtered.size()) return;
    const auto e = proc.getPresetManager().getEntries()[(size_t) filtered[(size_t) row]];
    if (ev.x > list.getWidth() - 34)
    {
        proc.getPresetManager().setFavorite (e, ! e.favorite);
        refresh();
        return;
    }
    proc.loadPresetEntry (e);
    list.repaint();
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) filtered.size())
        proc.audition (60, 100, 1.4);
}

void PresetBrowser::saveAs()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name, category and tags for your preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.getCurrentMeta().name, "Name");
    w->addComboBox ("category", FactoryPresets::categories(), "Category");
    w->addTextEditor ("tags", proc.getCurrentMeta().tags.joinIntoString (", "), "Tags (comma separated)");
    if (auto* c = w->getComboBoxComponent ("category"))
        c->setText (proc.getCurrentMeta().category.isNotEmpty() ? proc.getCurrentMeta().category : juce::String ("Analog"));
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBrowser> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe != nullptr && result == 1)
        {
            auto spec = safe->proc.getCurrentPatch();
            spec.meta.name = w->getTextEditorContents ("name").trim().isNotEmpty() ? w->getTextEditorContents ("name").trim() : juce::String ("Untitled");
            if (auto* c = w->getComboBoxComponent ("category")) spec.meta.category = c->getText();
            spec.meta.tags = juce::StringArray::fromTokens (w->getTextEditorContents ("tags"), ",", "");
            spec.meta.tags.trim();
            spec.meta.tags.removeEmptyStrings();
            spec.meta.source = "user";
            spec.meta.author = juce::SystemStats::getFullUserName().isNotEmpty() ? juce::SystemStats::getFullUserName() : juce::String ("User");
            juce::String err;
            safe->proc.getPresetManager().save (spec, false, err);
            safe->proc.setCurrentMeta (spec.meta);
            safe->refresh();
        }
    }), true);
}

void PresetBrowser::resized()
{
    auto r = content();
    search.setBounds (r.removeFromTop (26));
    r.removeFromTop (4);
    category.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    layoutRow (r.removeFromTop (22), { &allB, &factB, &userB, &aiB, &favB }, 2);
    r.removeFromTop (4);
    auto bottom = r.removeFromBottom (50);
    layoutRow (bottom.removeFromTop (24), { &saveB, &rescanB }, 3);
    bottom.removeFromTop (2);
    layoutRow (bottom.removeFromTop (24), { &importB, &exportB }, 3);
    list.setBounds (r.withTrimmedBottom (4));
}

} // namespace vsx
