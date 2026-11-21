#include "../Source/PluginEditor.h"
#include "../Source/GUI/Theme.h"
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
        // never touch the real user's AI settings
        const auto tempSettings = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vsx-gui-test-settings");
        tempSettings.createDirectory();
        AISettings::setSettingsFolderOverride (tempSettings);

        beginTest ("Vintage rotary drawing is visible and follows its value");
        {
            VsxLookAndFeel look;
            juce::Slider slider;
            slider.setColour (juce::Slider::rotarySliderFillColourId, Theme::cyberCyan);
            auto draw = [&] (int size, float position)
            {
                juce::Image image (juce::Image::ARGB, size, size, true);
                juce::Graphics g (image);
                look.drawRotarySlider (g, 0, 0, size, size, position, -2.35619449f, 2.35619449f, slider);
                return image;
            };
            for (int size : { 16, 24, 48, 96, 160 })
            {
                const auto a = draw (size, 0.15f);
                const auto b = draw (size, 0.85f);
                int visible = 0, changed = 0;
                for (int y = 0; y < size; ++y)
                    for (int x = 0; x < size; ++x)
                    {
                        const auto pa = a.getPixelAt (x, y), pb = b.getPixelAt (x, y);
                        if (pa.getAlpha() > 0) ++visible;
                        if (pa != pb) ++changed;
                    }
                expect (visible > size * size / 8, "Knob must be visible at " + juce::String (size));
                expect (changed > 0, "Value indicator must move at " + juce::String (size));
            }
            if (shotDir.isNotEmpty())
            {
                juce::File dir (shotDir);
                dir.createDirectory();
                auto file = dir.getChildFile ("vsx_silver_knob.png");
                file.deleteFile();
                juce::FileOutputStream os (file);
                expect (os.openedOk() && juce::PNGImageFormat().writeImageToStream (draw (160, 0.65f), os),
                        "Silver knob screenshot");
            }
        }

        beginTest ("Editor builds, shows every view and scales with a fixed aspect ratio");
        auto processor = std::make_unique<ViceSynthProcessor>();
        auto& proc = *processor;
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

        beginTest ("AI workspace: a real offline request is applied, explained (English) and highlighted");
        {
            editor->closeOverlay();
            editor->showView (ViceSynthEditor::Studio);
            auto& ai = proc.getAI();
            auto settings = ai.getSettings();
            settings.provider = AISettings::Offline;
            settings.autoPreview = false;
            ai.setSettings (settings);
            ai.submit ("Make it darker and wider");
            const auto end = juce::Time::getMillisecondCounter() + 60000;
            while (ai.isBusy() && juce::Time::getMillisecondCounter() < end) { ai.deliverPendingResults(); juce::Thread::sleep (10); }
            expect (! ai.isBusy(), "offline AI request did not finish");
            expect (! ai.getAIChanges().empty(), "the AI result must change real parameters");
            editor->refreshNow();
            int highlighted = 0;
            for (auto* k : ParamKnob::registry())
            {
                bool visible = true;
                for (const juce::Component* c = k; c != nullptr && c != editor; c = c->getParentComponent()) visible = visible && c->isVisible();
                if (visible && k->isAIHighlighted()) ++highlighted;
            }
            expect (highlighted > 0, "changed parameters must be highlighted on visible knobs");
            if (shotDir.isNotEmpty())
            {
                auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                juce::FileOutputStream os (juce::File (shotDir).getChildFile ("vsx_ai.png"));
                juce::PNGImageFormat().writeImageToStream (img, os);
                editor->showHistory();
                auto img2 = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                juce::FileOutputStream os2 (juce::File (shotDir).getChildFile ("vsx_ai_history.png"));
                juce::PNGImageFormat().writeImageToStream (img2, os2);
                editor->closeOverlay();
            }
        }

        for (const float s : { 0.5f, 0.85f, 2.0f })
        {
            editor->setScale (s);
            expectWithinAbsoluteError ((double) editor->getWidth() / editor->getHeight(),
                                       (double) ViceSynthEditor::kDesignW / ViceSynthEditor::kDesignH, 0.01);
            expectWithinAbsoluteError (editor->getRoot().getTransform().mat00, (float) editor->getWidth() / ViceSynthEditor::kDesignW, 0.001f);
        }

        beginTest ("Header size control reflects the actual editor scale");
        auto* h = editor->getRoot().findChildWithID ("vsx_main_header");
        auto* sizeButton = h != nullptr ? dynamic_cast<juce::TextButton*> (h->findChildWithID ("vsx_ui_size")) : nullptr;
        expect (sizeButton != nullptr);
        if (sizeButton != nullptr)
        {
            for (const int percent : { 50, 60, 75, 85, 100, 125, 150, 175, 200 })
            {
                editor->setScale ((float) percent / 100.0f);
                expectEquals (sizeButton->getButtonText(), "SIZE " + juce::String (percent) + "%");
                expectWithinAbsoluteError ((float) proc.uiState.getProperty ("scale", 0.0),
                                           (float) percent / 100.0f, 0.001f);
            }
            editor->setScale (0.1f);
            expectEquals (sizeButton->getButtonText(), juce::String ("SIZE 50%"));
            editor->setScale (3.0f);
            expectEquals (sizeButton->getButtonText(), juce::String ("SIZE 200%"));
            editor->setScale (0.85f);
        }

        beginTest ("File import and waveform drop share chromatic sample mode");
        {
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                .getNonexistentChildFile ("vsx-sample-fixture", ".wav");
            {
                juce::FileOutputStream out (file);
                constexpr int frames = 4096, bytes = frames * 4;
                out.write ("RIFF", 4); out.writeInt (36 + bytes); out.write ("WAVEfmt ", 8);
                out.writeInt (16); out.writeShort (1); out.writeShort (2);
                out.writeInt (48000); out.writeInt (48000 * 4); out.writeShort (4); out.writeShort (16);
                out.write ("data", 4); out.writeInt (bytes);
                for (int i = 0; i < frames; ++i)
                {
                    const auto value = (short) (12000.0 * std::sin (juce::MathConstants<double>::twoPi * 261.625565 * i / 48000.0));
                    out.writeShort (value); out.writeShort ((short) (value / 2));
                }
            }
            proc.setParam (pid::O (0, pid::osc::Mode), 3.0f);
            expect (proc.importSampleFile (0, file));
            proc.getPatch().waitForLoads();
            expectEquals ((int) proc.getParam (pid::O (0, pid::osc::Mode)), 2);
            expectEquals ((int) proc.getParam (pid::O (0, pid::osc::RootNote)), 60);
            auto loaded = proc.getPatch().osc[0].sample.getShared();
            expect (loaded != nullptr);
            if (loaded != nullptr)
            {
                expectEquals (loaded->numChannels, 2);
                expectEquals (loaded->length, 4096);
                expectWithinAbsoluteError (loaded->channel (0, 0)[100] * 0.5f,
                                           loaded->channel (0, 1)[100], 0.0001f);
            }
            OscDisplay drop (proc, 0);
            proc.setParam (pid::O (0, pid::osc::Mode), 0.0f);
            drop.filesDropped ({ file.getFullPathName() }, 0, 0);
            proc.getPatch().waitForLoads();
            expectEquals ((int) proc.getParam (pid::O (0, pid::osc::Mode)), 2);
            expectEquals (proc.getPatch().osc[0].sampleId, "file:" + file.getFullPathName());
            const auto low = file.getSiblingFile (file.getFileNameWithoutExtension() + "_C3.wav");
            const auto high = file.getSiblingFile (file.getFileNameWithoutExtension() + "_C5.wav");
            expect (file.copyFileTo (low) && file.copyFileTo (high));
            drop.filesDropped ({ low.getFullPathName(), high.getFullPathName() }, 0, 0);
            proc.getPatch().waitForLoads();
            expectEquals ((int) proc.getParam (pid::O (0, pid::osc::Mode)), 3);
            auto multi = proc.getPatch().osc[0].multi.getShared();
            expect (multi != nullptr && multi->zones.size() == 2);
            if (multi != nullptr && multi->zones.size() == 2)
            {
                expectEquals (multi->zones.front().loKey, 0);
                expectEquals (multi->zones.back().hiKey, 127);
                expectEquals (multi->zones.front().hiKey + 1, multi->zones.back().loKey);
                expectEquals (multi->find (0, 1)->rootNote, 48);
                expectEquals (multi->find (127, 127)->rootNote, 72);
            }
            PatchState recalled;
            recalled.fromVar (proc.getPatch().toVar(), false);
            expect (recalled.osc[0].multi.getShared() != nullptr);
            low.deleteFile(); high.deleteFile();
            expectEquals (recalled.missingResources().size(), 2);
            proc.importSampleFile (0, file);
            proc.getPatch().waitForLoads();
            const auto before = proc.getPatch().osc[0].sample.getShared();
            const auto missing = file.getSiblingFile ("missing-" + file.getFileName());
            proc.importSampleFile (0, missing);
            proc.getPatch().waitForLoads();
            expect (proc.getPatch().osc[0].sample.getShared() == before);
            expectEquals ((int) proc.getParam (pid::O (0, pid::osc::Mode)), 2);
            expect (proc.getPatch().osc[0].lastError.isNotEmpty());
            file.deleteFile();
        }

        beginTest ("Arrow navigation loads real processor patches without Enter");
        {
            BrowserModel model;
            model.update (proc.getPresetManager());
            BrowserPanel panel (proc, model);
            panel.setBounds (0, 0, 290, 600);
            auto* list = dynamic_cast<juce::ListBox*> (panel.findChildWithID ("vsx_preset_results"));
            expect (list != nullptr && model.results.size() > 5);
            auto checkLoaded = [&] (int row)
            {
                const auto& entry = proc.getPresetManager().getEntries()[(size_t) model.results[(size_t) row]];
                expectEquals (proc.getCurrentMeta().name, entry.name);
                for (const auto& spec : FactoryPresets::all())
                    if (spec.meta.name == entry.name)
                        for (size_t i = 0; i < spec.values.size(); ++i)
                            expectWithinAbsoluteError (proc.getParam ((int) i), spec.values[i], 0.0001f,
                                                       "Loaded parameter " + juce::String ((int) i));
            };
            if (list != nullptr && model.results.size() > 5)
            {
                list->selectRow (0);
                expect (list->keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
                checkLoaded (1);
                expect (list->keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
                checkLoaded (0);
                // No implicit load from refresh, resize, or programmatic selection.
                proc.setParam (pid::O (0, pid::osc::Level), 0.123f);
                panel.refresh();
                list->selectRow (2);
                expectWithinAbsoluteError (proc.getParam (pid::O (0, pid::osc::Level)), 0.123f, 0.0001f);
                LibraryView view (proc, model);
                view.setBounds (0, 0, 800, 500);
                expect (view.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
                checkLoaded (1);
                expect (view.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
                checkLoaded (5);
                model.search = proc.getCurrentMeta().name;
                model.update (proc.getPresetManager());
                panel.refresh(); view.refresh();
                const auto retained = proc.getCurrentMeta().name;
                expect (view.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
                expectEquals (proc.getCurrentMeta().name, retained);
                model.search = "no_such_original_preset_981723";
                model.update (proc.getPresetManager());
                panel.refresh(); view.refresh();
                expect (! view.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
                expectEquals (proc.getCurrentMeta().name, retained);
            }
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

        beginTest ("All 16 panel layouts persist independently across all views and sizes");
        for (int mask = 0; mask < 16; ++mask)
        {
            for (int panel = 0; panel < 4; ++panel)
                editor->setPanelVisible (panel, (mask & (1 << panel)) != 0);
            for (int view = 0; view < 4; ++view)
            {
                editor->showView (view);
                for (const float scale : { 0.5f, 0.85f, 2.0f })
                {
                    editor->setScale (scale);
                    for (int panel = 0; panel < 4; ++panel)
                        expectEquals (editor->isPanelVisible (panel), (mask & (1 << panel)) != 0);
                    for (auto* child : editor->getRoot().getChildren())
                    {
                        int panel = -1;
                        if (dynamic_cast<AIDesignerPanel*> (child)) panel = 0;
                        if (dynamic_cast<BrowserPanel*> (child)) panel = 1;
                        if (dynamic_cast<MacroStrip*> (child)) panel = 2;
                        if (dynamic_cast<PerformancePanel*> (child)) panel = 3;
                        if (panel >= 0)
                        {
                            const bool shown = (mask & (1 << panel)) != 0;
                            expectEquals (child->isVisible(), shown);
                            expectEquals (child->getBounds().isEmpty(), ! shown);
                            if (shown) expect (editor->getRoot().getLocalBounds().contains (child->getBounds()));
                        }
                    }
                }
            }
            if (shotDir.isNotEmpty())
            {
                editor->showView (ViceSynthEditor::Studio);
                editor->setScale (0.85f);
                auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                juce::FileOutputStream os (juce::File (shotDir).getChildFile ("vsx_panels_" + juce::String (mask) + ".png"));
                expect (os.openedOk() && juce::PNGImageFormat().writeImageToStream (img, os));
            }
        }
        editor->setPanelVisible (0, false);
        editor->setPanelVisible (2, false);

        beginTest ("Editor survives being destroyed and recreated");
        base.reset();
        expect (ParamKnob::registry().isEmpty(), "knob registry must be empty after the editor is destroyed");
        base.reset (proc.createEditor());
        expect (base != nullptr);
        if (auto* restored = dynamic_cast<ViceSynthEditor*> (base.get()))
        {
            expect (! restored->isPanelVisible (0));
            expect (restored->isPanelVisible (1));
            expect (! restored->isPanelVisible (2));
            expect (restored->isPanelVisible (3));
        }
        base.reset();
        proc.releaseResources();
    }
};

static GuiTests guiTests;
} // namespace vsx
