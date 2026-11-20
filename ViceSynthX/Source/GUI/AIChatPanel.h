#pragma once

#include "Widgets.h"

namespace vsx
{
class AIChatPanel : public Section, private juce::Timer
{
public:
    explicit AIChatPanel (ViceSynthProcessor& p);
    ~AIChatPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void rebuild();
    void submitText (const juce::String& text); // also used by tests

private:
    class Bubble;
    class ChatList : public juce::Component
    {
    public:
        void layout (int width);
    };
    void timerCallback() override;
    void send();

    ViceSynthProcessor& proc;
    AIController& ai;
    juce::Viewport viewport;
    ChatList list;
    juce::OwnedArray<Bubble> bubbles;
    juce::TextEditor input;
    juce::TextButton sendButton { "SEND" }, stopButton { "STOP" }, undoButton { "UNDO" }, redoButton { "REDO" },
        previewButton { "PREVIEW" }, saveButton { "SAVE" }, refButton { "LOAD REF" }, matchButton { "MATCH" }, clearButton { "CLEAR" };
    juce::Label status;
    double progressValue = 0.0;
    juce::ProgressBar progress { progressValue };
    std::unique_ptr<juce::FileChooser> chooser;
    size_t lastCount = 0;
};

} // namespace vsx
