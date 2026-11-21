#pragma once

#include "GUI/AIChatPanel.h"
#include "GUI/AIDesignerPanel.h"
#include "GUI/BrowserLibrary.h"
#include "GUI/DesignStage.h"
#include "GUI/HeaderHero.h"
#include "GUI/MacroPerformance.h"
#include "GUI/StudioView.h"
#include "GUI/WavetableEditor.h"
#include "PluginProcessor.h"
#include <juce_opengl/juce_opengl.h>
#include <map>

namespace vsx
{
/**
    Main editor (cinematic layout). Everything is laid out once on a 1648 x 928 artboard
    inside a root component that is scaled with an affine transform, so the interface stays
    sharp and proportional at any size (fixed aspect ratio, 50 % - 200 %, HiDPI aware).

    Views: LIBRARY (preset cards), STUDIO (main controls), SOUND DESIGN (every section in
    depth) and STAGE (performance). The browser, AI designer, macro strip and performance
    keyboard stay visible on every view.
*/
class ViceSynthEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    static constexpr int kDesignW = 1648, kDesignH = 928;
    enum View { Library = 0, Studio = 1, SoundDesign = 2, Stage = 3 };

    explicit ViceSynthEditor (ViceSynthProcessor&);
    ~ViceSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void showView (int view);
    int getView() const { return currentView; }
    void setScale (float s);
    void setOpenGL (bool enabled);
    juce::Component& getRoot() { return root; }
    void openWavetableEditor (int osc);
    void showSettings();
    void showHelp();
    void showHistory();
    AIDesignerPanel& getAIPanel() { return *aiPanel; }
    void closeOverlay() { overlay.hide(); }
    /** Runs one refresh pass now (modulation rings, AI change markers, header); normally done by the 30 Hz timer. */
    void refreshNow() { refresh (false); }

private:
    class Root : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    void timerCallback() override { refresh (true); }
    void refresh (bool requireOnScreen);
    bool visibleInEditor (const juce::Component* c) const;
    void layoutRoot();
    juce::DynamicObject& ui();

    ViceSynthProcessor& proc;
    VsxLookAndFeel lnf;
    juce::TooltipWindow tooltips { nullptr, 600 };
    Root root;
    BrowserModel browserModel;

    std::unique_ptr<MainHeader> header;
    std::unique_ptr<BrowserPanel> browser;
    std::unique_ptr<PresetHero> hero;
    std::unique_ptr<LibraryView> library;
    std::unique_ptr<StudioView> studio;
    std::unique_ptr<SoundDesignView> design;
    std::unique_ptr<StageView> stage;
    std::unique_ptr<AIDesignerPanel> aiPanel;
    std::unique_ptr<MacroStrip> macros;
    std::unique_ptr<PerformancePanel> performance;

    Overlay overlay;
    std::unique_ptr<SettingsPage> settingsPage;
    std::unique_ptr<AIChatPanel> history;
    std::unique_ptr<HelpView> help;
    std::unique_ptr<WavetableEditor> wavetableEditor;

    juce::OpenGLContext openGL;
    int currentView = Studio;
    int frame = 0;
    juce::String lastPresetName;
    std::map<int, float> aiChanged;
    int aiSerialSeen = -1;
};

} // namespace vsx
