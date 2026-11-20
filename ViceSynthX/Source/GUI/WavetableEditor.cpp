#include "WavetableEditor.h"

namespace vsx
{
static constexpr int N = Wavetable::kSize;

class WavetableEditor::Canvas : public juce::Component
{
public:
    explicit Canvas (WavetableEditor& o) : owner (o) { setMouseCursor (juce::MouseCursor::CrosshairCursor); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::deepBlack);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Theme::electricPurple.withAlpha (0.3f));
        g.drawHorizontalLine ((int) r.getCentreY(), 0.0f, r.getRight());
        const auto& f = owner.frames[(size_t) owner.current];
        juce::Path p;
        for (int i = 0; i < (int) r.getWidth(); ++i)
        {
            const float v = f[(size_t) (i * N / std::max (1, (int) r.getWidth()))];
            const float y = r.getCentreY() - v * r.getHeight() * 0.46f;
            if (i == 0) p.startNewSubPath (0.0f, y); else p.lineTo ((float) i, y);
        }
        g.setColour (Theme::neonPink.withAlpha (0.3f));
        g.strokePath (p, juce::PathStrokeType (4.0f));
        g.setColour (Theme::neonPink);
        g.strokePath (p, juce::PathStrokeType (1.6f));
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (11.0f));
        g.drawText ("Draw with the mouse", r.reduced (8.0f), juce::Justification::topLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override { last = e.position; draw (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { draw (e.position); }
private:
    void draw (juce::Point<float> pos)
    {
        auto& f = owner.frames[(size_t) owner.current];
        const float w = (float) getWidth(), h = (float) getHeight();
        auto toIdx = [&] (float x) { return juce::jlimit (0, N - 1, (int) (x / w * (float) N)); };
        auto toVal = [&] (float y) { return juce::jlimit (-1.0f, 1.0f, (h * 0.5f - y) / (h * 0.46f)); };
        int i0 = toIdx (last.x), i1 = toIdx (pos.x);
        float v0 = toVal (last.y), v1 = toVal (pos.y);
        if (i0 > i1) { std::swap (i0, i1); std::swap (v0, v1); }
        for (int i = i0; i <= i1; ++i)
            f[(size_t) i] = i1 == i0 ? v1 : v0 + (v1 - v0) * (float) (i - i0) / (float) (i1 - i0);
        last = pos;
        owner.repaintViews();
    }
    WavetableEditor& owner;
    juce::Point<float> last;
};

class WavetableEditor::Harmonics : public juce::Component
{
public:
    explicit Harmonics (WavetableEditor& o) : owner (o) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::deepBlack);
        g.fillRoundedRectangle (r, 6.0f);
        const auto amps = analyse();
        const float bw = r.getWidth() / 32.0f;
        for (int k = 0; k < 32; ++k)
        {
            const float h = r.getHeight() * juce::jlimit (0.0f, 1.0f, amps[(size_t) k]);
            g.setColour (k == 0 ? Theme::sunsetOrange : Theme::cyberCyan);
            g.fillRect (r.getX() + bw * (float) k + 1.0f, r.getBottom() - h, bw - 2.0f, h);
        }
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (11.0f));
        g.drawText ("Harmonics 1-32 (drag to edit: rebuilds the frame additively)", r.reduced (6.0f), juce::Justification::topLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override { amps = analyse(); edit (e); }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e); }
private:
    std::array<float, 32> analyse() const
    {
        std::array<float, 32> a {};
        const auto& f = owner.frames[(size_t) owner.current];
        for (int k = 1; k <= 32; ++k)
        {
            double re = 0.0, im = 0.0;
            for (int i = 0; i < N; i += 2)
            {
                const double ph = 2.0 * kPiD * k * i / N;
                re += f[(size_t) i] * std::cos (ph);
                im += f[(size_t) i] * std::sin (ph);
            }
            a[(size_t) k - 1] = (float) (2.0 * std::sqrt (re * re + im * im) / (N / 2));
        }
        return a;
    }
    void edit (const juce::MouseEvent& e)
    {
        const int k = juce::jlimit (0, 31, (int) ((float) e.x / ((float) getWidth() / 32.0f)));
        amps[(size_t) k] = juce::jlimit (0.0f, 1.0f, 1.0f - (float) e.y / (float) getHeight());
        auto& f = owner.frames[(size_t) owner.current];
        for (int i = 0; i < N; ++i)
        {
            float v = 0.0f;
            for (int h = 0; h < 32; ++h) v += amps[(size_t) h] * std::sin (kTwoPi * (float) (h + 1) * (float) i / (float) N);
            f[(size_t) i] = v;
        }
        owner.repaintViews();
    }
    WavetableEditor& owner;
    std::array<float, 32> amps {};
};

WavetableEditor::~WavetableEditor() = default;

void WavetableEditor::repaintViews()
{
    if (canvas) canvas->repaint();
    if (harmonics) harmonics->repaint();
}

WavetableEditor::WavetableEditor (ViceSynthProcessor& p, int o) : proc (p), osc (o)
{
    if (auto wt = proc.getPatch().osc[(size_t) o].wavetable.getShared())
    {
        frames = wt->rawFrames();
        name = wt->name + " Edit";
    }
    if (frames.empty())
    {
        frames.assign (1, std::vector<float> ((size_t) N));
        for (int i = 0; i < N; ++i) frames[0][(size_t) i] = std::sin (kTwoPi * (float) i / (float) N);
    }
    canvas = std::make_unique<Canvas> (*this);
    harmonics = std::make_unique<Harmonics> (*this);
    addAndMakeVisible (*canvas);
    addAndMakeVisible (*harmonics);
    title.setText ("WAVETABLE EDITOR  -  OSC " + juce::String::charToString ((juce::juce_wchar) ('A' + o)), juce::dontSendNotification);
    title.setFont (Theme::displayFont (16.0f));
    title.setColour (juce::Label::textColourId, Theme::neonPink);
    addAndMakeVisible (title);
    nameEditor.setText (name, false);
    addAndMakeVisible (nameEditor);
    frameSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    frameSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    frameSlider.setColour (juce::Slider::trackColourId, Theme::cyberCyan);
    frameSlider.onValueChange = [this] { setFrame ((int) frameSlider.getValue()); };
    addAndMakeVisible (frameSlider);
    frameLabel.setFont (Theme::font (12.0f, true));
    addAndMakeVisible (frameLabel);
    for (auto* b : { &addB, &delB, &morphB, &normB, &smoothB, &sineB, &importB, &exportB, &applyB, &saveB, &closeB }) addAndMakeVisible (b);
    applyB.setColour (juce::TextButton::buttonColourId, Theme::neonPink.withAlpha (0.6f));

    addB.onClick = [this] { if (frames.size() < 256) { frames.insert (frames.begin() + current + 1, frames[(size_t) current]); setFrame (current + 1); } };
    delB.onClick = [this] { if (frames.size() > 1) { frames.erase (frames.begin() + current); setFrame (std::min (current, (int) frames.size() - 1)); } };
    morphB.onClick = [this] { morphToLast (16); };
    normB.onClick = [this] { normalise(); };
    smoothB.onClick = [this] { smooth(); };
    sineB.onClick = [this] { for (int i = 0; i < N; ++i) frames[(size_t) current][(size_t) i] = std::sin (kTwoPi * (float) i / (float) N); repaint(); };
    applyB.onClick = [this] { apply(); };
    closeB.onClick = [this] { if (onClose) onClose(); };
    importB.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import wavetable", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            juce::String err;
            if (auto wt = Wavetable::fromAudioFile (fc.getResult(), err)) { frames = wt->rawFrames(); nameEditor.setText (wt->name, false); setFrame (0); }
        });
    };
    auto exportTo = [this] (const juce::File& f)
    {
        auto wt = Wavetable::fromFrames (frames, nameEditor.getText(), "custom:" + nameEditor.getText(), true);
        return wt != nullptr && wt->exportToWav (f);
    };
    exportB.onClick = [this, exportTo]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export wavetable", WavetableLibrary::instance().userFolder().getChildFile (nameEditor.getText() + ".wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [exportTo] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) exportTo (fc.getResult().withFileExtension ("wav")); });
    };
    saveB.onClick = [this, exportTo]
    {
        const auto f = WavetableLibrary::instance().userFolder().getChildFile (juce::File::createLegalFileName (nameEditor.getText()) + ".wav");
        if (exportTo (f))
        {
            proc.getPatch().setWavetable (osc, "user:" + f.getFullPathName(), true);
            proc.setParam (pid::O (osc, pid::osc::Mode), 0.0f);
        }
    };
    setFrame (0);
}

void WavetableEditor::setFrame (int f)
{
    current = juce::jlimit (0, (int) frames.size() - 1, f);
    frameSlider.setRange (0.0, (double) std::max (1, (int) frames.size() - 1), 1.0);
    frameSlider.setValue (current, juce::dontSendNotification);
    frameLabel.setText ("Frame " + juce::String (current + 1) + " / " + juce::String ((int) frames.size()), juce::dontSendNotification);
    repaint();
    if (canvas) canvas->repaint();
    if (harmonics) harmonics->repaint();
}

void WavetableEditor::morphToLast (int numFrames)
{
    if (frames.size() < 2)
    {
        frames.push_back (frames.front());
        for (int i = 0; i < N; ++i) frames.back()[(size_t) i] = (2.0f * (float) i / (float) N - 1.0f); // morph target: saw
    }
    const auto a = frames.front(), b = frames.back();
    frames.clear();
    for (int f = 0; f < numFrames; ++f)
    {
        const float t = (float) f / (float) (numFrames - 1);
        std::vector<float> fr ((size_t) N);
        for (int i = 0; i < N; ++i) fr[(size_t) i] = a[(size_t) i] + (b[(size_t) i] - a[(size_t) i]) * t;
        frames.push_back (fr);
    }
    setFrame (0);
}

void WavetableEditor::normalise()
{
    auto& f = frames[(size_t) current];
    float mean = 0.0f, peak = 1.0e-6f;
    for (float v : f) mean += v;
    mean /= (float) N;
    for (auto& v : f) { v -= mean; peak = std::max (peak, std::abs (v)); }
    for (auto& v : f) v /= peak;
    setFrame (current);
}

void WavetableEditor::smooth()
{
    auto& f = frames[(size_t) current];
    auto copy = f;
    for (int i = 0; i < N; ++i)
    {
        float s = 0.0f;
        for (int k = -8; k <= 8; ++k) s += copy[(size_t) ((i + k + N) % N)];
        f[(size_t) i] = s / 17.0f;
    }
    setFrame (current);
}

void WavetableEditor::apply()
{
    name = nameEditor.getText().trim().isNotEmpty() ? nameEditor.getText().trim() : juce::String ("My Wavetable");
    auto wt = Wavetable::fromFrames (frames, name, "custom:" + name + "#" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()), true);
    proc.getPatch().setWavetableObject (osc, wt);
    const int mode = (int) proc.getParam (pid::O (osc, pid::osc::Mode));
    if (mode != 0 && mode != 4 && mode != 5) proc.setParam (pid::O (osc, pid::osc::Mode), 0.0f);
}

void WavetableEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::deepBlack.withAlpha (0.75f));
    Theme::glassPanel (g, getLocalBounds().reduced (60, 40).toFloat(), 14.0f, Theme::neonPink);
}

void WavetableEditor::resized()
{
    auto r = getLocalBounds().reduced (60, 40).reduced (16);
    auto top = r.removeFromTop (30);
    title.setBounds (top.removeFromLeft (420));
    closeB.setBounds (top.removeFromRight (90));
    top.removeFromRight (8);
    nameEditor.setBounds (top.removeFromRight (240));
    r.removeFromTop (8);
    auto frameRow = r.removeFromTop (24);
    frameLabel.setBounds (frameRow.removeFromLeft (130));
    frameSlider.setBounds (frameRow);
    r.removeFromTop (8);
    auto buttons = r.removeFromBottom (30);
    layoutRow (buttons, { &addB, &delB, &morphB, &normB, &smoothB, &sineB, &importB, &exportB, &saveB, &applyB }, 4);
    r.removeFromBottom (8);
    harmonics->setBounds (r.removeFromBottom (r.getHeight() / 3));
    r.removeFromBottom (8);
    canvas->setBounds (r);
}

} // namespace vsx
