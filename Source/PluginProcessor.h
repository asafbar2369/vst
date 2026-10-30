#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

class RMX16AudioProcessor final : public juce::AudioProcessor
{
public:
    RMX16AudioProcessor();
    ~RMX16AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RMX16 Research Build"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 120.0; }
    int getNumPrograms() override { return 9; }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    void storeUserPreset();
    void recallUserPreset();
    bool hasStoredUserPreset() const noexcept;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static const juce::StringArray programNames;

private:
    class Engine;
    std::unique_ptr<Engine> engine;
    juce::ValueTree storedUserPreset;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainSmooth, outputGainSmooth, mixSmooth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RMX16AudioProcessor)
};
