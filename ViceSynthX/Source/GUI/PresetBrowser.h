#pragma once

#include "Widgets.h"

namespace vsx
{
class PresetBrowser : public Section, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (ViceSynthProcessor& p);
    void resized() override;
    void refresh();
    void saveAs();

private:
    int getNumRows() override { return (int) filtered.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void setFilter (PresetManager::SourceFilter f);

    ViceSynthProcessor& proc;
    juce::TextEditor search;
    juce::ComboBox category;
    juce::TextButton allB { "ALL" }, factB { "FACTORY" }, userB { "USER" }, aiB { "AI" }, favB { "FAV" };
    juce::TextButton saveB { "SAVE AS" }, importB { "IMPORT" }, exportB { "EXPORT" }, rescanB { "RESCAN" };
    juce::ListBox list { "Presets", this };
    std::vector<int> filtered;
    PresetManager::SourceFilter filter = PresetManager::SourceFilter::All;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace vsx
