#pragma once
#include "PluginProcessor.h"

class RMX16AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit RMX16AudioProcessorEditor (RMX16AudioProcessor&);
    ~RMX16AudioProcessorEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void selectPotTarget (const juce::String& id);
    void handleKeypadDigit (const juce::String& key);
    void commitKeypadEntry();
    void nudgeSelected (float direction);
    float selectedValue() const;
    void setSelectedValue (float value);
    float selectedMin() const;
    float selectedMax() const;
    float selectedStep() const;
    float maxPredelayForCurrentProgram() const;

    RMX16AudioProcessor& processor;
    juce::Image faceplate;
    juce::Slider input, output, pot;
    juce::Label display;
    juce::TextButton programButton { "" }, predelayButton { "" }, decayButton { "" };
    juce::TextButton lowButton { "" }, highButton { "" }, storeButton { "" }, recallButton { "" };
    juce::TextButton nudgeDown { "" }, nudgeUp { "" };
    std::array<std::unique_ptr<juce::TextButton>, 16> keypad;
    juce::String potTarget { "decay" };
    bool updatingPot = false;
    bool parameterEntryArmed = false;
    bool programEntryArmed = false;
    juce::String entryBuffer;
    int echoBank = 0; // 0 = A, 1 = B when Program 7 Echo is selected
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inputA, outputA;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RMX16AudioProcessorEditor)
};
