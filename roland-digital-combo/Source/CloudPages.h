#pragma once
// The five synth pages (SOUND, ENVELOPES, MODULATION, EFFECTS, PERFORMANCE) for each engine, built
// from the documented parameter tables. Only parameters the engines really use get a control here;
// everything else stays reachable on the Advanced page.

#include "CloudUI.h"

class SynthPage : public ScrollPage, private juce::Timer
{
public:
    enum class Scope { Fixed, JdTone, D50Partial, D50Common };
    enum class Targets { None, JdTones, D50Partials, D50Tones };

    SynthPage (DigitalComboProcessor&, bool jdEngine, Targets targets, juce::String name);
    ~SynthPage() override;

    void resized() override;
    juce::String getName() const { return pageName; }
    bool isJd() const noexcept { return jd; }
    int getTarget() const noexcept { return target; }
    int getNumTargets() const noexcept { return (int) selector.size(); }
    void setTarget (int idx);
    int baseFor (Scope) const noexcept;
    void addBound (ByteControl* c, int local, Scope s) { bound.push_back ({ c, local, s }); }
    void addDimRule (ByteControl* c, int tone, int partial, bool synthOnly);   // D-50: no effect on a PCM / synth partial
    juce::TextButton* getSelectorButton (int i) { return selector[(size_t) i].get(); }
    void refreshDimming();
    void collectControls (std::vector<ByteControl*>& out);

    DigitalComboProcessor& proc;

private:
    void timerCallback() override { if (isShowing()) refreshDimming(); }
    struct Bound { ByteControl* ctl; int local; Scope scope; };
    struct Dim { ByteControl* ctl; int tone, partial; bool synthOnly; };
    struct Bar : juce::Component { void paint (juce::Graphics&) override; };
    bool jd;
    Targets targets;
    juce::String pageName;
    int target = 0;
    Bar bar;
    std::vector<std::unique_ptr<juce::TextButton>> selector;
    std::vector<Bound> bound;
    std::vector<Dim> dims;
};

enum PageId { PageSound = 0, PageEnvelopes, PageModulation, PageEffects, PagePerformance, kNumSynthPages };
juce::String pageTitle (int pageId);
std::unique_ptr<SynthPage> makeSynthPage (DigitalComboProcessor&, bool jd, int pageId);
