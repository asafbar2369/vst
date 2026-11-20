#pragma once

#include "GUI/AIChatPanel.h"
#include "GUI/MiamiArtwork.h"
#include "GUI/PresetBrowser.h"
#include "GUI/SynthPanels.h"
#include "GUI/TabPages.h"
#include "GUI/WavetableEditor.h"
#include "PluginProcessor.h"
#include <juce_opengl/juce_opengl.h>

namespace vsx
{
/**
    Main editor. The whole interface is laid out at a fixed design size (1680 x 1050) inside
    a root component that is scaled with an affine transform, so every control stays sharp
    and proportional at any window size (fixed aspect ratio, resizable from 50 % to 200 %).
*/
class ViceSynthEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    static constexpr int kDesignW = 1680, kDesignH = 1050;

    explicit ViceSynthEditor (ViceSynthProcessor&);
    ~ViceSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void showTab (int index);
    int getCurrentTab() const { return currentTab; }
    void setScale (float s);
    void setOpenGL (bool enabled);
    juce::Component& getRoot() { return root; }
    AIChatPanel& getChat() { return *chat; }
    void openWavetableEditor (int osc);

private:
    class Header;
    class Root : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    void timerCallback() override;
    void layoutRoot();

    ViceSynthProcessor& proc;
    VsxLookAndFeel lnf;
    juce::TooltipWindow tooltips { nullptr, 600 };
    Root root;
    std::unique_ptr<Header> header;
    juce::OwnedArray<juce::TextButton> tabButtons;
    std::unique_ptr<PresetBrowser> browser;
    std::unique_ptr<AIChatPanel> chat;
    std::unique_ptr<BottomBar> bottom;

    juce::Component synthPage;
    std::array<std::unique_ptr<OscPanel>, kNumOscs> oscPanels;
    std::unique_ptr<SubNoisePanel> subNoise;
    std::array<std::unique_ptr<FilterPanel>, kNumFilters> filterPanels;
    std::unique_ptr<ModulationSection> modSection;

    std::unique_ptr<FxPage> fxPage;
    std::unique_ptr<MatrixPage> matrixPage;
    std::unique_ptr<LfoPage> lfoPage;
    std::unique_ptr<EnvPage> envPage;
    std::unique_ptr<ArpPage> arpPage;
    std::unique_ptr<SeqPage> seqPage;
    std::unique_ptr<MixerPage> mixerPage;
    std::unique_ptr<SettingsPage> settingsPage;
    std::vector<juce::Component*> pages;

    std::unique_ptr<WavetableEditor> wavetableEditor;
    juce::OpenGLContext openGL;
    int currentTab = 0;
    int frame = 0;
};

} // namespace vsx
