#pragma once

#include "Icons.h"
#include "Widgets.h"

namespace vsx
{
/** Shared browser state (search, source tab, category) and the filtered result list. */
struct BrowserModel
{
    enum Source { Factory = 0, User = 1, Favorites = 2 };
    juce::String search, category; // empty category = all
    int source = Factory;
    std::vector<int> results;      // indices into PresetManager::getEntries()
    std::function<void()> onChanged;
    void update (PresetManager& pm);
    int countFor (PresetManager& pm, const juce::String& category) const;
};

void showSavePresetDialog (ViceSynthProcessor& proc, std::function<void()> onSaved);

/** Left panel: PRESET BROWSER with search, Factory / User / Favorites, categories with real counts and results. */
class BrowserPanel : public juce::Component, private juce::ListBoxModel, private juce::KeyListener
{
public:
    BrowserPanel (ViceSynthProcessor& p, BrowserModel& m);
    ~BrowserPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    class CategoryList;
    int getNumRows() override { return (int) model.results.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    bool keyPressed (const juce::KeyPress&, juce::Component* origin) override;

    ViceSynthProcessor& proc;
    BrowserModel& model;
    juce::TextEditor search;
    IconButton options { Icons::Type::Filter, "Preset actions" };
    IconButton factoryTab { Icons::Type::Box, "Factory presets", "Factory" }, userTab { Icons::Type::User, "Your presets and AI-generated presets", "User" },
        favTab { Icons::Type::Heart, "Favourites", "Favorites" };
    std::unique_ptr<CategoryList> categories;
    juce::Viewport categoryView;
    // Handle only navigation delivered to the focused list, never search refreshes.
    class ResultsList final : public juce::ListBox
    {
    public:
        explicit ResultsList (BrowserPanel& p) : juce::ListBox ("Results", &p), owner (p) {}
        bool keyPressed (const juce::KeyPress& key) override
        {
            const int before = getSelectedRow();
            const bool handled = juce::ListBox::keyPressed (key);
            const bool nav = key == juce::KeyPress::upKey || key == juce::KeyPress::downKey || key == juce::KeyPress::pageUpKey
                             || key == juce::KeyPress::pageDownKey || key == juce::KeyPress::homeKey || key == juce::KeyPress::endKey;
            if (nav && getSelectedRow() >= 0 && getSelectedRow() != before)
                owner.returnKeyPressed (getSelectedRow());
            return handled;
        }
    private:
        BrowserPanel& owner;
    };
    ResultsList results { *this };
public:
    /** Moves the result selection by delta rows (clamped) and loads that preset immediately. Used by the list,
        the search field and tests. Returns false when there are no results. */
    bool stepSelection (int delta);
    /** Key handling of the search field: up / down / page keys browse and load, Enter moves focus to the list. */
    bool handleSearchKey (const juce::KeyPress& key);
private:
    std::unique_ptr<juce::FileChooser> chooser;
};

/** LIBRARY page: preset cards for the current browser selection (keyboard navigable). */
class LibraryView : public juce::Component
{
public:
    LibraryView (ViceSynthProcessor& p, BrowserModel& m);
    ~LibraryView() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    bool keyPressed (const juce::KeyPress&) override;

private:
    class Grid;
    ViceSynthProcessor& proc;
    BrowserModel& model;
    juce::Viewport viewport;
    std::unique_ptr<Grid> grid;
};

} // namespace vsx
