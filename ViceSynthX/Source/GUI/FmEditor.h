#pragma once

#include "Widgets.h"
#include "../Synth/FmVoice.h"

namespace vsx
{
/** Draws one of the 32 algorithms: operator boxes, modulation paths, the feedback loop and the carrier bus.
    Carriers are highlighted; muted operators are dimmed. */
class FmAlgorithmDiagram : public juce::Component
{
public:
    void setAlgorithm (int a, uint8_t enabledMask) { if (a != algo || enabledMask != enabled) { algo = a; enabled = enabledMask; repaint(); } }
    void paint (juce::Graphics&) override;
    /** Operator positions in units (x column, y row from the bottom); used for drawing and tests. */
    static std::array<juce::Point<float>, fm::kOps> layout (int algorithm);
private:
    int algo = 0;
    uint8_t enabled = 0x3F;
};

/**
    FM page (SOUND DESIGN > FM): edits the six-operator voice of the selected oscillator.
      - LOAD CARTRIDGE (.syx 32-voice bank or single voice, checksum checked) and a voice list (arrow keys load at once)
      - factory voices, INIT VOICE, SAVE VOICE / SAVE BANK (.syx)
      - algorithm 1-32 with diagram, feedback, FM Amount (automatable), transpose, oscillator key sync
      - per operator: on (mute) and solo, ratio / fixed, coarse, fine, detune, output level, velocity, amp-mod sens.,
        rates 1-4, levels 1-4, keyboard break point / depths / curves, rate scaling
      - pitch envelope and LFO
    Edits are published to the audio thread immediately (lock-free); loading a voice is one undoable step.
*/
class FmEditor : public juce::Component, private juce::Timer, private juce::ListBoxModel
{
public:
    explicit FmEditor (ViceSynthProcessor& p);
    ~FmEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void selectOscillator (int o);
    int getOscillator() const { return osc; }
    /** Cartridge handling (also used by tests). Returns false and shows the error if the file is not usable. */
    bool loadCartridge (const juce::File& f);
    const std::vector<fm::Patch>& getCartridge() const { return cartridge; }
    void loadCartridgeVoice (int index);
    juce::String getStatus() const { return status; }
    void setSolo (int op);   // -1 = no solo
    void toggleMute (int op);

private:
    class OpRow;
    struct Field;
    void timerCallback() override;
    void pushEdit();               // local voice -> processor
    void pullVoice();              // processor -> local voice and controls
    void syncControls();
    juce::Slider& makeField (juce::Component& parent, int lo, int hi, const juce::String& tip, std::function<uint8_t& ()> ref);
    int getNumRows() override { return (int) cartridge.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int row) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    ViceSynthProcessor& proc;
    int osc = 0;
    fm::Patch voice;
    juce::String lastHex;
    int solo = -1;
    uint8_t muteMask = 0x3F;       // the user's mute state; solo overrides it while active
    bool syncing = false;

    juce::OwnedArray<juce::TextButton> oscButtons;
    juce::TextButton useFm { "USE FM ON OSC 1" }, loadCart { "LOAD CARTRIDGE" }, factoryB { "FACTORY VOICES" }, initB { "INIT VOICE" },
        saveVoiceB { "SAVE VOICE" }, saveBankB { "SAVE BANK" }, newPatchB { "AS NEW PATCH" }, algoPrev { "<" }, algoNext { ">" },
        oscSyncB { "KEY SYNC" }, lfoSyncB { "LFO SYNC" };
    juce::TextEditor nameEdit;
    juce::Label algoLabel, statusLabel, cartLabel;
    FmAlgorithmDiagram diagram;
    std::unique_ptr<ParamKnob> fmAmount;
    juce::ComboBox lfoWave;
    juce::OwnedArray<OpRow> rows;
    juce::OwnedArray<juce::Slider> ownedRowFields, globalFields;
    juce::StringArray globalNames;
    std::vector<std::function<uint8_t& ()>> fieldRefs; // parallel to all sliders (rows + globals)
    std::vector<juce::Slider*> allFields;
    juce::ListBox cartList { "Cartridge", this };
    std::vector<fm::Patch> cartridge;
    juce::String cartName;
    juce::String status;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> globalArea, opHeader;
};
} // namespace vsx
