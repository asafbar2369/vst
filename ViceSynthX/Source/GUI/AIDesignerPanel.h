#pragma once

#include "Icons.h"
#include "Widgets.h"

namespace vsx
{
/**
    AI SOUND DESIGNER panel: provider / mode selector (OFFLINE / LOCAL / CLOUD), prompt with
    character count (English or Hebrew, right-to-left aware), GENERATE SOUND, live status and
    progress, the latest explanation with measured results, suggested prompts, the variation
    navigator, and tools (stop, preview, A/B compare, save & rename, reference audio, match,
    history). Reference audio can be dropped onto the panel. Nothing is simulated: the active
    engine is always named.
*/
class AIDesignerPanel : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit AIDesignerPanel (ViceSynthProcessor& p);
    ~AIDesignerPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void generate();
    std::function<void()> onShowHistory; // AI provider settings live in the single header SETTINGS page

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void conversationChanged();
    void showSuggestions();
    void stepVariation (int delta);
    void updatePromptDirection();

    ViceSynthProcessor& proc;
    AIController& ai;
    juce::ComboBox providerBox;
    juce::TextEditor prompt, response;
    juce::TextButton generateB { "GENERATE SOUND" }, compareB { "A/B" }, loadVarB { "LOAD" };
    IconButton refreshB { Icons::Type::Refresh, "More suggestions" }, prevVar { Icons::Type::ChevronLeft, "Previous variation" },
        nextVar { Icons::Type::ChevronRight, "Next variation" }, stopB { Icons::Type::Panic, "Stop the running request" },
        refB { Icons::Type::Audio, "Load reference audio (or drop a WAV / AIFF / FLAC on this panel)" },
        matchB { Icons::Type::Link, "Match the loaded reference: render candidates and refine towards it" },
        historyB { Icons::Type::History, "Conversation history and generated preset cards" };
    juce::OwnedArray<juce::TextButton> suggestionButtons;
    juce::Label status, varLabel, counter;
    juce::StringArray suggestions;
    int suggestionOffset = 0;
    std::vector<PatchSpec> variations;
    int variationIndex = 0;
    size_t lastConversationSize = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    double progressValue = 0.0;
    juce::ProgressBar progress { progressValue };
    bool dragOver = false;
    juce::Rectangle<int> badgeArea;
    static constexpr int kVisibleSuggestions = 3;
};

} // namespace vsx
