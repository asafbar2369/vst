#include "../Source/PluginEditor.h"
#include "../Source/GUI/Theme.h"
#include "../Source/GUI/OscSourceMenu.h"
#include "../Source/Presets/SoundRecipes.h"
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
            expect (proc.waitForImports());
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
            expect (proc.waitForImports());
            expectEquals ((int) proc.getParam (pid::O (0, pid::osc::Mode)), 2);
            expectEquals (proc.getPatch().osc[0].sampleId, "file:" + file.getFullPathName());
            const auto low = file.getSiblingFile (file.getFileNameWithoutExtension() + "_C3.wav");
            const auto high = file.getSiblingFile (file.getFileNameWithoutExtension() + "_C5.wav");
            expect (file.copyFileTo (low) && file.copyFileTo (high));
            drop.filesDropped ({ low.getFullPathName(), high.getFullPathName() }, 0, 0);
            expect (proc.waitForImports());
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
            expect (proc.waitForImports());
            const auto before = proc.getPatch().osc[0].sample.getShared();
            const auto missing = file.getSiblingFile ("missing-" + file.getFileName());
            proc.importSampleFile (0, missing);
            expect (proc.waitForImports());
            expect (proc.getPatch().osc[0].sample.getShared() == before);
            expectEquals ((int) proc.getParam (pid::O (0, pid::osc::Mode)), 2);
            expect (proc.getImportStatus (0).error, "missing file must be reported");
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
                        {
                            // parameters travel through the host's normalised float value: allow one part in 10^6 of the range
                            const auto& d = ParamRegistry::get()[i];
                            expectWithinAbsoluteError (proc.getParam ((int) i), spec.values[i], 0.0001f + 1.0e-6f * (d.maxValue - d.minValue),
                                                       "Loaded parameter " + juce::String ((int) i));
                        }
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

        beginTest ("Search field arrows load presets instantly; Page Down jumps a page");
        {
            BrowserModel model;
            model.update (proc.getPresetManager());
            BrowserPanel panel (proc, model);
            panel.setBounds (0, 0, 290, 900);
            auto* list = dynamic_cast<juce::ListBox*> (panel.findChildWithID ("vsx_preset_results"));
            expect (list != nullptr && model.results.size() > 20);
            if (list != nullptr && model.results.size() > 20)
            {
                list->selectRow (0);
                expect (panel.stepSelection (1));
                expectEquals (list->getSelectedRow(), 1);
                expectEquals (proc.getCurrentMeta().name, proc.getPresetManager().getEntries()[(size_t) model.results[1]].name);
                expect (list->keyPressed (juce::KeyPress (juce::KeyPress::pageDownKey)));
                const int row = list->getSelectedRow();
                expect (row > 2, "page down moves more than one row");
                expectEquals (proc.getCurrentMeta().name, proc.getPresetManager().getEntries()[(size_t) model.results[(size_t) row]].name);
                expect (list->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
                expectEquals (proc.getCurrentMeta().name, proc.getPresetManager().getEntries()[(size_t) model.results[0]].name);
                // the search editor forwards up / down to the result list
                expect (panel.handleSearchKey (juce::KeyPress (juce::KeyPress::downKey)));
                expectEquals (proc.getCurrentMeta().name, proc.getPresetManager().getEntries()[(size_t) model.results[1]].name);
                expect (! panel.handleSearchKey (juce::KeyPress ('a')), "typing still goes to the search text");
            }
        }

        beginTest ("INIT loads a clean patch and is undoable");
        {
            proc.loadPresetEntry (proc.getPresetManager().getEntries()[3]);
            const auto before = proc.getCurrentMeta().name;
            auto* initButton = dynamic_cast<juce::Button*> (editor->getRoot().findChildWithID ("vsx_main_header") != nullptr
                                                                 ? editor->getRoot().findChildWithID ("vsx_main_header")->findChildWithID ("vsx_init_patch") : nullptr);
            expect (initButton != nullptr, "header INIT button");
            if (initButton != nullptr && initButton->onClick) initButton->onClick(); // triggerClick() is asynchronous
            expectEquals (proc.getCurrentMeta().name, juce::String ("Init"));
            const auto init = Recipes::init ("Init", "Init");
            for (size_t i = 0; i < init.values.size(); ++i)
                expectWithinAbsoluteError (proc.getParam ((int) i), init.values[i],
                                           0.0001f + 1.0e-6f * (ParamRegistry::get()[i].maxValue - ParamRegistry::get()[i].minValue));
            expect (proc.canUndo());
            proc.undo();
            expectEquals (proc.getCurrentMeta().name, before);
        }

        beginTest ("SHOW / HIDE panels: the centre view expands and the layout is remembered");
        {
            editor->showView (ViceSynthEditor::Studio);
            auto* studioView = editor->getRoot().findChildWithID ("vsx_studio_view");
            expect (studioView != nullptr);
            const auto full = studioView != nullptr ? studioView->getBounds() : juce::Rectangle<int>();
            editor->setPanelVisible (MainHeader::PresetsPanel, false);
            editor->setPanelVisible (MainHeader::AIPanel, false);
            if (studioView != nullptr) expectEquals (studioView->getWidth(), full.getWidth() + 290 + 8 + 318 + 8);
            editor->setPanelVisible (MainHeader::MacrosPanel, false);
            editor->setPanelVisible (MainHeader::KeysPanel, false);
            if (studioView != nullptr) expectEquals (studioView->getHeight(), full.getHeight() + 104 + 8 + 136 + 8);
            expect (! proc.uiState.getProperty ("showAI", true));
            // a new editor restores the hidden layout
            base.reset();
            base.reset (proc.createEditor());
            editor = dynamic_cast<ViceSynthEditor*> (base.get());
            expect (editor != nullptr && ! editor->isPanelVisible (MainHeader::AIPanel) && ! editor->isPanelVisible (MainHeader::KeysPanel));
            if (editor != nullptr)
                for (int i = 0; i < MainHeader::NumPanels; ++i) editor->setPanelVisible (i, true);
            expect (editor != nullptr && editor->getRoot().findChildWithID ("vsx_ai_settings_gear") == nullptr, "no second settings button");
        }

        beginTest ("Oscillator waveform arrow applies analog shapes directly");
        {
            for (int shape = 0; shape < 5; ++shape)
            {
                proc.setParam (pid::O (1, pid::osc::Mode), 0.0f);
                OscSourceMenu::applyAnalogShape (proc, 1, shape);
                expectEquals ((int) proc.getParam (pid::O (1, pid::osc::Mode)), 1);
                expect (proc.getParam (pid::O (1, pid::osc::On)) > 0.5f);
            }
            expectWithinAbsoluteError (proc.getParam (pid::O (1, pid::osc::PulseWidth)), 0.25f, 0.001f);
            int arrows = 0;
            std::function<void (juce::Component&)> scan = [&] (juce::Component& c)
            {
                if (dynamic_cast<OscSourceArrow*> (&c) != nullptr) ++arrows;
                for (auto* ch : c.getChildren()) scan (*ch);
            };
            if (editor != nullptr) scan (editor->getRoot());
            expect (arrows >= 2 * kNumOscs, "a waveform arrow on every oscillator card and panel, found " + juce::String (arrows));
        }

        beginTest ("FM page: cartridge import, arrow-key loading, mute / solo, undo");
        if (editor != nullptr)
        {
            std::function<juce::Component* (juce::Component&, const juce::String&)> find = [&] (juce::Component& c, const juce::String& id) -> juce::Component*
            {
                if (c.getComponentID() == id) return &c;
                for (auto* ch : c.getChildren()) if (auto* f = find (*ch, id)) return f;
                return nullptr;
            };
            editor->showDesignPage (SoundDesignView::Fm);
            auto* fmEd = dynamic_cast<FmEditor*> (find (editor->getRoot(), "vsx_fm_editor"));
            expect (fmEd != nullptr && fmEd->isVisible() && fmEd->getParentComponent()->isVisible());
            if (fmEd != nullptr)
            {
                const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("vsx-gui-bank", ".syx");
                const auto bank = fm::makeBankSysex (fm::factoryVoices());
                file.replaceWithData (bank.getData(), bank.getSize());
                expect (fmEd->loadCartridge (file));
                expectEquals ((int) fmEd->getCartridge().size(), 32);
                expect (fmEd->getStatus().contains ("checksum OK"));
                auto* list = dynamic_cast<juce::ListBox*> (find (*fmEd, "vsx_fm_cartridge_list"));
                expect (list != nullptr);
                const auto before = proc.getCurrentMeta().name;
                if (list != nullptr)
                {
                    list->selectRow (0);                                    // loads at once
                    expectEquals (proc.getCurrentMeta().name, juce::String ("TINE PIANO"));
                    expect (list->keyPressed (juce::KeyPress (juce::KeyPress::downKey))); // next voice, no Enter
                    expectEquals (proc.getCurrentMeta().name, juce::String ("GLASS BELL"));
                    expectEquals ((int) proc.getParam (pid::O (0, pid::osc::Mode)), (int) OscMode::FM);
                    expect (proc.getPatch().getFmVoice (0) == fm::factoryVoices()[1]);
                    proc.undo();
                    expectEquals (proc.getCurrentMeta().name, juce::String ("TINE PIANO"));
                }
                fmEd->setSolo (2);
                expectEquals ((int) proc.getPatch().getFmVoice (0).opEnabled, 1 << 2);
                fmEd->setSolo (-1);
                fmEd->toggleMute (0);
                expectEquals ((int) proc.getPatch().getFmVoice (0).opEnabled, 0x3E);
                fmEd->toggleMute (0);
                file.deleteFile();
                juce::File broken = file.getSiblingFile ("vsx-broken.syx");
                broken.replaceWithData ("not a cartridge", 15);
                expect (! fmEd->loadCartridge (broken));
                expect (fmEd->getStatus().startsWith ("Cannot load"));
                broken.deleteFile();
                expect (fmEd->loadCartridge (file) == false); // deleted file is reported, not crashed on
                if (shotDir.isNotEmpty())
                {
                    fmEd->loadCartridge (file);
                    const auto bank2 = fm::makeBankSysex (fm::factoryVoices());
                    file.replaceWithData (bank2.getData(), bank2.getSize());
                    fmEd->loadCartridge (file);
                    if (list != nullptr) list->selectRow (0);
                    auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                    juce::File (shotDir).getChildFile ("vsx_fm.png").deleteFile();
                    juce::FileOutputStream os (juce::File (shotDir).getChildFile ("vsx_fm.png"));
                    juce::PNGImageFormat().writeImageToStream (img, os);
                    file.deleteFile();
                }
            }
            editor->showView (ViceSynthEditor::Studio);
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
