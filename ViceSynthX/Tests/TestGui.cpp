#include "../Source/PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace vsx
{
/**
    Editor tests: construction / destruction, every view, scaling limits, fixed aspect ratio,
    every knob bound to a real parameter, and optional PNG screenshots of the real editor
    (set VSX_SCREENSHOT_DIR) used for visual review.
*/
class GuiTests final : public juce::UnitTest
{
public:
    GuiTests() : juce::UnitTest ("Editor", "VICE SYNTH X") {}

    static void renderSomeAudio (ViceSynthProcessor& proc)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 60; ++b)
        {
            midi.clear();
            if (b == 0)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 100), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            }
            buffer.clear();
            proc.processBlock (buffer, midi);
        }
    }

    void runTest() override
    {
        const auto shotDir = juce::SystemStats::getEnvironmentVariable ("VSX_SCREENSHOT_DIR", {});

        beginTest ("Editor builds, shows every view and scales with a fixed aspect ratio");
        ViceSynthProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        const int idx = proc.getPresetManager().indexOfName ("Miami Drive");
        if (idx >= 0) proc.loadPresetEntry (proc.getPresetManager().getEntries()[(size_t) idx]);
        renderSomeAudio (proc);

        std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
        auto* editor = dynamic_cast<ViceSynthEditor*> (base.get());
        expect (editor != nullptr);
        if (editor == nullptr) return;

        editor->setScale (1.0f);
        expectEquals (editor->getWidth(), ViceSynthEditor::kDesignW);
        expectEquals (editor->getHeight(), ViceSynthEditor::kDesignH);

        const char* viewNames[] = { "library", "studio", "sound_design", "stage" };
        for (int v = 0; v < 4; ++v)
        {
            editor->showView (v);
            expectEquals (editor->getView(), v);
            if (shotDir.isNotEmpty())
            {
                auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                juce::File dir (shotDir);
                dir.createDirectory();
                auto file = dir.getChildFile (juce::String ("vsx_") + viewNames[v] + ".png");
                file.deleteFile();
                juce::FileOutputStream os (file);
                juce::PNGImageFormat png;
                expect (os.openedOk() && png.writeImageToStream (img, os), "screenshot " + file.getFullPathName());
            }
        }

        if (shotDir.isNotEmpty())
        {
            editor->showView (ViceSynthEditor::Studio);
            editor->showSettings();
            auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            juce::FileOutputStream os (juce::File (shotDir).getChildFile ("vsx_settings.png"));
            juce::PNGImageFormat().writeImageToStream (img, os);
        }

        for (const float s : { 0.5f, 0.85f, 2.0f })
        {
            editor->setScale (s);
            expectWithinAbsoluteError ((double) editor->getWidth() / editor->getHeight(),
                                       (double) ViceSynthEditor::kDesignW / ViceSynthEditor::kDesignH, 0.01);
            expectWithinAbsoluteError (editor->getRoot().getTransform().mat00, (float) editor->getWidth() / ViceSynthEditor::kDesignW, 0.001f);
        }

        beginTest ("Every visible knob controls a real registry parameter");
        const int numParams = ParamRegistry::get().size();
        int knobs = 0;
        for (auto* k : ParamKnob::registry())
        {
            ++knobs;
            expect (juce::isPositiveAndBelow (k->getParamIndex(), numParams), "knob bound to invalid parameter");
        }
        expect (knobs > 50, "expected the editor to create its knobs");

        beginTest ("Editor survives being destroyed and recreated");
        base.reset();
        expect (ParamKnob::registry().isEmpty(), "knob registry must be empty after the editor is destroyed");
        base.reset (proc.createEditor());
        expect (base != nullptr);
        base.reset();
        proc.releaseResources();
    }
};

static GuiTests guiTests;
} // namespace vsx
