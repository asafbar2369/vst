#include "CloudPages.h"

using namespace cloud;

// ===========================================================================
void SynthPage::Bar::paint (juce::Graphics& g)
{
    g.setColour (panel);
    g.fillRect (getLocalBounds());
    g.setColour (line);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

SynthPage::SynthPage (DigitalComboProcessor& p, bool jdEngine, Targets t, juce::String name)
    : proc (p), jd (jdEngine), targets (t), pageName (std::move (name))
{
    if (targets != Targets::None)
    {
        juce::StringArray names;
        if (targets == Targets::JdTones) names = { "TONE A", "TONE B", "TONE C", "TONE D" };
        else if (targets == Targets::D50Partials) names = { "UPPER P1", "UPPER P2", "LOWER P1", "LOWER P2" };
        else names = { "UPPER TONE", "LOWER TONE" };
        const auto acc = engineColour (jd);
        for (int i = 0; i < names.size(); ++i)
        {
            auto b = std::make_unique<juce::TextButton> (names[i]);
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, acc);
            b->onClick = [this, i] { setTarget (i); };
            b->setTitle ("Edit " + names[i]);
            bar.addAndMakeVisible (*b);
            selector.push_back (std::move (b));
        }
        addAndMakeVisible (bar);
        header = &bar;
        headerHeight = 48;
        if (! selector.empty()) selector[0]->setToggleState (true, juce::dontSendNotification);
    }
    startTimerHz (6);
}

void SynthPage::resized()
{
    ScrollPage::resized();
    auto r = bar.getLocalBounds().reduced (10, 7);
    for (auto& b : selector)
    {
        b->setBounds (r.removeFromLeft (132));
        r.removeFromLeft (8);
    }
}

SynthPage::~SynthPage()
{
    stopTimer();
    header = nullptr;
}

int SynthPage::baseFor (Scope s) const noexcept
{
    switch (s)
    {
        case Scope::JdTone: return jd800::kToneOffset[juce::jlimit (0, 3, target)];
        case Scope::D50Partial:
        {
            const int bases[4] = { (int) d50::kUpperPartial1, (int) d50::kUpperPartial2, (int) d50::kLowerPartial1, (int) d50::kLowerPartial2 };
            return bases[juce::jlimit (0, 3, target)];
        }
        case Scope::D50Common:
        {
            const int tone = targets == Targets::D50Partials ? target / 2 : target;
            return tone == 0 ? (int) d50::kUpperCommon : (int) d50::kLowerCommon;
        }
        case Scope::Fixed: break;
    }
    return 0;
}

void SynthPage::setTarget (int idx)
{
    idx = juce::jlimit (0, juce::jmax (0, (int) selector.size() - 1), idx);
    target = idx;
    for (int i = 0; i < (int) selector.size(); ++i)
        selector[(size_t) i]->setToggleState (i == idx, juce::dontSendNotification);
    for (auto& b : bound)
        b.ctl->setOffset (baseFor (b.scope) + b.local);
    refreshDimming();
    repaint();
}

void SynthPage::addDimRule (ByteControl* c, int tone, int partial, bool synthOnly) { dims.push_back ({ c, tone, partial, synthOnly }); }

void SynthPage::refreshDimming()
{
    // D-50 structures 1-7: which partial of the tone is a synth partial (the others play a PCM wave)
    static const bool p1Synth[7] = { true, true, false, false, true, false, false };
    static const bool p2Synth[7] = { true, true, true, true, false, false, false };
    for (auto& d : dims)
    {
        const int commonBase = d.tone == 0 ? (int) d50::kUpperCommon : (int) d50::kLowerCommon;
        const int st = juce::jlimit (0, 6, (int) proc.getD50Byte (commonBase + d50::common::Structure));
        const bool synth = d.partial == 0 ? p1Synth[st] : p2Synth[st];
        d.ctl->setDimmed (d.synthOnly ? ! synth : synth);
    }
}

void SynthPage::collectControls (std::vector<ByteControl*>& out)
{
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        if (auto* b = dynamic_cast<ByteControl*> (&c)) { out.push_back (b); return; }
        for (auto* child : c.getChildren()) walk (*child);
    };
    walk (*this);
}

// ===========================================================================
namespace
{
using SP = SynthPage;
using Kind = ByteSpec::Kind;

struct Ctx
{
    DigitalComboProcessor& proc;
    SP& page;
    bool jd;
};

ByteControl* put (Ctx& c, Card& card, int local, SP::Scope sc, const juce::String& label = {}, int span = 1, Kind kind = Kind::Auto)
{
    auto ctl = std::make_unique<ByteControl> (c.proc, ByteSpec { c.jd, c.page.baseFor (sc) + local, label, kind });
    auto* raw = card.add (std::move (ctl), span);
    if (sc != SP::Scope::Fixed) c.page.addBound (raw, local, sc);
    return raw;
}

ByteControl* putAt (Ctx& c, Card& card, int absOffset, const juce::String& label = {}, int span = 1, Kind kind = Kind::Auto)
{
    return card.add (std::make_unique<ByteControl> (c.proc, ByteSpec { c.jd, absOffset, label, kind }), span);
}

// Envelope shape: segments of (time byte, level 0..1); a hold is inserted after segment holdAfter.
std::vector<float> envShape (float startLevel, const std::vector<std::pair<int, float>>& segs, int holdAfter, bool pitch = false)
{
    (void) pitch;
    std::vector<float> w;
    float total = 0.0f;
    for (auto& s : segs) { w.push_back (std::sqrt (dc::timeFromValue ((float) s.first)) + 0.04f); total += w.back(); }
    const float hold = holdAfter >= 0 ? total * 0.3f + 0.1f : 0.0f;
    total += hold;
    std::vector<float> pts;
    float x = 0.0f;
    pts.push_back (0.0f);
    pts.push_back (juce::jlimit (0.0f, 1.0f, startLevel));
    for (size_t i = 0; i < segs.size(); ++i)
    {
        x += w[i];
        pts.push_back (x / total);
        pts.push_back (juce::jlimit (0.0f, 1.0f, segs[i].second));
        if ((int) i == holdAfter)
        {
            x += hold;
            pts.push_back (x / total);
            pts.push_back (juce::jlimit (0.0f, 1.0f, segs[i].second));
        }
    }
    return pts;
}

std::unique_ptr<Card> newCard (const juce::String& t, bool jd, int cols) { return std::make_unique<Card> (t, engineColour (jd), cols); }

void finish (SP& page, std::unique_ptr<Card> c) { page.getFlow().addCard (c.release()); }

float lvl (DigitalComboProcessor& p, bool jd, int off, float scale = 100.0f)
{
    return (jd ? (float) p.getJdByte (off) : (float) p.getD50Byte (off)) / scale;
}

// ---------------------------------------------------------------------------------- JD-800
std::unique_ptr<SP> jdSound (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, true, SP::Targets::None, "SOUND");
    Ctx c { proc, *page, true };
    for (int t = 0; t < 4; ++t)
    {
        auto card = newCard ("TONE " + juce::String::charToString (juce::juce_wchar ('A' + t)), true, 3);
        card->setEnableLed ([&proc, t] { return ((proc.getJdByte (jd800::kCommonOffset + jd800::common::LayerTone) >> t) & 1) != 0; },
                            [&proc, t] (bool on)
                            {
                                proc.pushUndoPoint();
                                int m = proc.getJdByte (jd800::kCommonOffset + jd800::common::LayerTone);
                                m = on ? (m | (1 << t)) : (m & ~(1 << t));
                                if (auto* prm = proc.apvts.getParameter (params::jdToneId (t, "on")))
                                {
                                    prm->beginChangeGesture();
                                    prm->setValueNotifyingHost (prm->convertTo0to1 (on ? 1.0f : 0.0f));
                                    prm->endChangeGesture();
                                }
                                proc.setJdByteFromUi (jd800::kCommonOffset + jd800::common::LayerTone, m);
                            });
        card->setActivity ([&proc, t] { return proc.getJdActiveTones (t) > 0; });
        const int b = jd800::kToneOffset[t];
        putAt (c, *card, b + jd800::tone::WaveMsb, "Waveform", 3, Kind::Wave);
        putAt (c, *card, b + jd800::tone::PitchCoarse, "Coarse");
        putAt (c, *card, b + jd800::tone::PitchFine, "Fine");
        putAt (c, *card, b + jd800::tone::Level, "Level");
        putAt (c, *card, b + jd800::tone::FilterMode, "Filter");
        putAt (c, *card, b + jd800::tone::Cutoff, "Cutoff");
        putAt (c, *card, b + jd800::tone::Resonance, "Reso");
        putAt (c, *card, b + jd800::tone::TvfEnvDepth, "Env depth");
        putAt (c, *card, b + jd800::tone::CutoffKf, "Cutoff KF");
        putAt (c, *card, b + jd800::tone::TvaEnvVelo, "Velocity");
        finish (*page, std::move (card));
    }
    return page;
}

std::unique_ptr<SP> jdEnvelopes (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, true, SP::Targets::JdTones, "ENVELOPES");
    Ctx c { proc, *page, true };
    SP* pg = page.get();
    using namespace jd800::tone;
    const auto S = SP::Scope::JdTone;
    {
        auto card = newCard ("TVF ENVELOPE (FILTER)", true, 4);
        card->add (std::make_unique<EnvelopeGraph> ([&proc, pg]
        {
            const int b = pg->baseFor (S);
            auto g = [&] (int o) { return lvl (proc, true, b + o); };
            auto t = [&] (int o) { return (int) proc.getJdByte (b + o); };
            return envShape (g (TvfEnvDepth) > 0 ? 0.0f : 0.0f,
                             { { t (TvfT1), g (TvfL1) }, { t (TvfT2), g (TvfL2) }, { t (TvfT3), g (TvfSustain) }, { t (TvfT4), g (TvfL4) } }, 2);
        }, jdCol), 4);
        put (c, *card, TvfEnvDepth, S, "Depth"); put (c, *card, TvfEnvVelo, S, "Velocity"); put (c, *card, TvfEnvTimeVelo, S, "T.Velo"); put (c, *card, TvfEnvTimeKf, S, "Time KF");
        put (c, *card, TvfT1, S, "T1"); put (c, *card, TvfL1, S, "L1"); put (c, *card, TvfT2, S, "T2"); put (c, *card, TvfL2, S, "L2");
        put (c, *card, TvfT3, S, "T3"); put (c, *card, TvfSustain, S, "Sustain"); put (c, *card, TvfT4, S, "T4"); put (c, *card, TvfL4, S, "L4");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("TVA ENVELOPE (AMP)", true, 4);
        card->add (std::make_unique<EnvelopeGraph> ([&proc, pg]
        {
            const int b = pg->baseFor (S);
            auto g = [&] (int o) { return lvl (proc, true, b + o); };
            auto t = [&] (int o) { return (int) proc.getJdByte (b + o); };
            return envShape (0.0f, { { t (TvaT1), g (TvaL1) }, { t (TvaT2), g (TvaL2) }, { t (TvaT3), g (TvaSustain) }, { t (TvaT4), 0.0f } }, 2);
        }, jdCol), 4);
        put (c, *card, Level, S, "Level"); put (c, *card, TvaEnvVelo, S, "Velocity"); put (c, *card, TvaEnvTimeVelo, S, "T.Velo"); put (c, *card, TvaEnvTimeKf, S, "Time KF");
        put (c, *card, TvaT1, S, "T1"); put (c, *card, TvaL1, S, "L1"); put (c, *card, TvaT2, S, "T2"); put (c, *card, TvaL2, S, "L2");
        put (c, *card, TvaT3, S, "T3"); put (c, *card, TvaSustain, S, "Sustain"); put (c, *card, TvaT4, S, "T4");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("PITCH ENVELOPE", true, 4);
        card->add (std::make_unique<EnvelopeGraph> ([&proc, pg]
        {
            const int b = pg->baseFor (S);
            auto g = [&] (int o) { return lvl (proc, true, b + o); };
            auto t = [&] (int o) { return (int) proc.getJdByte (b + o); };
            return envShape (g (PEnvL0), { { t (PEnvT1), g (PEnvL1) }, { t (PEnvT2), 0.5f }, { t (PEnvT3), g (PEnvL2) } }, 1);
        }, jdCol), 4);
        put (c, *card, PEnvVelo, S, "Velocity"); put (c, *card, PEnvTimeVelo, S, "T.Velo"); put (c, *card, PEnvTimeKf, S, "Time KF"); put (c, *card, PEnvL0, S, "L0");
        put (c, *card, PEnvT1, S, "T1"); put (c, *card, PEnvL1, S, "L1"); put (c, *card, PEnvT2, S, "T2"); put (c, *card, PEnvT3, S, "T3");
        put (c, *card, PEnvL2, S, "L2");
        finish (*page, std::move (card));
    }
    return page;
}

std::unique_ptr<SP> jdModulation (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, true, SP::Targets::JdTones, "MODULATION");
    Ctx c { proc, *page, true };
    using namespace jd800::tone;
    const auto S = SP::Scope::JdTone;
    for (int l = 0; l < 2; ++l)
    {
        const int o = l * 6;
        auto card = newCard (l == 0 ? "LFO 1" : "LFO 2", true, 3);
        put (c, *card, Lfo1Wave + o, S, "Wave"); put (c, *card, Lfo1Rate + o, S, "Rate"); put (c, *card, Lfo1Delay + o, S, "Delay");
        put (c, *card, Lfo1Fade + o, S, "Fade"); put (c, *card, Lfo1Offset + o, S, "Offset"); put (c, *card, Lfo1KeyTrigger + o, S, "Key trig");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("PITCH ROUTING (MOD WHEEL)", true, 3);
        put (c, *card, Lfo1Sens, S, "LFO 1"); put (c, *card, Lfo2Sens, S, "LFO 2"); put (c, *card, LeverSens, S, "Mod wheel");
        put (c, *card, AftertouchModSens, S, "A-touch"); put (c, *card, BenderSw, S, "Bender"); put (c, *card, AftertouchBendSw, S, "A-touch bend");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("FILTER & AMP ROUTING", true, 3);
        put (c, *card, TvfLfoSelect, S, "TVF LFO"); put (c, *card, TvfLfoDepth, S, "TVF depth"); put (c, *card, TvfAftertouch, S, "TVF A-touch");
        put (c, *card, TvaLfoSelect, S, "TVA LFO"); put (c, *card, TvaLfoDepth, S, "TVA depth"); put (c, *card, TvaAftertouch, S, "TVA A-touch");
        finish (*page, std::move (card));
    }
    return page;
}

std::unique_ptr<SP> jdEffects (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, true, SP::Targets::None, "EFFECTS");
    Ctx c { proc, *page, true };
    using namespace jd800::effect;
    const int E = jd800::kEffectOffset;
    auto fx = [&] (Card& card, int local, const char* label, int span = 1) { putAt (c, card, E + local, label, span); };
    {
        auto card = newCard ("GROUP A  (DS PH SP EN)", true, 4);
        fx (*card, GroupASequence, "Order", 4); fx (*card, GroupABlock1, "Block 1"); fx (*card, GroupABlock1 + 1, "Block 2");
        fx (*card, GroupABlock1 + 2, "Block 3"); fx (*card, GroupABlock1 + 3, "Block 4");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("DISTORTION", true, 4);
        fx (*card, DistType, "Type", 2); fx (*card, DistDrive, "Drive"); fx (*card, DistLevel, "Level");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("PHASER", true, 5);
        fx (*card, PhaserManual, "Manual"); fx (*card, PhaserRate, "Rate"); fx (*card, PhaserDepth, "Depth");
        fx (*card, PhaserResonance, "Reso"); fx (*card, PhaserMix, "Mix");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("SPECTRUM", true, 4);
        for (int i = 0; i < 6; ++i) fx (*card, SpectrumBand1 + i, ("Band " + juce::String (i + 1)).toRawUTF8());
        fx (*card, SpectrumWidth, "Width");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("ENHANCER", true, 3);
        fx (*card, EnhancerSens, "Sens"); fx (*card, EnhancerMix, "Mix");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("GROUP B  (CHO DLY REV)", true, 4);
        fx (*card, GroupBSequence, "Order", 2); fx (*card, GroupBBalance, "Dry : FX"); fx (*card, GroupBBlock1, "Block 1");
        fx (*card, GroupBBlock1 + 1, "Block 2"); fx (*card, GroupBBlock1 + 2, "Block 3");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("CHORUS", true, 5);
        fx (*card, ChorusRate, "Rate"); fx (*card, ChorusDepth, "Depth"); fx (*card, ChorusDelay, "Delay");
        fx (*card, ChorusFeedback, "Feedback"); fx (*card, ChorusLevel, "Level");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("DELAY", true, 4);
        fx (*card, DelayCenterTap, "Centre tap"); fx (*card, DelayCenterLevel, "Centre lvl"); fx (*card, DelayLeftTap, "Left tap");
        fx (*card, DelayLeftLevel, "Left lvl"); fx (*card, DelayRightTap, "Right tap"); fx (*card, DelayRightLevel, "Right lvl");
        fx (*card, DelayFeedback, "Feedback");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("REVERB", true, 6);
        fx (*card, ReverbType, "Type", 2); fx (*card, ReverbPreDelay, "Pre-delay"); fx (*card, ReverbEarlyRef, "Early ref");
        fx (*card, ReverbHfDamp, "HF damp"); fx (*card, ReverbTime, "Time"); fx (*card, ReverbLevel, "Level");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("PATCH EQUALISER", true, 7);
        using namespace jd800::common;
        putAt (c, *card, LowFreq, "Low freq"); putAt (c, *card, LowGain, "Low gain"); putAt (c, *card, MidFreq, "Mid freq");
        putAt (c, *card, MidQ, "Mid Q"); putAt (c, *card, MidGain, "Mid gain"); putAt (c, *card, HighFreq, "High freq"); putAt (c, *card, HighGain, "High gain");
        finish (*page, std::move (card));
    }
    return page;
}

std::unique_ptr<SP> jdPerformance (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, true, SP::Targets::None, "PERFORMANCE");
    Ctx c { proc, *page, true };
    using namespace jd800::common;
    {
        auto card = newCard ("BEND & A-TOUCH", true, 3);
        putAt (c, *card, BenderUp, "Bend up"); putAt (c, *card, BenderDown, "Bend down"); putAt (c, *card, AftertouchBend, "A-touch bend");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("PORTAMENTO & SOLO", true, 5);
        putAt (c, *card, PortamentoSw, "Porta"); putAt (c, *card, PortamentoMode, "Mode"); putAt (c, *card, PortamentoTime, "Time");
        putAt (c, *card, SoloSw, "Solo"); putAt (c, *card, SoloLegato, "Legato");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("LEVEL", true, 1);
        putAt (c, *card, Level, "Level");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("KEY RANGES  (per tone)", true, 4);
        for (int t = 0; t < 4; ++t)
        {
            const juce::String L = juce::String::charToString (juce::juce_wchar ('A' + t));
            putAt (c, *card, KeyRangeLowA + 2 * t, L + " low"); putAt (c, *card, KeyRangeHighA + 2 * t, L + " high");
        }
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("VELOCITY & HOLD (per tone)", true, 4);
        for (int t = 0; t < 4; ++t)
            putAt (c, *card, jd800::kToneOffset[t] + jd800::tone::VelocityCurve, "Curve " + juce::String::charToString (juce::juce_wchar ('A' + t)));
        for (int t = 0; t < 4; ++t)
            putAt (c, *card, jd800::kToneOffset[t] + jd800::tone::HoldControl, "Hold " + juce::String::charToString (juce::juce_wchar ('A' + t)));
        finish (*page, std::move (card));
    }
    return page;
}

// ---------------------------------------------------------------------------------- D-50
std::unique_ptr<SP> d50Sound (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, false, SP::Targets::None, "SOUND");
    Ctx c { proc, *page, false };
    for (int tone = 0; tone < 2; ++tone)
    {
        const int common = tone == 0 ? (int) d50::kUpperCommon : (int) d50::kLowerCommon;
        const juce::String tn = tone == 0 ? "UPPER" : "LOWER";
        {
            auto card = newCard (tn + " TONE", false, 3);
            card->setActivity ([&proc, tone] { return proc.getD50ActiveTones (tone == 0) > 0; });
            putAt (c, *card, common + d50::common::Structure, "Structure", 3);
            putAt (c, *card, common + d50::common::PartialBalance, "P1 : P2");
            putAt (c, *card, common + d50::common::PEnvVelocity, "P-ENV velo");
            finish (*page, std::move (card));
        }
        for (int part = 0; part < 2; ++part)
        {
            const int base = tone == 0 ? (part == 0 ? (int) d50::kUpperPartial1 : (int) d50::kUpperPartial2)
                                       : (part == 0 ? (int) d50::kLowerPartial1 : (int) d50::kLowerPartial2);
            auto card = newCard (tn + " PARTIAL " + juce::String (part + 1), false, 4);
            card->setEnableLed ([&proc, common, part] { return ((proc.getD50Byte (common + d50::common::PartialMute) >> (part == 0 ? 1 : 0)) & 1) != 0; },
                                [&proc, common, part] (bool on)
                                {
                                    proc.pushUndoPoint();
                                    int m = proc.getD50Byte (common + d50::common::PartialMute);
                                    const int bit = part == 0 ? 2 : 1;
                                    proc.setD50ByteFromUi (common + d50::common::PartialMute, on ? (m | bit) : (m & ~bit));
                                });
            card->setActivity ([&proc, tone] { return proc.getD50ActiveTones (tone == 0) > 0; });
            auto* wave = putAt (c, *card, base + d50::partial::Waveform, "Synth wave", 2);
            auto* pcm = putAt (c, *card, base + d50::partial::PcmWave, "PCM wave", 2, Kind::Combo);
            auto* coarse = putAt (c, *card, base + d50::partial::Coarse, "Coarse");
            auto* fine = putAt (c, *card, base + d50::partial::Fine, "Fine");
            auto* cutoff = putAt (c, *card, base + d50::partial::TvfCutoff, "Cutoff");
            auto* reso = putAt (c, *card, base + d50::partial::TvfResonance, "Reso");
            auto* level = putAt (c, *card, base + d50::partial::TvaLevel, "Level");
            auto* tvf = putAt (c, *card, base + d50::partial::TvfEnvDepth, "TVF depth");
            auto* pw = putAt (c, *card, base + d50::partial::PulseWidth, "Pulse width");
            auto* velo = putAt (c, *card, base + d50::partial::TvaVelocity, "Velocity");
            (void) coarse; (void) fine; (void) level; (void) velo;
            page->addDimRule (wave, tone, part, true);
            page->addDimRule (pw, tone, part, true);
            page->addDimRule (cutoff, tone, part, true);
            page->addDimRule (reso, tone, part, true);
            page->addDimRule (tvf, tone, part, true);
            page->addDimRule (pcm, tone, part, false);
            finish (*page, std::move (card));
        }
    }
    page->refreshDimming();
    return page;
}

std::unique_ptr<SP> d50Envelopes (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, false, SP::Targets::D50Partials, "ENVELOPES");
    Ctx c { proc, *page, false };
    SP* pg = page.get();
    using namespace d50::partial;
    const auto P = SP::Scope::D50Partial;
    const auto C = SP::Scope::D50Common;
    {
        auto card = newCard ("TVF ENVELOPE (SYNTH PARTIALS)", false, 6);
        card->add (std::make_unique<EnvelopeGraph> ([&proc, pg]
        {
            const int b = pg->baseFor (P);
            auto g = [&] (int o) { return lvl (proc, false, b + o); };
            auto t = [&] (int o) { return (int) proc.getD50Byte (b + o); };
            return envShape (0.0f, { { t (TvfT1), g (TvfL1) }, { t (TvfT2), g (TvfL2) }, { t (TvfT3), g (TvfL3) }, { t (TvfT4), g (TvfSustain) }, { t (TvfT5), g (TvfEnd) * 0.0f } }, 3);
        }, d50Col), 6);
        put (c, *card, TvfEnvDepth, P, "Depth"); put (c, *card, TvfEnvVelocity, P, "Velocity"); put (c, *card, TvfEnvDepthKf, P, "Depth KF"); put (c, *card, TvfEnvTimeKf, P, "Time KF");
        put (c, *card, TvfKeyfollow, P, "Key Fol."); put (c, *card, TvfBiasPoint, P, "Bias point");
        put (c, *card, TvfT1, P, "T1"); put (c, *card, TvfT2, P, "T2"); put (c, *card, TvfT3, P, "T3"); put (c, *card, TvfT4, P, "T4"); put (c, *card, TvfT5, P, "T5"); put (c, *card, TvfBiasLevel, P, "Bias level");
        put (c, *card, TvfL1, P, "L1"); put (c, *card, TvfL2, P, "L2"); put (c, *card, TvfL3, P, "L3"); put (c, *card, TvfSustain, P, "Sustain"); put (c, *card, TvfEnd, P, "End");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("TVA ENVELOPE (AMP)", false, 6);
        card->add (std::make_unique<EnvelopeGraph> ([&proc, pg]
        {
            const int b = pg->baseFor (P);
            auto g = [&] (int o) { return lvl (proc, false, b + o); };
            auto t = [&] (int o) { return (int) proc.getD50Byte (b + o); };
            return envShape (0.0f, { { t (TvaT1), g (TvaL1) }, { t (TvaT2), g (TvaL2) }, { t (TvaT3), g (TvaL3) }, { t (TvaT4), g (TvaSustain) }, { t (TvaT5), g (TvaEnd) * 0.0f } }, 3);
        }, d50Col), 6);
        put (c, *card, TvaLevel, P, "Level"); put (c, *card, TvaVelocity, P, "Velocity"); put (c, *card, TvaVelocityFollow, P, "Velo follow"); put (c, *card, TvaTimeKf, P, "Time KF");
        put (c, *card, TvaBiasPoint, P, "Bias point"); put (c, *card, TvaBiasLevel, P, "Bias level");
        put (c, *card, TvaT1, P, "T1"); put (c, *card, TvaT2, P, "T2"); put (c, *card, TvaT3, P, "T3"); put (c, *card, TvaT4, P, "T4"); put (c, *card, TvaT5, P, "T5"); put (c, *card, TvaEnd, P, "End");
        put (c, *card, TvaL1, P, "L1"); put (c, *card, TvaL2, P, "L2"); put (c, *card, TvaL3, P, "L3"); put (c, *card, TvaSustain, P, "Sustain");
        finish (*page, std::move (card));
    }
    {
        using namespace d50::common;
        auto card = newCard ("PITCH ENVELOPE (TONE)", false, 5);
        card->add (std::make_unique<EnvelopeGraph> ([&proc, pg]
        {
            const int b = pg->baseFor (C);
            auto g = [&] (int o) { return lvl (proc, false, b + o); };
            auto t = [&] (int o) { return (int) proc.getD50Byte (b + o) * 2; };
            return envShape (g (PEnvL0), { { t (PEnvT1), g (PEnvL1) }, { t (PEnvT2), g (PEnvL2) }, { t (PEnvT3), g (PEnvSustain) }, { t (PEnvT4), g (PEnvEnd) } }, 2);
        }, d50Col), 5);
        put (c, *card, PEnvVelocity, C, "Velocity"); put (c, *card, PEnvTimeKf, C, "Time KF");
        put (c, *card, PEnvT1, C, "T1"); put (c, *card, PEnvT2, C, "T2"); put (c, *card, PEnvT3, C, "T3"); put (c, *card, PEnvT4, C, "T4");
        put (c, *card, PEnvL0, C, "L0"); put (c, *card, PEnvL1, C, "L1"); put (c, *card, PEnvL2, C, "L2"); put (c, *card, PEnvSustain, C, "Sustain"); put (c, *card, PEnvEnd, C, "End");
        finish (*page, std::move (card));
    }
    return page;
}

std::unique_ptr<SP> d50Modulation (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, false, SP::Targets::D50Partials, "MODULATION");
    Ctx c { proc, *page, false };
    using namespace d50::common;
    const auto C = SP::Scope::D50Common;
    const auto P = SP::Scope::D50Partial;
    for (int l = 0; l < 3; ++l)
    {
        auto card = newCard ("LFO " + juce::String (l + 1) + " (TONE)", false, 4);
        put (c, *card, Lfo1Wave + 4 * l, C, "Wave"); put (c, *card, Lfo1Rate + 4 * l, C, "Rate");
        put (c, *card, Lfo1Delay + 4 * l, C, "Delay"); put (c, *card, Lfo1Sync + 4 * l, C, "Sync");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("PITCH MOD (TONE)", false, 3);
        put (c, *card, PModLfoDepth, C, "LFO depth"); put (c, *card, PModLever, C, "Mod wheel"); put (c, *card, PModAftertouch, C, "A-touch");
        finish (*page, std::move (card));
    }
    {
        using namespace d50::partial;
        auto card = newCard ("PARTIAL ROUTING", false, 6);
        put (c, *card, LfoMode, P, "Pitch LFO", 2); put (c, *card, PEnvMode, P, "P-ENV mode"); put (c, *card, BendMode, P, "Bend mode");
        put (c, *card, PwVelocity, P, "PW velo"); put (c, *card, PwAftertouch, P, "PW A-touch");
        put (c, *card, PwLfoSelect, P, "PW LFO"); put (c, *card, PwLfoDepth, P, "PW depth");
        put (c, *card, TvfLfoSelect, P, "TVF LFO"); put (c, *card, TvfLfoDepth, P, "TVF depth"); put (c, *card, TvfAftertouch, P, "TVF A-touch");
        put (c, *card, TvaLfoSelect, P, "TVA LFO"); put (c, *card, TvaLfoDepth, P, "TVA depth"); put (c, *card, TvaAftertouch, P, "TVA A-touch");
        finish (*page, std::move (card));
    }
    return page;
}

std::unique_ptr<SP> d50Effects (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, false, SP::Targets::D50Tones, "EFFECTS");
    Ctx c { proc, *page, false };
    using namespace d50::common;
    const auto C = SP::Scope::D50Common;
    {
        auto card = newCard ("CHORUS (TONE)", false, 4);
        put (c, *card, ChorusType, C, "Type"); put (c, *card, ChorusRate, C, "Rate"); put (c, *card, ChorusDepth, C, "Depth"); put (c, *card, ChorusBalance, C, "Balance");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("EQUALISER (TONE)", false, 5);
        put (c, *card, LowEqFreq, C, "Low freq"); put (c, *card, LowEqGain, C, "Low gain"); put (c, *card, HighEqFreq, C, "High freq");
        put (c, *card, HighEqQ, C, "High Q"); put (c, *card, HighEqGain, C, "High gain");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("REVERB (PATCH)", false, 2);
        putAt (c, *card, (int) d50::kPatchFactors + d50::patch::ReverbType, "Type");
        putAt (c, *card, (int) d50::kPatchFactors + d50::patch::ReverbBalance, "Balance");
        finish (*page, std::move (card));
    }
    return page;
}

std::unique_ptr<SP> d50Performance (DigitalComboProcessor& proc)
{
    auto page = std::make_unique<SP> (proc, false, SP::Targets::None, "PERFORMANCE");
    Ctx c { proc, *page, false };
    const int P = (int) d50::kPatchFactors;
    using namespace d50::patch;
    {
        auto card = newCard ("KEY MODE & BALANCE", false, 3);
        putAt (c, *card, P + KeyMode, "Key mode", 3); putAt (c, *card, P + SplitPoint, "Split point"); putAt (c, *card, P + ToneBalance, "U : L");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("TUNING", false, 4);
        putAt (c, *card, P + UpperKeyShift, "Upper shift"); putAt (c, *card, P + LowerKeyShift, "Lower shift");
        putAt (c, *card, P + UpperFineTune, "Upper fine"); putAt (c, *card, P + LowerFineTune, "Lower fine");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("BEND & A-TOUCH", false, 2);
        putAt (c, *card, P + BenderRange, "Bend range"); putAt (c, *card, P + AftertouchBendRange, "A-touch bend");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("PORTAMENTO & HOLD", false, 3);
        putAt (c, *card, P + PortamentoMode, "Porta mode"); putAt (c, *card, P + PortamentoTime, "Porta time"); putAt (c, *card, P + HoldMode, "Hold mode");
        finish (*page, std::move (card));
    }
    {
        auto card = newCard ("VOLUME", false, 1);
        putAt (c, *card, P + TotalVolume, "Volume");
        finish (*page, std::move (card));
    }
    return page;
}
} // namespace

// ===========================================================================
juce::String pageTitle (int id)
{
    static const char* names[] = { "SOUND", "ENVELOPES", "MODULATION", "EFFECTS", "PERFORMANCE" };
    return names[juce::jlimit (0, kNumSynthPages - 1, id)];
}

std::unique_ptr<SynthPage> makeSynthPage (DigitalComboProcessor& proc, bool jd, int id)
{
    switch (id)
    {
        case PageSound:       return jd ? jdSound (proc)       : d50Sound (proc);
        case PageEnvelopes:   return jd ? jdEnvelopes (proc)   : d50Envelopes (proc);
        case PageModulation:  return jd ? jdModulation (proc)  : d50Modulation (proc);
        case PageEffects:     return jd ? jdEffects (proc)     : d50Effects (proc);
        default:              return jd ? jdPerformance (proc) : d50Performance (proc);
    }
}
