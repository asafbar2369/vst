#include "EnvLfoEditors.h"
#include "../Synth/Modulation.h"

namespace vsx
{
using namespace pid;

EnvelopeEditor::EnvelopeEditor (ViceSynthProcessor& p, int e) : proc (p), env (e)
{
    setTooltip ("Drag the handles: attack (time), decay/sustain (time and level), release (time).");
}

float EnvelopeEditor::timeToX (float t, float total) const
{
    // square-root time axis keeps short and long envelopes readable
    const float w = (float) getWidth() - 12.0f;
    return 6.0f + w * std::sqrt (juce::jlimit (0.0f, 1.0f, t / total));
}

float EnvelopeEditor::xToTime (float x, float total) const
{
    const float w = (float) getWidth() - 12.0f;
    const float n = juce::jlimit (0.0f, 1.0f, (x - 6.0f) / w);
    return n * n * total;
}

EnvelopeEditor::Handles EnvelopeEditor::layout() const
{
    const float dl = proc.getParam (E (env, env::Delay)), a = proc.getParam (E (env, env::Attack)), h = proc.getParam (E (env, env::Hold));
    const float d = proc.getParam (E (env, env::Decay)), s = proc.getParam (E (env, env::Sustain)), r = proc.getParam (E (env, env::Release));
    const float sustainTime = 0.25f * std::max (0.4f, a + d);
    const float total = dragging >= 0 ? dragTotal : std::max (1.0f, (dl + a + h + d + sustainTime + r) * 1.1f);
    const float top = 6.0f, bottom = (float) getHeight() - 6.0f;
    auto y = [&] (float v) { return bottom - v * (bottom - top); };
    Handles hd;
    hd.totalTime = total;
    hd.a = { timeToX (dl + a, total), y (1.0f) };
    hd.d = { timeToX (dl + a + h + d, total), y (s) };
    hd.s = { timeToX (dl + a + h + d + sustainTime, total), y (s) };
    hd.r = { timeToX (dl + a + h + d + sustainTime + r, total), y (0.0f) };
    return hd;
}

void EnvelopeEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::deepBlack.withAlpha (0.9f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Theme::neonPink.withAlpha (0.4f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    const auto hd = layout();
    const float dl = proc.getParam (E (env, env::Delay)), h = proc.getParam (E (env, env::Hold));
    const float bottom = r.getBottom() - 6.0f;
    const float ac = proc.getParam (E (env, env::AttackCurve)), dc = proc.getParam (E (env, env::DecayCurve)), rc = proc.getParam (E (env, env::ReleaseCurve));
    auto curveSeg = [] (juce::Path& p, juce::Point<float> from, juce::Point<float> to, float curve)
    {
        const float kc = -curve * 6.0f;
        for (int i = 1; i <= 24; ++i)
        {
            const float t = (float) i / 24.0f;
            const float sh = std::abs (kc) < 1.0e-3f ? t : (1.0f - std::exp (-kc * t)) / (1.0f - std::exp (-kc));
            p.lineTo (from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * sh);
        }
    };
    juce::Path p;
    const juce::Point<float> start { timeToX (dl, hd.totalTime), bottom };
    p.startNewSubPath (6.0f, bottom);
    p.lineTo (start);
    curveSeg (p, start, hd.a, ac);
    const juce::Point<float> holdEnd { timeToX (dl + proc.getParam (E (env, env::Attack)) + h, hd.totalTime), hd.a.y };
    p.lineTo (holdEnd);
    curveSeg (p, holdEnd, hd.d, dc);
    p.lineTo (hd.s);
    curveSeg (p, hd.s, hd.r, rc);
    juce::Path fill (p);
    fill.lineTo (hd.r.x, bottom);
    fill.closeSubPath();
    juce::ColourGradient grad (Theme::neonPink.withAlpha (0.35f), 0.0f, 0.0f, Theme::electricPurple.withAlpha (0.05f), 0.0f, bottom, false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (Theme::neonPink);
    g.strokePath (p, juce::PathStrokeType (1.8f));
    for (auto pt : { hd.a, hd.d, hd.r })
    {
        g.setColour (Theme::deepBlack);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt));
        g.setColour (Theme::cyberCyan);
        g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pt), 1.5f);
    }
    // live level
    const float live = proc.getEngine().envDisplay[(size_t) env].load();
    g.setColour (Theme::cyberCyan.withAlpha (0.5f));
    g.drawHorizontalLine ((int) (bottom - live * (bottom - 6.0f)), 6.0f, r.getRight() - 6.0f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f, true));
    g.drawText ("ENV " + juce::String (env + 1) + (env == 0 ? " (AMP)" : env == 1 ? " (FILTER)" : ""), r.reduced (8.0f, 4.0f), juce::Justification::topLeft);
}

void EnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto hd = layout();
    dragTotal = hd.totalTime;
    const juce::Point<float> pts[] = { hd.a, hd.d, hd.r };
    dragging = -1;
    float best = 14.0f;
    for (int i = 0; i < 3; ++i)
        if (pts[i].getDistanceFrom (e.position) < best) { best = pts[i].getDistanceFrom (e.position); dragging = i; }
}

void EnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const float dl = proc.getParam (E (env, env::Delay)), a = proc.getParam (E (env, env::Attack)), h = proc.getParam (E (env, env::Hold));
    const float t = xToTime (e.position.x, dragTotal);
    const float bottom = (float) getHeight() - 6.0f;
    if (dragging == 0) proc.setParam (E (env, env::Attack), juce::jlimit (0.0f, 20.0f, t - dl));
    else if (dragging == 1)
    {
        proc.setParam (E (env, env::Decay), juce::jlimit (0.0f, 20.0f, t - dl - a - h));
        proc.setParam (E (env, env::Sustain), juce::jlimit (0.0f, 1.0f, (bottom - e.position.y) / (bottom - 6.0f)));
    }
    else
    {
        const auto hd = layout();
        const float sustainEnd = xToTime (hd.s.x, dragTotal);
        proc.setParam (E (env, env::Release), juce::jlimit (0.0f, 20.0f, t - sustainEnd));
    }
    repaint();
}

void EnvelopeEditor::mouseUp (const juce::MouseEvent&) { dragging = -1; repaint(); }

// =================================================================================
LfoEditor::LfoEditor (ViceSynthProcessor& p, int l) : proc (p), lfo (l)
{
    setTooltip ("Custom shape: click to add a point, drag to move, Shift-drag for curve tension, right-click to delete, double-click to reset.");
}

juce::Point<float> LfoEditor::toScreen (float x, float y) const
{
    auto r = getLocalBounds().toFloat().reduced (6.0f);
    return { r.getX() + x * r.getWidth(), r.getCentreY() - y * r.getHeight() * 0.48f };
}

juce::Point<float> LfoEditor::fromScreen (juce::Point<float> p) const
{
    auto r = getLocalBounds().toFloat().reduced (6.0f);
    return { juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()), juce::jlimit (-1.0f, 1.0f, (r.getCentreY() - p.y) / (r.getHeight() * 0.48f)) };
}

void LfoEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::deepBlack.withAlpha (0.9f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Theme::cyberCyan.withAlpha (0.4f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (Theme::electricPurple.withAlpha (0.25f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4.0f, r.getRight() - 4.0f);

    const int shape = (int) proc.getParam (L (lfo, lfo::Shape));
    const auto table = proc.getPatch().lfoTables[(size_t) lfo].get();
    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float ph = (float) i / 200.0f;
        const float v = Lfo::shapeValue (shape, ph, std::sin ((float) (i / 25) * 1.7f), std::sin ((float) (i / 25 + 1) * 1.7f), table);
        const auto pt = toScreen (ph, v);
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    g.setColour (Theme::cyberCyan.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (5.0f));
    g.setColour (Theme::cyberCyan);
    g.strokePath (p, juce::PathStrokeType (1.8f));
    if (shape == 7)
        for (const auto& pt : proc.getPatch().lfoCurves[(size_t) lfo].points)
        {
            g.setColour (Theme::neonPink);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (toScreen (pt.x, pt.y)));
        }
    const float live = proc.getEngine().lfoDisplay[(size_t) lfo].load();
    g.setColour (Theme::sunsetOrange);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getRight() - 10.0f, toScreen (0.0f, live).y }));
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f, true));
    g.drawText ("LFO " + juce::String (lfo + 1) + "  " + choices::lfoShapes[shape], r.reduced (8.0f, 4.0f), juce::Justification::topLeft);
}

void LfoEditor::mouseDown (const juce::MouseEvent& e)
{
    if ((int) proc.getParam (L (lfo, lfo::Shape)) != 7) return;
    auto curve = proc.getPatch().lfoCurves[(size_t) lfo];
    dragPoint = -1;
    for (int i = 0; i < (int) curve.points.size(); ++i)
        if (toScreen (curve.points[(size_t) i].x, curve.points[(size_t) i].y).getDistanceFrom (e.position) < 9.0f) dragPoint = i;
    if (e.mods.isPopupMenu())
    {
        if (dragPoint > 0 && dragPoint < (int) curve.points.size() - 1)
        {
            curve.points.erase (curve.points.begin() + dragPoint);
            proc.getPatch().setLfoCurve (lfo, curve);
        }
        dragPoint = -1;
        repaint();
        return;
    }
    if (dragPoint < 0)
    {
        const auto v = fromScreen (e.position);
        curve.points.push_back ({ v.x, v.y, 0.0f });
        std::sort (curve.points.begin(), curve.points.end(), [] (const auto& a, const auto& b) { return a.x < b.x; });
        for (int i = 0; i < (int) curve.points.size(); ++i)
            if (std::abs (curve.points[(size_t) i].x - v.x) < 1.0e-6f) dragPoint = i;
        proc.getPatch().setLfoCurve (lfo, curve);
    }
    if (dragPoint >= 0) dragStartTension = curve.points[(size_t) dragPoint].tension;
    repaint();
}

void LfoEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragPoint < 0) return;
    auto curve = proc.getPatch().lfoCurves[(size_t) lfo];
    if (dragPoint >= (int) curve.points.size()) return;
    auto& pt = curve.points[(size_t) dragPoint];
    if (e.mods.isShiftDown())
        pt.tension = juce::jlimit (-1.0f, 1.0f, dragStartTension - (float) e.getDistanceFromDragStartY() / 100.0f);
    else
    {
        const auto v = fromScreen (e.position);
        const bool edge = dragPoint == 0 || dragPoint == (int) curve.points.size() - 1;
        if (! edge)
        {
            const float lo = curve.points[(size_t) dragPoint - 1].x + 0.001f, hi = curve.points[(size_t) dragPoint + 1].x - 0.001f;
            pt.x = juce::jlimit (lo, hi, v.x);
        }
        pt.y = v.y;
    }
    proc.getPatch().setLfoCurve (lfo, curve);
    repaint();
}

void LfoEditor::mouseDoubleClick (const juce::MouseEvent&)
{
    if ((int) proc.getParam (L (lfo, lfo::Shape)) != 7) return;
    proc.getPatch().setLfoCurve (lfo, LfoCurve());
    repaint();
}

} // namespace vsx
