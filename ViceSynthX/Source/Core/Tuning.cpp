#include "Tuning.h"
#include <cmath>

namespace vsx::tuning
{
namespace
{
    // non-comment lines ('!' starts a comment line); leading / trailing whitespace removed
    juce::StringArray dataLines (const juce::String& text, bool keepEmptyFirst)
    {
        juce::StringArray all, out;
        all.addLines (text);
        for (auto l : all)
        {
            if (l.trimStart().startsWithChar ('!')) continue;
            if (out.isEmpty() && keepEmptyFirst) { out.add (l.trim()); continue; } // the description may be empty
            l = l.trim();
            if (l.isNotEmpty()) out.add (l);
        }
        return out;
    }

    // one pitch value: "701.955" (cents, has a '.') or "3/2" / "2" (ratio); text after the value is ignored
    bool parsePitch (const juce::String& line, double& cents)
    {
        const auto token = line.trim().upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("\t", false, false);
        if (token.isEmpty()) return false;
        if (token.containsChar ('.'))
        {
            if (! token.containsOnly ("+-0123456789.")) return false;
            cents = token.getDoubleValue();
            return std::isfinite (cents);
        }
        const auto num = token.upToFirstOccurrenceOf ("/", false, false), den = token.fromFirstOccurrenceOf ("/", false, false);
        if (! num.containsOnly ("0123456789") || num.isEmpty()) return false;
        if (token.containsChar ('/') && (den.isEmpty() || ! den.containsOnly ("0123456789"))) return false;
        const double n = num.getDoubleValue(), d = token.containsChar ('/') ? den.getDoubleValue() : 1.0;
        if (n <= 0.0 || d <= 0.0) return false;
        cents = 1200.0 * std::log2 (n / d);
        return std::isfinite (cents);
    }
}

bool parseScl (const juce::String& text, Scale& out, juce::String& error)
{
    out = {};
    const auto lines = dataLines (text, true);
    if (lines.size() < 2) { error = "Not a Scala scale: missing description or note count."; return false; }
    out.description = lines[0];
    const auto countText = lines[1].upToFirstOccurrenceOf (" ", false, false);
    if (! countText.containsOnly ("0123456789") || countText.isEmpty()) { error = "Invalid note count: " + lines[1]; return false; }
    const int n = countText.getIntValue();
    if (n < 1 || n > 1024) { error = "Note count out of range (1..1024): " + juce::String (n); return false; }
    if (lines.size() - 2 < n) { error = "The file lists " + juce::String (lines.size() - 2) + " pitches but declares " + juce::String (n) + "."; return false; }
    for (int i = 0; i < n; ++i)
    {
        double c = 0.0;
        if (! parsePitch (lines[2 + i], c)) { error = "Invalid pitch on degree " + juce::String (i + 1) + ": " + lines[2 + i]; return false; }
        out.cents.push_back (c);
    }
    if (out.cents.back() <= 0.0) { error = "The period (last degree) must be above the unison."; return false; }
    return true;
}

bool parseKbm (const juce::String& text, KeyMap& out, juce::String& error)
{
    out = {};
    const auto lines = dataLines (text, false);
    if (lines.size() < 7) { error = "Not a Scala keyboard mapping: 7 header values expected."; return false; }
    auto intAt = [&] (int i, int& v) { const auto t = lines[i].upToFirstOccurrenceOf (" ", false, false);
                                       if (! t.containsOnly ("-0123456789") || t.isEmpty()) return false; v = t.getIntValue(); return true; };
    if (! intAt (0, out.size) || ! intAt (1, out.first) || ! intAt (2, out.last) || ! intAt (3, out.middle) || ! intAt (4, out.refNote))
    { error = "Invalid header value in the keyboard mapping."; return false; }
    out.refFreq = lines[5].upToFirstOccurrenceOf (" ", false, false).getDoubleValue();
    if (! intAt (6, out.octaveDegree)) { error = "Invalid formal-octave degree."; return false; }
    if (out.size < 0 || out.size > 1024 || out.refFreq <= 0.0 || out.first < 0 || out.last > 127 || out.first > out.last
        || out.middle < 0 || out.middle > 127 || out.refNote < 0 || out.refNote > 127)
    { error = "Keyboard-mapping header out of range."; return false; }
    for (int i = 0; i < out.size; ++i)
    {
        if (7 + i >= lines.size()) { out.map.push_back (-1); continue; } // missing trailing entries are unmapped
        const auto t = lines[7 + i].upToFirstOccurrenceOf (" ", false, false);
        if (t.equalsIgnoreCase ("x")) out.map.push_back (-1);
        else if (t.containsOnly ("0123456789") && t.isNotEmpty()) out.map.push_back (t.getIntValue());
        else { error = "Invalid mapping entry " + juce::String (i) + ": " + lines[7 + i]; return false; }
    }
    return true;
}

Table standard()
{
    Table t {};
    for (int k = 0; k < 128; ++k) t[(size_t) k] = (float) k;
    return t;
}

Table build (const Scale& scale, const KeyMap* mapIn)
{
    const KeyMap def;
    const KeyMap& m = mapIn != nullptr ? *mapIn : def;
    const int n = (int) scale.cents.size();
    if (n == 0) return standard();
    const double period = scale.cents.back();
    auto degreeCents = [&] (long long degree) // any integer degree, period-repeating
    {
        const long long oct = degree >= 0 ? degree / n : -((-degree + n - 1) / n);
        const int d = (int) (degree - oct * n);
        return (double) oct * period + (d == 0 ? 0.0 : scale.cents[(size_t) d - 1]);
    };
    const int formal = m.octaveDegree > 0 ? m.octaveDegree : n;
    // scale degree of a key (or nothing when unmapped)
    auto degreeOf = [&] (int key, long long& degree)
    {
        const int steps = key - m.middle;
        if (m.size <= 0) { degree = steps; return true; }
        const int rep = steps >= 0 ? steps / m.size : -((-steps + m.size - 1) / m.size);
        const int idx = steps - rep * m.size;
        const int mapped = m.map[(size_t) idx];
        if (mapped < 0) return false;
        degree = (long long) rep * formal + mapped;
        return true;
    };
    long long refDegree = 0;
    const bool refOk = degreeOf (m.refNote, refDegree);
    const double refCents = refOk ? degreeCents (refDegree) : 0.0;
    const double refPitch = 69.0 + 12.0 * std::log2 (m.refFreq / 440.0);

    Table t {};
    for (int k = 0; k < 128; ++k)
    {
        if (k < m.first || k > m.last) { t[(size_t) k] = (float) k; continue; } // outside the retuned range: 12-TET
        long long degree = 0;
        if (! degreeOf (k, degree)) { t[(size_t) k] = kUnmapped; continue; }
        const double pitch = refPitch + (degreeCents (degree) - refCents) / 100.0;
        t[(size_t) k] = std::isfinite (pitch) ? (float) juce::jlimit (-100.0, 300.0, pitch) : kUnmapped;
    }
    return t;
}
} // namespace vsx::tuning
