#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "D50Engine.h"
#include "JD800Engine.h"
#include "ParamArea.h"
#include "Parameters.h"

class DigitalComboProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    DigitalComboProcessor();
    ~DigitalComboProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Digital Combo"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Roland SysEx (D-50 model 14H, JD-800 model 3DH) ----
    struct SysExStats
    {
        std::atomic<int> accepted { 0 }, rejected { 0 }, ignored { 0 }, replies { 0 };
        std::atomic<int> lastModel { -1 }, lastCommand { -1 };
        std::atomic<uint32_t> lastAddress { 0 };
    };
    SysExStats sysexStats;

    // Handles one exclusive message. Replies (DT1 / RJC) are appended to `replies` if non-null.
    // A null `replies` means "from a file": device ID and receive switch are not checked.
    void handleSysEx (const uint8_t* data, int size, juce::MidiBuffer* replies, int samplePos) noexcept;

    int importSysExData (const void* data, size_t size); // message thread; returns accepted messages
    juce::MemoryBlock exportD50TempAsSysEx() const;
    juce::MemoryBlock exportJdTempAsSysEx() const;

    // Copies SysEx/UI-driven byte changes into the host-visible parameters (timer; tests call it).
    void syncParametersFromD50Temp() { d50Area.syncParameters(); jdArea.syncParameters(); }

    ParamArea& getD50Area() noexcept { return d50Area; }
    ParamArea& getJdArea() noexcept { return jdArea; }
    uint8_t getD50Byte (int offset) const noexcept { return d50Area.get (offset); }
    uint8_t getJdByte (int offset) const noexcept { return jdArea.get (offset); }
    bool isUserBankSlotValid (int slot) const noexcept { return d50Area.isSlotValid (slot); }
    uint8_t getUserBankByte (int slot, int offset) const noexcept { return d50Area.getBank (slot, offset); }

    // UI editing of the areas (message thread)
    void setD50ByteFromUi (int offset, int value) { d50Area.write ((uint32_t) offset, value); d50Area.syncParameters(); }
    void setJdByteFromUi (int offset, int value)  { jdArea.write ((uint32_t) offset, value); jdArea.syncParameters(); }
    void setD50Name (int offset, int length, const juce::String& text);
    juce::String getD50Name (int offset, int length) const;
    void setJdName (const juce::String& text);
    juce::String getJdName() const;
    juce::String getD50PatchName() const { return getD50Name ((int) d50::kPatchFactors, 18); }

    int getD50ActiveVoices() const noexcept { return d50Voices.load(); }
    int getD50ActiveTones (bool upper) const noexcept { return upper ? d50UpperTones.load() : d50LowerTones.load(); }
    int getJdActiveVoices() const noexcept { return jdVoices.load(); }
    int getJdActiveTones (int tone) const noexcept { return jdTones[(size_t) (tone & 3)].load(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    void timerCallback() override { syncParametersFromD50Temp(); }
    void routeNoteOn (int note, float velocity, int mode) noexcept;
    void renderSegment (juce::AudioBuffer<float>& buffer, int start, int end, int mode) noexcept;
    void handleMidi (const juce::MidiMessage& m, int mode) noexcept;
    void decodeD50() noexcept;
    void decodeJd() noexcept;
    bool handleModelSysEx (bool isJd, const d50::Message& msg, juce::MidiBuffer* replies, int samplePos) noexcept;

    dc::D50Engine d50;
    dc::JD800Engine jd;
    ParamArea d50Area, jdArea;

    std::atomic<float>* pMode = nullptr; std::atomic<float>* pSplit = nullptr; std::atomic<float>* pSwap = nullptr;
    std::atomic<float>* pBalance = nullptr; std::atomic<float>* pVolume = nullptr;
    std::atomic<float>* pSysexRx = nullptr; std::atomic<float>* pSysexCh = nullptr; std::atomic<float>* pJdUnit = nullptr;

    juce::AudioBuffer<float> d50Buf, jdBuf;
    juce::MidiBuffer replyBuffer;
    int lastMode = -1;
    int currentProgram = 0;
    std::atomic<int> d50Voices { 0 }, jdVoices { 0 }, d50UpperTones { 0 }, d50LowerTones { 0 };
    std::array<std::atomic<int>, 4> jdTones {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DigitalComboProcessor)
};
