#pragma once

#include "Widgets.h"

namespace vsx
{
/** User wavetable creation & editing: draw, harmonics, frame management, morphing, import/export. */
class WavetableEditor : public juce::Component
{
public:
    WavetableEditor (ViceSynthProcessor& p, int oscIndex);
    ~WavetableEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onClose;

    // editing API (also used by tests)
    std::vector<std::vector<float>>& getFrames() { return frames; }
    void apply();
    void morphToLast (int numFrames);

private:
    class Canvas;
    class Harmonics;
    void setFrame (int f);
    void normalise();
    void smooth();
    void repaintViews();
    ViceSynthProcessor& proc;
    int osc;
    std::vector<std::vector<float>> frames;
    int current = 0;
    juce::String name { "My Wavetable" };
    std::unique_ptr<Canvas> canvas;
    std::unique_ptr<Harmonics> harmonics;
    juce::Slider frameSlider;
    juce::Label title, frameLabel;
    juce::TextEditor nameEditor;
    juce::TextButton addB { "ADD FRAME" }, delB { "DELETE FRAME" }, morphB { "MORPH TO LAST (16)" }, normB { "NORMALISE" }, smoothB { "SMOOTH" },
        sineB { "RESET SINE" }, importB { "IMPORT WAV" }, exportB { "EXPORT WAV" }, applyB { "APPLY TO OSC" }, saveB { "SAVE TO LIBRARY" }, closeB { "CLOSE" };
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace vsx
