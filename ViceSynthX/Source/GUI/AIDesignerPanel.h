#pragma once

#include "Icons.h"
#include "Widgets.h"

namespace vsx
{
/**
    AI SOUND DESIGNER panel (concept layout): prompt with character count, GENERATE SOUND,
    suggested prompts, AI variations navigator, last response / status and tools.
    The active engine is always shown: either the local rule-based designer (offline) or the
    configured cloud / local LLM provider. Nothing is simulated.
*/
class AIDesignerPanel : public juce::Component, private juce::Timer
{
public:
    explicit AIDesignerPanel (ViceSynthProcessor& p);
    ~AIDesignerPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void generate();
    std::function<void()> onShowHistory;

private:
    void timerCallback() override;
    void conversationChanged();
    void showSuggestions();
    void stepVariation (int delta);

    ViceSynthProcessor& proc;
    AIController& ai;
    juce::TextEditor prompt;
    juce::TextButton generateB { "GENERATE SOUND" };
    IconButton refreshB { Icons::Type::Refresh, "More suggestions" }, prevVar { Icons::Type::ChevronLeft, "Previous variation" },
        nextVar { Icons::Type::ChevronRight, "Next variation" }, stopB { Icons::Type::Panic, "Stop the running request" },
        previewB { Icons::Type::Play, "Preview (play C4)" }, saveB { Icons::Type::Save, "Save the current sound to AI Generated presets" },
        refB { Icons::Type::Audio, "Load reference audio to analyse and match" }, matchB { Icons::Type::Link, "Match the loaded reference (render + optimise)" },
        historyB { Icons::Type::History, "Conversation history" };
    juce::OwnedArray<juce::TextButton> suggestionButtons;
    juce::Label response, status, varLabel, counter;
    juce::Viewport responseView;
    juce::StringArray suggestions;
    int suggestionOffset = 0;
    std::vector<PatchSpec> variations;
    int variationIndex = 0;
    size_t lastConversationSize = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    double progressValue = 0.0;
    juce::ProgressBar progress { progressValue };
};

} // namespace vsx
