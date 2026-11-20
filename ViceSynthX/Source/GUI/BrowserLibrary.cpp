#include "BrowserLibrary.h"
#include "../AI/PatchOps.h"

namespace vsx
{
static bool matchesSource (const PresetManager::Entry& e, int source)
{
    if (source == BrowserModel::Factory) return e.source == "factory";
    if (source == BrowserModel::User) return e.source != "factory";
    return e.favorite;
}

void BrowserModel::update (PresetManager& pm)
{
    results.clear();
    const auto words = juce::StringArray::fromTokens (search.toLowerCase(), " ,", "");
    const auto& entries = pm.getEntries();
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        if (! matchesSource (e, source)) continue;
        if (category.isNotEmpty() && ! e.category.equalsIgnoreCase (category)) continue;
        const auto hay = (e.name + " " + e.category + " " + e.tags.joinIntoString (" ") + " " + e.description).toLowerCase();
        bool ok = true;
        for (const auto& w : words) if (w.isNotEmpty() && ! hay.contains (w)) { ok = false; break; }
        if (ok) results.push_back ((int) i);
    }
    if (onChanged) onChanged();
}

int BrowserModel::countFor (PresetManager& pm, const juce::String& cat) const
{
    int n = 0;
    for (const auto& e : pm.getEntries())
        if (matchesSource (e, source) && (cat.isEmpty() || e.category.equalsIgnoreCase (cat))) ++n;
    return n;
}

void showSavePresetDialog (ViceSynthProcessor& proc, std::function<void()> onSaved)
{
    auto* w = new juce::AlertWindow ("Save preset", "Name, category and tags for your preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.getCurrentMeta().name, "Name");
    w->addComboBox ("category", FactoryPresets::categories(), "Category");
    w->addTextEditor ("tags", proc.getCurrentMeta().tags.joinIntoString (", "), "Tags (comma separated)");
    if (auto* c = w->getComboBoxComponent ("category"))
        c->setText (proc.getCurrentMeta().category.isNotEmpty() ? proc.getCurrentMeta().category : juce::String ("Analog"));
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([&proc, w, onSaved] (int result)
    {
        if (result != 1) return;
        auto spec = proc.getCurrentPatch();
        spec.meta.name = w->getTextEditorContents ("name").trim().isNotEmpty() ? w->getTextEditorContents ("name").trim() : juce::String ("Untitled");
        if (auto* c = w->getComboBoxComponent ("category")) spec.meta.category = c->getText();
        spec.meta.tags = juce::StringArray::fromTokens (w->getTextEditorContents ("tags"), ",", "");
        spec.meta.tags.trim();
        spec.meta.tags.removeEmptyStrings();
        spec.meta.source = "user";
        spec.meta.author = juce::SystemStats::getFullUserName().isNotEmpty() ? juce::SystemStats::getFullUserName() : juce::String ("User");
        juce::String err;
        proc.getPresetManager().save (spec, false, err);
        proc.setCurrentMeta (spec.meta);
        if (onSaved) onSaved();
    }), true);
}

// =================================================================================
class BrowserPanel::CategoryList : public juce::Component
{
public:
    CategoryList (ViceSynthProcessor& p, BrowserModel& m) : proc (p), model (m) {}
    static constexpr int kRow = 28;
    juce::StringArray names() const
    {
        juce::StringArray n { "" };
        n.addArray (FactoryPresets::categories());
        for (const auto& e : proc.getPresetManager().getEntries())
            if (e.category.isNotEmpty() && ! n.contains (e.category, true)) n.add (e.category);
        return n;
    }
    void paint (juce::Graphics& g) override
    {
        const auto n = names();
        for (int i = 0; i < n.size(); ++i)
        {
            auto r = juce::Rectangle<float> (0.0f, (float) (i * kRow), (float) getWidth(), (float) kRow - 2.0f);
            const bool sel = model.category.equalsIgnoreCase (n[i]);
            if (sel)
            {
                juce::ColourGradient grad (Theme::neonPink.withAlpha (0.32f), r.getX(), 0.0f, Theme::neonPink.withAlpha (0.08f), r.getRight(), 0.0f, false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (r, 5.0f);
                g.setColour (Theme::neonPink.withAlpha (0.6f));
                g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
            }
            else if (i == hover)
            {
                g.setColour (juce::Colours::white.withAlpha (0.04f));
                g.fillRoundedRectangle (r, 5.0f);
            }
            Icons::draw (g, i == 0 ? Icons::Type::Wave : (i % 3 == 0 ? Icons::Type::Audio : (i % 3 == 1 ? Icons::Type::Wave : Icons::Type::Keyboard)),
                         r.withWidth (34.0f).reduced (9.0f, 7.0f), sel ? Theme::neonPink : Theme::textDim, 1.3f);
            g.setColour (sel ? juce::Colours::white : Theme::text);
            g.setFont (Theme::font (13.5f, sel));
            g.drawText (i == 0 ? juce::String ("All Presets") : n[i], r.withTrimmedLeft (38.0f), juce::Justification::centredLeft, true);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (12.0f));
            g.drawText (juce::String (model.countFor (proc.getPresetManager(), n[i])), r.withTrimmedRight (10.0f), juce::Justification::centredRight);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = e.y / kRow; if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto n = names();
        const int i = e.y / kRow;
        if (i >= 0 && i < n.size())
        {
            model.category = n[i];
            model.update (proc.getPresetManager());
            repaint();
        }
    }
    int heightNeeded() const { return names().size() * kRow; }

private:
    ViceSynthProcessor& proc;
    BrowserModel& model;
    int hover = -1;
};

BrowserPanel::BrowserPanel (ViceSynthProcessor& p, BrowserModel& m) : proc (p), model (m)
{
    search.setTextToShowWhenEmpty ("Search presets...", Theme::textDim);
    search.setFont (Theme::font (13.5f));
    search.setIndents (30, 7);
    search.onTextChange = [this] { model.search = search.getText(); model.update (proc.getPresetManager()); };
    addAndMakeVisible (search);
    addAndMakeVisible (options);
    options.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Save current sound as...");
        menu.addItem (2, "Import preset or pack...");
        menu.addItem (3, "Export current sound as pack...");
        menu.addItem (4, "Rescan library");
        menu.addSeparator();
        menu.addItem (5, "Initialise patch");
        menu.addItem (6, "Restore last session (crash recovery)");
        menu.addItem (7, "Open preset folder");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&options), [this] (int r)
        {
            auto& pm = proc.getPresetManager();
            if (r == 1) showSavePresetDialog (proc, [this] { model.update (proc.getPresetManager()); });
            else if (r == 4) { pm.rescan(); model.update (pm); }
            else if (r == 5) { auto s = PatchOps::initPatch(); s.meta.name = "Init"; proc.applyPatch (s, "Init patch"); }
            else if (r == 6)
            {
                PatchSpec s;
                if (pm.readRecovery (s)) proc.applyPatch (s, "Restore session");
                else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Restore session", "No recovery snapshot was found.");
            }
            else if (r == 7) PresetManager::rootFolder().startAsProcess();
            else if (r == 2 || r == 3)
            {
                const bool imp = r == 2;
                chooser = std::make_unique<juce::FileChooser> (imp ? "Import preset or pack" : "Export preset pack",
                    imp ? juce::File() : PresetManager::rootFolder().getChildFile (juce::File::createLegalFileName (proc.getCurrentMeta().name) + PresetFormat::kPackExtension),
                    imp ? "*" + PresetFormat::kPackExtension + ";*" + PresetFormat::kExtension : "*" + PresetFormat::kPackExtension);
                const int flags = imp ? juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                      : juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
                chooser->launchAsync (flags, [this, imp] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (f == juce::File()) return;
                    auto& pm2 = proc.getPresetManager();
                    juce::String err;
                    if (! imp) { pm2.exportPack (proc.getCurrentPatch(), f.withFileExtension (PresetFormat::kPackExtension), err); return; }
                    PatchSpec spec;
                    const bool ok = f.hasFileExtension (PresetFormat::kPackExtension) ? pm2.importPack (f, spec, err) : pm2.loadFile (f, spec, err);
                    if (ok)
                    {
                        if (! f.hasFileExtension (PresetFormat::kPackExtension)) pm2.save (spec, false, err);
                        proc.applyPatch (spec, "Import " + spec.meta.name);
                        model.update (pm2);
                    }
                });
            }
        });
    };
    IconButton* tabs[] = { &factoryTab, &userTab, &favTab };
    for (int i = 0; i < 3; ++i)
    {
        tabs[i]->accent = Theme::neonPink;
        tabs[i]->onClick = [this, i] { model.source = i; model.update (proc.getPresetManager()); };
        addAndMakeVisible (tabs[i]);
    }
    categories = std::make_unique<CategoryList> (p, m);
    categoryView.setViewedComponent (categories.get(), false);
    categoryView.setScrollBarsShown (true, false);
    categoryView.setScrollBarThickness (6);
    addAndMakeVisible (categoryView);
    results.setRowHeight (26);
    results.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (results);
    refresh();
}

void BrowserPanel::refresh()
{
    factoryTab.setToggleState (model.source == BrowserModel::Factory, juce::dontSendNotification);
    userTab.setToggleState (model.source == BrowserModel::User, juce::dontSendNotification);
    favTab.setToggleState (model.source == BrowserModel::Favorites, juce::dontSendNotification);
    categories->setSize (categoryView.getWidth() - 8, categories->heightNeeded());
    categories->repaint();
    results.updateContent();
    results.repaint();
}

void BrowserPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Theme::card (g, r, 10.0f);
    Icons::draw (g, Icons::Type::Folder, juce::Rectangle<float> (18.0f, 16.0f, 22.0f, 20.0f), Theme::text, 1.5f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (15.0f, true));
    g.drawText ("PRESET BROWSER", juce::Rectangle<float> (50.0f, 14.0f, 220.0f, 24.0f), juce::Justification::left);
    Icons::draw (g, Icons::Type::Search, search.getBounds().toFloat().withWidth (30.0f).reduced (8.0f), Theme::textDim, 1.4f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.5f, true).withExtraKerningFactor (0.1f));
    g.drawText ("CATEGORIES", juce::Rectangle<float> (18.0f, (float) categoryView.getY() - 22.0f, 200.0f, 18.0f), juce::Justification::left);
    g.drawText ("SOUNDS  (" + juce::String ((int) model.results.size()) + ")", juce::Rectangle<float> (18.0f, (float) results.getY() - 22.0f, 200.0f, 18.0f), juce::Justification::left);
    g.setColour (Theme::border);
    g.drawHorizontalLine (results.getY() - 26, 14.0f, r.getRight() - 14.0f);
}

void BrowserPanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (34);
    auto sr = r.removeFromTop (34);
    options.setBounds (sr.removeFromRight (32));
    sr.removeFromRight (6);
    search.setBounds (sr);
    r.removeFromTop (10);
    auto tabsRow = r.removeFromTop (32);
    const int tw = tabsRow.getWidth() / 3;
    factoryTab.setBounds (tabsRow.removeFromLeft (tw).reduced (2, 0));
    userTab.setBounds (tabsRow.removeFromLeft (tw).reduced (2, 0));
    favTab.setBounds (tabsRow.reduced (2, 0));
    r.removeFromTop (30);
    const int catH = (int) (r.getHeight() * 0.56f);
    categoryView.setBounds (r.removeFromTop (catH));
    r.removeFromTop (30);
    results.setBounds (r);
    refresh();
}

void BrowserPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) model.results.size()) return;
    const auto& e = proc.getPresetManager().getEntries()[(size_t) model.results[(size_t) row]];
    const bool current = e.name == proc.getCurrentMeta().name;
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (1.0f, 1.0f);
    if (current || selected)
    {
        g.setColour ((current ? Theme::neonPink : Theme::electricPurple).withAlpha (current ? 0.22f : 0.12f));
        g.fillRoundedRectangle (r, 4.0f);
    }
    Icons::draw (g, e.favorite ? Icons::Type::HeartFilled : Icons::Type::Heart, r.removeFromRight (22.0f).reduced (4.0f),
                 e.favorite ? Theme::neonPink : Theme::textDim.withAlpha (0.4f), 1.2f);
    g.setColour (current ? juce::Colours::white : Theme::text);
    g.setFont (Theme::font (13.0f, current));
    g.drawText (e.name, r.reduced (8.0f, 0.0f), juce::Justification::centredLeft, true);
    if (e.source == "ai")
    {
        g.setColour (Theme::cyberCyan);
        g.setFont (Theme::font (9.5f, true));
        g.drawText ("AI", r.withTrimmedRight (2.0f), juce::Justification::centredRight);
    }
}

void BrowserPanel::listBoxItemClicked (int row, const juce::MouseEvent& ev)
{
    if (row < 0 || row >= (int) model.results.size()) return;
    const auto e = proc.getPresetManager().getEntries()[(size_t) model.results[(size_t) row]];
    if (ev.x > results.getWidth() - 30) { proc.getPresetManager().setFavorite (e, ! e.favorite); model.update (proc.getPresetManager()); return; }
    proc.loadPresetEntry (e);
    results.repaint();
}

void BrowserPanel::listBoxItemDoubleClicked (int, const juce::MouseEvent&) { proc.audition (60, 100, 1.4); }

void BrowserPanel::returnKeyPressed (int row)
{
    if (row >= 0 && row < (int) model.results.size())
        proc.loadPresetEntry (proc.getPresetManager().getEntries()[(size_t) model.results[(size_t) row]]);
}

// =================================================================================
class LibraryView::Grid : public juce::Component
{
public:
    Grid (ViceSynthProcessor& p, BrowserModel& m) : proc (p), model (m) { setWantsKeyboardFocus (true); }
    static constexpr int kCols = 4, kCardH = 132;
    int selected = 0;
    void layoutFor (int width) { setSize (width, std::max (100, (((int) model.results.size() + kCols - 1) / kCols) * (kCardH + 10))); }
    juce::Rectangle<float> cardBounds (int i) const
    {
        const float cw = (float) getWidth() / kCols;
        return { (float) (i % kCols) * cw + 5.0f, (float) (i / kCols) * (kCardH + 10) + 5.0f, cw - 10.0f, (float) kCardH };
    }
    void paint (juce::Graphics& g) override
    {
        const auto& entries = proc.getPresetManager().getEntries();
        for (int i = 0; i < (int) model.results.size(); ++i)
        {
            const auto r = cardBounds (i);
            if (! g.clipRegionIntersects (r.toNearestInt())) continue;
            const auto& e = entries[(size_t) model.results[(size_t) i]];
            const bool current = e.name == proc.getCurrentMeta().name;
            Theme::card (g, r, 8.0f);
            const auto accent = Theme::accentFor ((int) (e.category.hashCode() & 0x7fffffff));
            auto art = r.reduced (6.0f).withHeight (66.0f);
            juce::ColourGradient grad (accent.withAlpha (0.55f), art.getX(), art.getBottom(), Theme::display, art.getX(), art.getY(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (art, 6.0f);
            Rng rng ((uint32_t) e.name.hashCode());
            g.setColour (Theme::deepBlack.withAlpha (0.8f));
            for (float x = art.getX(); x < art.getRight() - 4.0f; x += 6.0f + rng.uni() * 8.0f)
            {
                const float hh = 10.0f + rng.uni() * 34.0f;
                g.fillRect (x, art.getBottom() - hh, 5.0f + rng.uni() * 7.0f, hh);
            }
            g.setColour (Theme::text);
            g.setFont (Theme::font (14.0f, true));
            g.drawText (e.name, r.reduced (10.0f, 0.0f).withTrimmedTop (76.0f).withHeight (20.0f), juce::Justification::left, true);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (11.0f));
            g.drawText (e.category.toUpperCase() + (e.tags.isEmpty() ? juce::String() : juce::String::fromUTF8 ("  \xe2\x80\xa2  ") + e.tags.joinIntoString (", ").toUpperCase()),
                        r.reduced (10.0f, 0.0f).withTrimmedTop (98.0f).withHeight (16.0f), juce::Justification::left, true);
            if (e.favorite) Icons::draw (g, Icons::Type::HeartFilled, juce::Rectangle<float> (art.getRight() - 22.0f, art.getY() + 6.0f, 16.0f, 16.0f), Theme::neonPink);
            if (current || i == selected)
            {
                g.setColour ((current ? Theme::neonPink : Theme::cyberCyan).withAlpha (0.85f));
                g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, current ? 1.6f : 1.0f);
            }
        }
        if (model.results.empty())
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (14.0f));
            g.drawText ("No presets match the current search.", getLocalBounds(), juce::Justification::centredTop);
        }
    }
    int indexAt (juce::Point<int> p) const
    {
        for (int i = 0; i < (int) model.results.size(); ++i) if (cardBounds (i).contains (p.toFloat())) return i;
        return -1;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        const int i = indexAt (e.getPosition());
        if (i < 0) return;
        selected = i;
        proc.loadPresetEntry (proc.getPresetManager().getEntries()[(size_t) model.results[(size_t) i]]);
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { proc.audition (60, 100, 1.4); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        const int n = (int) model.results.size();
        if (n == 0) return false;
        int s = selected;
        if (k == juce::KeyPress::rightKey) ++s;
        else if (k == juce::KeyPress::leftKey) --s;
        else if (k == juce::KeyPress::downKey) s += kCols;
        else if (k == juce::KeyPress::upKey) s -= kCols;
        else if (k == juce::KeyPress::returnKey) { proc.loadPresetEntry (proc.getPresetManager().getEntries()[(size_t) model.results[(size_t) selected]]); repaint(); return true; }
        else return false;
        selected = juce::jlimit (0, n - 1, s);
        if (auto* vp = findParentComponentOfClass<juce::Viewport>()) vp->autoScroll (0, (int) cardBounds (selected).getCentreY() - vp->getViewPositionY(), 40, 40);
        repaint();
        return true;
    }

private:
    ViceSynthProcessor& proc;
    BrowserModel& model;
};

LibraryView::LibraryView (ViceSynthProcessor& p, BrowserModel& m) : proc (p), model (m)
{
    grid = std::make_unique<Grid> (p, m);
    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (7);
    addAndMakeVisible (viewport);
}

void LibraryView::refresh()
{
    grid->layoutFor (std::max (200, viewport.getWidth() - 10));
    grid->repaint();
}

bool LibraryView::keyPressed (const juce::KeyPress& k) { return grid->keyPressed (k); }

void LibraryView::paint (juce::Graphics& g)
{
    Theme::card (g, getLocalBounds().toFloat(), 10.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (14.0f, true));
    g.drawText ("LIBRARY  -  " + (model.category.isEmpty() ? juce::String ("All Presets") : model.category) + "  (" + juce::String ((int) model.results.size()) + ")",
                getLocalBounds().reduced (16, 10).removeFromTop (22), juce::Justification::left);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.5f));
    g.drawText ("Click to load  |  double-click to audition  |  arrow keys + Enter", getLocalBounds().reduced (16, 10).removeFromTop (22), juce::Justification::right);
}

void LibraryView::resized()
{
    viewport.setBounds (getLocalBounds().reduced (10).withTrimmedTop (30));
    refresh();
}

BrowserPanel::~BrowserPanel() = default;
LibraryView::~LibraryView() = default;

} // namespace vsx
