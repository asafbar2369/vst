#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "D50Engine.h"
#include "JD800Engine.h"
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

    // ---- D-50 SysEx ----
    struct SysExStats
    {
        std::atomic<int> accepted { 0 }, rejected { 0 }, ignored { 0 }, replies { 0 };
        std::atomic<int> lastCommand { -1 };
        std::atomic<uint32_t> lastAddress { 0 };
    };
    SysExStats sysexStats;

    // Handles one exclusive message. Replies (DT1 / RJC) are appended to `replies` if non-null.
    // Safe on the audio thread (no allocation unless the MidiBuffer must grow).
    void handleSysEx (const uint8_t* data, int size, juce::MidiBuffer* replies, int samplePos) noexcept;

    // File helpers (message thread)
    int importSysExData (const void* data, size_t size); // returns number of accepted messages
    juce::MemoryBlock exportD50TempAsSysEx() const;

    // Copies pending SysEx-driven changes into the host-visible parameters. Called by a timer;
    // tests call it directly.
    void syncParametersFromD50Temp();

    uint8_t getD50Byte (int offset) const noexcept { return d50Temp[(size_t) offset].load (std::memory_order_relaxed); }
    juce::String getD50PatchName() const;
    bool isUserBankSlotValid (int slot) const noexcept { return userBankValid[(size_t) slot].load(); }
    uint8_t getUserBankByte (int slot, int offset) const noexcept { return userBank[(size_t) (slot * d50::kTempSize + offset)].load(); }

    int getD50ActiveVoices() const noexcept { return d50Voices.load(); }
    int getD50ActiveTones (bool upper) const noexcept { return upper ? d50UpperTones.load() : d50LowerTones.load(); }

    // Direct editing of the temporary area from the UI (message thread). Values are clamped to the
    // documented range; bound host parameters follow via syncParametersFromD50Temp().
    void setD50ByteFromUi (int offset, int value);
    void setD50Name (int offset, int length, const juce::String& text);
    juce::String getD50Name (int offset, int length) const;
    int getJdActiveVoices() const noexcept { return jdVoices.load(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    void timerCallback() override { syncParametersFromD50Temp(); }
    void writeD50Byte (uint32_t linear, uint8_t value) noexcept;
    void applyParamsToD50Temp() noexcept;
    void routeNoteOn (int note, float velocity, int mode) noexcept;
    void renderSegment (juce::AudioBuffer<float>& buffer, int start, int end, int mode) noexcept;
    void handleMidi (const juce::MidiMessage& m, juce::MidiBuffer& out, int samplePos, int mode) noexcept;
    dc::JdParams readJdParams() const noexcept;
    void decodeD50() noexcept;
    void resetD50TempToInit() noexcept;

    dc::D50Engine d50;
    dc::JD800Engine jd;

    std::array<std::atomic<uint8_t>, d50::kTempSize> d50Temp;
    std::array<std::atomic<uint64_t>, 7> dirtyBits;   // one bit per temp-area byte changed by SysEx
    std::unique_ptr<std::atomic<uint8_t>[]> userBank; // 64 x 448 bytes received into Patch Memory
    std::array<std::atomic<bool>, d50::kNumPatches> userBankValid;

    struct Bound { juce::RangedAudioParameter* param; std::atomic<float>* value; int offset; int lastSeen; };
    std::vector<Bound> bound;
    std::atomic<bool> forceParamsToTemp { true }, resyncLastSeen { false };

    std::atomic<float>* pMode = nullptr; std::atomic<float>* pSplit = nullptr; std::atomic<float>* pSwap = nullptr;
    std::atomic<float>* pBalance = nullptr; std::atomic<float>* pVolume = nullptr;
    std::atomic<float>* pSysexRx = nullptr; std::atomic<float>* pSysexCh = nullptr;
    std::vector<std::atomic<float>*> jdTone[4];
    std::atomic<float>* pJd[11] {};

    juce::AudioBuffer<float> d50Buf, jdBuf;
    juce::MidiBuffer replyBuffer;
    int lastMode = -1;
    int currentProgram = 0;
    bool d50Dirty = true;
    std::atomic<int> d50Voices { 0 }, jdVoices { 0 }, d50UpperTones { 0 }, d50LowerTones { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DigitalComboProcessor)
};
