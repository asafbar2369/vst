#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "D50Engine.h"
#include "JD800Engine.h"
#include "ParamArea.h"
#include "Parameters.h"
#include "UserWaves.h"

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

    // Which engines a key reaches in the given play mode (split point, swap and zones applied).
    void getRouting (int note, int mode, bool& toD50, bool& toJd) const noexcept;
    int getCurrentMode() const noexcept { return juce::jlimit (0, 3, (int) pMode->load()); }
    int getTranspose (bool forJd) const noexcept { return (int) std::lround ((forJd ? mixJd : mixD50).transpose->load()); }

    int getD50ActiveVoices() const noexcept { return d50Voices.load(); }
    int getD50ActiveTones (bool upper) const noexcept { return upper ? d50UpperTones.load() : d50LowerTones.load(); }
    int getJdActiveVoices() const noexcept { return jdVoices.load(); }
    int getJdActiveTones (int tone) const noexcept { return jdTones[(size_t) (tone & 3)].load(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // ---- Patch files (.dcpatch): the whole combo (both synths' areas, mode, mixer) ----
    static juce::File defaultPatchFolder();             // Documents/Digital Combo/Patches
    juce::File getPatchFolder() const { return patchFolder; }
    void setPatchFolder (const juce::File& f) { patchFolder = f; }
    juce::Array<juce::File> findUserPatches() const;    // *.dcpatch in the patch folder, sorted
    bool savePatchFile (const juce::File& file, const juce::String& name);
    bool loadPatchFile (const juce::File& file);        // false if the file is not a valid patch
    juce::String getCurrentPatchName() const { return currentPatchName; }

    // ---- The user's own wave files (replace substitute waves; see UserWaveLoader.h) ----
    // jd == false: D-50 PCM folder (files 1-100), true: JD-800 waveform folder (files 1-108).
    // Returns a short report. An empty File clears that folder.
    juce::String setUserWaveFolder (bool forJd, const juce::File& folder);
    juce::File getUserWaveFolder (bool forJd) const { const juce::ScopedLock sl (wavesLock); return forJd ? userWaveFolderJd : userWaveFolderD50; }
    int getUserWaveCount (bool forJd) const;
    juce::StringArray getUserWaveMessages() const { const juce::ScopedLock sl (wavesLock); return userWaveMessages; }

    // Editor-only preference, saved with the state: 0 = software renderer (default), 1 = Direct2D.
    // JUCE's Direct2D path can show a blank/black editor with some hosts or graphics drivers.
    std::atomic<int> guiRenderer { 0 };

private:
    void timerCallback() override { syncParametersFromD50Temp(); freeRetiredWaveSets(); }
    void reloadUserWaves();
    void freeRetiredWaveSets();
    void routeNoteOn (int note, float velocity, int mode) noexcept;
    void routeNoteOff (int note) noexcept;
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
    struct EngineMix { std::atomic<float>* level = nullptr; std::atomic<float>* pan = nullptr; std::atomic<float>* transpose = nullptr;
                       std::atomic<float>* keyLow = nullptr; std::atomic<float>* keyHigh = nullptr; };
    EngineMix mixD50, mixJd;
    // Which (transposed) note each incoming key started on each engine, so note-offs match even if
    // the transpose changes while a key is held. -1 = not sounding on that engine.
    std::array<int, 128> sentD50 {}, sentJd {};
    void clearNoteMaps() noexcept { sentD50.fill (-1); sentJd.fill (-1); }

    juce::AudioBuffer<float> d50Buf, jdBuf;
    juce::MidiBuffer replyBuffer;
    int lastMode = -1;
    int currentProgram = 0;
    std::atomic<int> d50Voices { 0 }, jdVoices { 0 }, d50UpperTones { 0 }, d50LowerTones { 0 };
    std::array<std::atomic<int>, 4> jdTones {};

    juce::File patchFolder { defaultPatchFolder() };
    juce::String currentPatchName;

    // User wave sets: the message thread publishes `liveWaves`; the audio thread announces the set
    // it is using in `wavesInUse` (hazard pointer) so a replaced set is only freed once unused.
    juce::File userWaveFolderD50, userWaveFolderJd;
    juce::StringArray userWaveMessages;
    std::atomic<const dc::UserWaveSet*> liveWaves { nullptr }, wavesInUse { nullptr };
    std::unique_ptr<dc::UserWaveSet> ownedWaves;
    juce::CriticalSection wavesLock; // message/state threads only; the audio thread never takes it
    std::vector<std::unique_ptr<dc::UserWaveSet>> retiredWaves;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DigitalComboProcessor)
};
