#include "WaveDisplay.h"
#include "../Synth/Filters.h"
#include "../Synth/ExtraEngines.h"

namespace vsx
{
using namespace pid;

namespace
{
    void displayBackground (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (Theme::deepBlack.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Theme::electricPurple.withAlpha (0.12f));
        for (int i = 1; i < 8; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / 8.0f;
            g.drawVerticalLine ((int) x, r.getY() + 2.0f, r.getBottom() - 2.0f);
        }
        for (int i = 1; i < 4; ++i)
            g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX() + 2.0f, r.getRight() - 2.0f);
        g.setColour (Theme::electricPurple.withAlpha (0.5f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    void glowPath (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w)
    {
        g.setColour (c.withAlpha (0.18f));
        g.strokePath (p, juce::PathStrokeType (w * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (w * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

OscDisplay::OscDisplay (ViceSynthProcessor& p, int o) : proc (p), osc (o)
{
    setTooltip ("Drag left/right to morph the position. Double-click to edit the wavetable. Drop WAV/AIFF/SFZ files here.");
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void OscDisplay::refresh()
{
    const int mode = (int) proc.getParam (O (osc, osc::Mode));
    const float live = proc.getEngine().modDisplay[(size_t) O (osc, osc::Position)].load();
    const float pos = live >= 0.0f ? live : proc.getParam (O (osc, osc::Position));
    const void* res = proc.getPatch().osc[(size_t) osc].wavetable.get();
    if (mode != lastMode || std::abs (pos - lastPos) > 1.0e-3f || res != lastResource
        || mode == 2 || mode == 3 || mode == 4)
    {
        lastMode = mode; lastPos = pos; lastResource = res;
        repaint();
    }
}

void OscDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    displayBackground (g, r);
    r = r.reduced (6.0f, 8.0f);
    const int mode = (int) proc.getParam (O (osc, osc::Mode));
    const bool on = proc.getParam (O (osc, osc::On)) > 0.5f;
    const float live = proc.getEngine().modDisplay[(size_t) O (osc, osc::Position)].load();
    const float pos = live >= 0.0f ? live : proc.getParam (O (osc, osc::Position));
    auto& res = proc.getPatch().osc[(size_t) osc];

    juce::Graphics::ScopedSaveState ss (g);
    if (! on) g.setOpacity (0.35f);
    switch (mode)
    {
        case 0: drawWavetable (g, r, pos); break;
        case 1: drawAnalog (g, r, pos); break;
        case 2: drawSample (g, r, res.sample.get(), pos); break;
        case 3:
        {
            const auto* m = res.multi.get();
            const SampleData* s = (m != nullptr && ! m->zones.empty()) ? m->zones[m->zones.size() / 2].sample.get() : nullptr;
            drawSample (g, r, s, pos);
            g.setColour (Theme::sunsetOrange);
            g.setFont (Theme::font (11.0f, true));
            g.drawText (m != nullptr ? m->name + "  (" + juce::String ((int) m->zones.size()) + " zones)" : juce::String ("No multisample loaded"),
                        r.removeFromTop (14.0f), juce::Justification::topRight, true);
            break;
        }
        case 4:
            if (proc.getParam (O (osc, osc::Source)) > 0.5f && res.sample.get() != nullptr)
                drawSample (g, r, res.sample.get(), pos);
            else
                drawWavetable (g, r, pos);
            {
                const float spray = proc.getParam (O (osc, osc::GrainSpray));
                auto band = r.withX (r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, pos - spray * 0.5f))
                             .withWidth (std::max (2.0f, r.getWidth() * spray));
                g.setColour (Theme::cyberCyan.withAlpha (0.18f));
                g.fillRect (band);
            }
            break;
        case 5: drawSpectral (g, r, pos); break;
        case (int) OscMode::PhaseDist: drawPhaseDist (g, r); break;
        case (int) OscMode::Modal: drawModal (g, r, pos); break;
        case (int) OscMode::Physical:
        {
            // harmonic series shaped by the pick position (plucking at x cancels every 1/x-th harmonic)
            std::array<float, 32> a {};
            const float x = juce::jlimit (0.02f, 0.98f, pos);
            for (int k = 1; k <= 32; ++k) a[(size_t) k - 1] = std::abs (std::sin (kPi * (float) k * x)) / (float) k;
            drawBars (g, r, a.data(), 32, "Waveguide string  pick " + juce::String (juce::roundToInt (x * 100.0f)) + " %");
            break;
        }
        case (int) OscMode::Additive:
        {
            std::array<float, 64> a {};
            const int parts = juce::jlimit (1, 64, (int) proc.getParam (O (osc, osc::SpecPartials)));
            const float tilt = proc.getParam (O (osc, osc::SpecTilt)), oe = proc.getParam (O (osc, osc::SpecOddEven));
            const float fs = std::exp2 (-proc.getParam (O (osc, osc::SpecFormant)) / 12.0f);
            for (int k = 1; k <= parts; ++k)
            {
                const float kk = std::max (1.0f, (float) k * fs);
                float v = 1.0f / kk * std::pow (kk, tilt * 1.5f); // as AdditiveBank
                if (k % 2 == 0) v *= juce::jlimit (0.0f, 2.0f, 1.0f + oe);
                a[(size_t) k - 1] = v;
            }
            drawBars (g, r, a.data(), 64, "Additive  " + juce::String (parts) + " partials");
            break;
        }
        default: break;
    }
}

void OscDisplay::drawWavetable (juce::Graphics& g, juce::Rectangle<float> r, float pos)
{
    auto wt = proc.getPatch().osc[(size_t) osc].wavetable.getShared();
    if (wt == nullptr || wt->numFrames == 0)
    {
        g.setColour (Theme::textDim);
        g.drawText ("Loading...", r, juce::Justification::centred);
        return;
    }
    const int shown = std::min (wt->numFrames, 14);
    const float dx = r.getWidth() * 0.12f / (float) std::max (1, shown - 1);
    const float dy = r.getHeight() * 0.28f / (float) std::max (1, shown - 1);
    const auto area = r.withTrimmedRight (r.getWidth() * 0.12f).withTrimmedTop (r.getHeight() * 0.28f);
    auto framePath = [&] (int f, float ox, float oy)
    {
        const auto& fr = wt->rawFrame (f);
        juce::Path p;
        const int pts = 96;
        for (int i = 0; i <= pts; ++i)
        {
            const float x = area.getX() + ox + area.getWidth() * (float) i / pts;
            const float v = fr[(size_t) std::min ((int) fr.size() - 1, i * (int) fr.size() / pts)];
            const float y = area.getCentreY() - oy - v * area.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };
    for (int k = shown - 1; k >= 0; --k)
    {
        const int f = shown == 1 ? 0 : k * (wt->numFrames - 1) / (shown - 1);
        g.setColour (Theme::electricPurple.withAlpha (0.18f + 0.25f * (1.0f - (float) k / (float) shown)));
        g.strokePath (framePath (f, dx * (float) k, dy * (float) k), juce::PathStrokeType (1.0f));
    }
    const int cur = juce::jlimit (0, wt->numFrames - 1, (int) std::round (pos * (float) (wt->numFrames - 1)));
    const float k = pos * (float) (shown - 1);
    glowPath (g, framePath (cur, dx * k, dy * k), Theme::neonPink, 1.8f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    g.drawText (wt->name + "  " + juce::String (cur + 1) + "/" + juce::String (wt->numFrames), r.removeFromBottom (12.0f), juce::Justification::bottomRight);
}

void OscDisplay::drawAnalog (juce::Graphics& g, juce::Rectangle<float> r, float pos)
{
    const float pw = proc.getParam (O (osc, osc::PulseWidth));
    juce::Path p;
    const int pts = 160;
    for (int i = 0; i <= pts; ++i)
    {
        const float ph = (float) i / (float) pts * 2.0f; // two cycles
        const float q = ph - std::floor (ph);
        const float s = pos * 3.0f;
        const int seg = std::min (2, (int) s);
        const float t = s - (float) seg;
        const float sine = std::sin (kTwoPi * q);
        const float tri = 1.0f - 4.0f * std::abs (q + (q < 0.75f ? 0.25f : -0.75f) - 0.5f);
        const float saw = 2.0f * q - 1.0f;
        const float sq = q < pw ? 1.0f : -1.0f;
        const float v = seg == 0 ? lerp (sine, tri, t) : seg == 1 ? lerp (tri, saw, t) : lerp (saw, sq, t);
        const float x = r.getX() + r.getWidth() * (float) i / pts;
        const float y = r.getCentreY() - v * r.getHeight() * 0.42f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    glowPath (g, p, Theme::cyberCyan, 1.8f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    const char* names[] = { "Sine", "Triangle", "Saw", "Square" };
    g.drawText (juce::String ("Analog  ") + names[juce::jlimit (0, 3, (int) std::round (pos * 3.0f))], r.removeFromBottom (12.0f), juce::Justification::bottomRight);
}

void OscDisplay::drawSample (juce::Graphics& g, juce::Rectangle<float> r, const SampleData* s, float pos)
{
    if (s == nullptr)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        g.drawText ("Drop a WAV / AIFF / SFZ here or choose a built-in sample", r, juce::Justification::centred, true);
        return;
    }
    const int cols = std::max (8, (int) r.getWidth());
    if (s != cachedSample || (int) overview.size() != cols)
    {
        cachedSample = s;
        overview.assign ((size_t) cols, { 0.0f, 0.0f });
        const float* d = s->channel (0, 0);
        for (int c = 0; c < cols; ++c)
        {
            const int a = (int) ((int64_t) c * s->length / cols), b = std::max (a + 1, (int) ((int64_t) (c + 1) * s->length / cols));
            float mn = 0.0f, mx = 0.0f;
            for (int i = a; i < b && i < s->length; i += std::max (1, (b - a) / 64)) { mn = std::min (mn, d[i]); mx = std::max (mx, d[i]); }
            overview[(size_t) c] = { mn, mx };
        }
    }
    g.setColour (Theme::cyberCyan.withAlpha (0.8f));
    for (int c = 0; c < cols; ++c)
    {
        const float x = r.getX() + (float) c;
        g.drawVerticalLine ((int) x, r.getCentreY() - overview[(size_t) c].second * r.getHeight() * 0.48f,
                            r.getCentreY() - overview[(size_t) c].first * r.getHeight() * 0.48f + 1.0f);
    }
    const int loopMode = (int) proc.getParam (O (osc, osc::LoopMode));
    if (loopMode > 0)
    {
        const float ls = proc.getParam (O (osc, osc::LoopStart)), le = proc.getParam (O (osc, osc::LoopEnd));
        g.setColour (Theme::sunsetOrange.withAlpha (0.18f));
        g.fillRect (r.withX (r.getX() + r.getWidth() * ls).withWidth (r.getWidth() * (le - ls)));
    }
    g.setColour (Theme::neonPink);
    g.drawVerticalLine ((int) (r.getX() + r.getWidth() * pos), r.getY(), r.getBottom());
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    g.drawText (s->name + "  root " + juce::MidiMessage::getMidiNoteName (s->rootNote, true, true, 4), r.removeFromBottom (12.0f), juce::Justification::bottomRight);
}

void OscDisplay::drawSpectral (juce::Graphics& g, juce::Rectangle<float> r, float pos)
{
    auto& res = proc.getPatch().osc[(size_t) osc];
    std::array<float, 64> amps {};
    auto s = res.sample.getShared();
    const bool fromSample = proc.getParam (O (osc, osc::Source)) > 0.5f && s != nullptr && s->numSpecFrames > 0;
    if (fromSample)
    {
        const int f = juce::jlimit (0, s->numSpecFrames - 1, (int) (pos * (float) s->numSpecFrames));
        for (int k = 0; k < 64; ++k) amps[(size_t) k] = s->specAmps[(size_t) f * 64 + (size_t) k];
    }
    else if (auto wt = res.wavetable.getShared())
    {
        const int f = juce::jlimit (0, wt->numFrames - 1, (int) std::round (pos * (float) (wt->numFrames - 1)));
        for (int k = 0; k < 64; ++k) amps[(size_t) k] = wt->partialAmps (f)[k];
    }
    const float tilt = proc.getParam (O (osc, osc::SpecTilt));
    const float oe = proc.getParam (O (osc, osc::SpecOddEven));
    const int parts = (int) proc.getParam (O (osc, osc::SpecPartials));
    float mx = 1.0e-6f;
    for (int k = 0; k < 64; ++k)
    {
        float a = amps[(size_t) k] * std::pow ((float) (k + 1), tilt * 1.5f);
        if (k > 0) a *= (k % 2 == 1) ? juce::jlimit (0.0f, 2.0f, 1.0f + oe) : juce::jlimit (0.0f, 2.0f, 1.0f - oe);
        if (k >= parts) a = 0.0f;
        amps[(size_t) k] = a;
        mx = std::max (mx, a);
    }
    const float bw = r.getWidth() / 64.0f;
    for (int k = 0; k < 64; ++k)
    {
        const float db = juce::jlimit (-60.0f, 0.0f, gainToDb (amps[(size_t) k] / mx));
        const float h = r.getHeight() * (1.0f + db / 60.0f);
        juce::ColourGradient grad (Theme::neonPink, 0.0f, r.getBottom(), Theme::cyberCyan, 0.0f, r.getY(), false);
        g.setGradientFill (grad);
        g.fillRect (r.getX() + bw * (float) k + 0.5f, r.getBottom() - h, std::max (1.0f, bw - 1.5f), h);
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    g.drawText (fromSample ? "Spectral: sample partials" : "Spectral: wavetable partials", r.removeFromTop (12.0f), juce::Justification::topRight);
}

void OscDisplay::mouseDown (const juce::MouseEvent&)
{
    dragStartPos = proc.getParam (O (osc, osc::Position));
    if (auto* p = proc.getParameterObject (O (osc, osc::Position))) p->beginChangeGesture();
}

void OscDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const float v = juce::jlimit (0.0f, 1.0f, dragStartPos + (float) e.getDistanceFromDragStartX() / (float) std::max (1, getWidth()));
    proc.setParam (O (osc, osc::Position), v);
    repaint();
}

void OscDisplay::mouseUp (const juce::MouseEvent&)
{
    if (auto* p = proc.getParameterObject (O (osc, osc::Position))) p->endChangeGesture();
}

void OscDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onOpenEditor) onOpenEditor();
}

bool OscDisplay::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff")
            || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".sfz"))
            return true;
    return false;
}

void OscDisplay::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    proc.importSampleFiles (osc, files);
}

// =================================================================================
FilterDisplay::FilterDisplay (ViceSynthProcessor& p, int f) : proc (p), index (f) {}

void FilterDisplay::refresh()
{
    const float live = proc.getEngine().modDisplay[(size_t) F (index, flt::Cutoff)].load();
    const float cutoff = live >= 0.0f ? ParamRegistry::get()[F (index, flt::Cutoff)].fromNormalised (live) : proc.getParam (F (index, flt::Cutoff));
    const int model = (int) proc.getParam (F (index, flt::Model)), mode = (int) proc.getParam (F (index, flt::Mode));
    const float reso = proc.getParam (F (index, flt::Resonance)), drive = proc.getParam (F (index, flt::Drive)), ch = proc.getParam (F (index, flt::Character));
    const auto k = juce::String (model) + "/" + juce::String (mode) + "/" + juce::String ((int) cutoff) + "/" + juce::String (reso, 3) + "/"
                 + juce::String (drive, 2) + "/" + juce::String (ch, 2) + "/" + juce::String ((int) proc.getParam (F (index, flt::On)));
    if (k == key) return;
    key = k;
    std::array<float, 160> freqs {};
    for (size_t i = 0; i < freqs.size(); ++i) freqs[i] = 20.0f * std::pow (1000.0f, (float) i / (float) (freqs.size() - 1));
    measureFilterResponse (model, mode, cutoff, std::min (reso, 0.97f), drive, ch, 96000.0, freqs.data(), mags.data(), (int) freqs.size());
    repaint();
}

void FilterDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    displayBackground (g, r);
    r = r.reduced (4.0f);
    const bool on = proc.getParam (F (index, flt::On)) > 0.5f;
    juce::Path p, fill;
    for (size_t i = 0; i < mags.size(); ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (mags.size() - 1);
        const float db = juce::jlimit (-48.0f, 24.0f, mags[i]);
        const float y = r.getY() + r.getHeight() * (1.0f - (db + 48.0f) / 72.0f);
        if (i == 0) { p.startNewSubPath (x, y); fill.startNewSubPath (x, r.getBottom()); fill.lineTo (x, y); }
        else { p.lineTo (x, y); fill.lineTo (x, y); }
    }
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();
    const auto c = on ? Theme::sunsetOrange : Theme::textDim;
    juce::ColourGradient grad (c.withAlpha (0.35f), 0.0f, r.getY(), c.withAlpha (0.0f), 0.0f, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    glowPath (g, p, c, 1.6f);
    g.setColour (Theme::textDim.withAlpha (0.6f));
    g.setFont (Theme::font (9.5f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f), juce::Rectangle<float> (x - 14.0f, r.getBottom() - 11.0f, 28.0f, 10.0f), juce::Justification::centred);
    }
}

void Oscilloscope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    displayBackground (g, r);
    r = r.reduced (3.0f);
    const int w = proc.scopeWrite.load();
    juce::Path p;
    const int n = 512;
    for (int i = 0; i < n; ++i)
    {
        const int idx = (w - n + i + 2048) & 2047;
        const float v = 0.5f * (proc.scopeBuffer.getSample (0, idx) + proc.scopeBuffer.getSample (1, idx));
        const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
        const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * r.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    glowPath (g, p, Theme::cyberCyan, 1.2f);
}


// ---------------------------------------------------------------------------------
void OscDisplay::drawPhaseDist (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int shape = (int) proc.getParam (O (osc, osc::PdShape));
    const float amt = proc.getParam (O (osc, osc::PdAmount));
    juce::Path p;
    const int pts = 200;
    for (int i = 0; i <= pts; ++i)
    {
        const float ph = (float) i / (float) pts * 2.0f; // two cycles
        const float v = PhaseDistortion::shapeValue (ph - std::floor (ph), shape, amt);
        const float x = r.getX() + r.getWidth() * (float) i / pts;
        const float y = r.getCentreY() - v * r.getHeight() * 0.42f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    glowPath (g, p, Theme::cyberCyan, 1.8f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    g.drawText ("Phase Dist  " + choices::pdShapes[juce::jlimit (0, choices::pdShapes.size() - 1, shape)] + "  "
                    + juce::String (juce::roundToInt (amt * 100.0f)) + " %", r.removeFromBottom (12.0f), juce::Justification::bottomRight);
}

void OscDisplay::drawBars (juce::Graphics& g, juce::Rectangle<float> r, const float* amps, int count, const juce::String& label)
{
    float mx = 1.0e-6f;
    for (int k = 0; k < count; ++k) mx = std::max (mx, amps[k]);
    const float bw = r.getWidth() / (float) count;
    for (int k = 0; k < count; ++k)
    {
        // level in dB over a 48 dB range, so quiet partials stay visible
        const float db = juce::jlimit (-48.0f, 0.0f, juce::Decibels::gainToDecibels (amps[k] / mx, -48.0f));
        const float h = (db + 48.0f) / 48.0f * r.getHeight() * 0.85f;
        g.setColour (Theme::cyberCyan.withAlpha (0.35f + 0.5f * (db + 48.0f) / 48.0f));
        g.fillRect (r.getX() + k * bw + 0.5f, r.getBottom() - h, std::max (1.0f, bw - 1.0f), h);
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    g.drawText (label, r.removeFromBottom (12.0f), juce::Justification::bottomRight);
}

void OscDisplay::drawModal (juce::Graphics& g, juce::Rectangle<float> r, float pos)
{
    const int material = (int) proc.getParam (O (osc, osc::PhysMaterial));
    const float inharm = proc.getParam (O (osc, osc::PhysInharm));
    const float* ratios = nullptr;
    const int n = ModalModel::ratios (material, ratios);
    const float stretch = 1.0f + 0.25f * inharm;
    const float x0 = juce::jlimit (0.02f, 0.98f, pos);
    // mode frequencies on a log axis from 0.5 to 64 times the fundamental (as ModalModel: strike-position weighting)
    for (int k = 0; k < n; ++k)
    {
        const float rk = std::pow (ratios[k], stretch);
        const float xn = (std::log2 (rk) + 1.0f) / 7.0f;
        if (xn < 0.0f || xn > 1.0f) continue;
        const float amp = 1.0f / (1.0f + 0.35f * (float) k) * std::max (0.08f, std::abs (std::sin (kPi * (float) (k + 1) * x0)));
        const float h = juce::jlimit (0.05f, 1.0f, amp) * r.getHeight() * 0.85f;
        const float x = r.getX() + xn * r.getWidth();
        g.setColour (Theme::cyberCyan.withAlpha (0.85f));
        g.fillRect (x - 1.0f, r.getBottom() - h, 2.5f, h);
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    g.drawText ("Modal  " + choices::physMaterials[juce::jlimit (0, choices::physMaterials.size() - 1, material)]
                    + "  " + juce::String (n) + " modes", r.removeFromBottom (12.0f), juce::Justification::bottomRight);
}
} // namespace vsx
