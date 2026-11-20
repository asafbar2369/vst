#include "ArpSeq.h"

namespace vsx
{
using namespace pid;

void ArpSeq::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void ArpSeq::reset()
{
    held.clear();
    latched.clear();
    latchActive = false;
    sequence.clear();
    sequenceVel.clear();
    sounding.clear();
    arpIndex = 0;
    sampleClock = 0;
    freeBeat = 0.0;
    lastStep = -1;
    wasRunning = false;
    seqMod = 0.0f;
    currentStep = -1;
}

void ArpSeq::noteOnInput (int note, int vel, bool latch)
{
    if (latch)
    {
        // a new chord replaces the latched one once all keys were released
        if (held.empty() && latchActive)
            latched.clear();
        latchActive = true;
        latched.erase (std::remove_if (latched.begin(), latched.end(), [note] (const Held& h) { return h.note == note; }), latched.end());
        latched.push_back ({ note, vel, orderCounter });
    }
    held.erase (std::remove_if (held.begin(), held.end(), [note] (const Held& h) { return h.note == note; }), held.end());
    held.push_back ({ note, vel, orderCounter++ });
}

void ArpSeq::noteOffInput (int note, bool)
{
    held.erase (std::remove_if (held.begin(), held.end(), [note] (const Held& h) { return h.note == note; }), held.end());
}

void ArpSeq::buildArpSequence (const ParamSnapshot& p)
{
    const bool latch = p.getb (ARP (arp::Latch));
    std::vector<Held> notes = latch && latchActive ? latched : held;
    sequence.clear();
    sequenceVel.clear();
    if (notes.empty()) return;

    const int pattern = p.geti (ARP (arp::Pattern));
    if (pattern == 4) // as played
        std::sort (notes.begin(), notes.end(), [] (const Held& a, const Held& b) { return a.order < b.order; });
    else
        std::sort (notes.begin(), notes.end(), [] (const Held& a, const Held& b) { return a.note < b.note; });

    const int octaves = p.geti (ARP (arp::Octaves));
    const int octMode = p.geti (ARP (arp::OctaveMode));
    std::vector<int> octList;
    for (int o = 0; o < octaves; ++o) octList.push_back (o);
    if (octMode == 1) std::reverse (octList.begin(), octList.end());
    else if (octMode == 2 && octaves > 1)
        for (int o = octaves - 2; o > 0; --o) octList.push_back (o);

    std::vector<int> base, vel;
    for (int oc : octList)
        for (const auto& h : notes)
        {
            const int n = h.note + 12 * oc;
            if (n <= 127) { base.push_back (n); vel.push_back (h.velocity); }
        }
    if (octMode == 3 && octaves > 1)
    {
        // random octave per note: shuffle octave copies deterministically
        for (size_t i = 0; i < base.size(); ++i)
        {
            const int r = (int) (rng.next() % (uint32_t) octaves);
            base[i] = juce::jlimit (0, 127, notes[i % notes.size()].note + 12 * r);
        }
    }

    switch (pattern)
    {
        case 1: std::reverse (base.begin(), base.end()); std::reverse (vel.begin(), vel.end()); break;
        case 2: // up-down
        case 3: // down-up
        {
            if (pattern == 3) { std::reverse (base.begin(), base.end()); std::reverse (vel.begin(), vel.end()); }
            const size_t n = base.size();
            for (size_t i = n >= 2 ? n - 2 : 0; i >= 1 && i < n; --i) { base.push_back (base[i]); vel.push_back (vel[i]); }
            break;
        }
        case 7: // converge
        {
            std::vector<int> b2, v2;
            size_t lo = 0, hi = base.size() - 1;
            while (lo <= hi && hi < base.size())
            {
                b2.push_back (base[lo]); v2.push_back (vel[lo]);
                if (hi != lo) { b2.push_back (base[hi]); v2.push_back (vel[hi]); }
                ++lo; if (hi == 0) break; --hi;
            }
            base = b2; vel = v2;
            break;
        }
        default: break;
    }
    sequence = base;
    sequenceVel = vel;
}

void ArpSeq::allSoundingOff (juce::MidiBuffer& out, int offset)
{
    for (const auto& s : sounding)
        out.addEvent (juce::MidiMessage::noteOff (1, s.note), offset);
    sounding.clear();
}

void ArpSeq::flushOffs (juce::MidiBuffer& out, int64_t blockStart, int numSamples)
{
    for (auto it = sounding.begin(); it != sounding.end();)
    {
        if (it->offTime < blockStart + numSamples)
        {
            out.addEvent (juce::MidiMessage::noteOff (1, it->note), (int) juce::jlimit<int64_t> (0, numSamples - 1, it->offTime - blockStart));
            it = sounding.erase (it);
        }
        else
            ++it;
    }
}

void ArpSeq::triggerStep (int64_t step, int offset, juce::MidiBuffer& out, const ParamSnapshot& p, double stepSamples,
                          const ArpPattern* ap, const SeqPattern* sp)
{
    const int mode = p.geti (ARP (arp::Mode));
    const int64_t now = sampleClock + offset;

    auto play = [&] (int note, int vel, double lengthSamples)
    {
        note = juce::jlimit (0, 127, note);
        vel = juce::jlimit (1, 127, vel);
        // retrigger: close an identical sounding note first
        for (auto it = sounding.begin(); it != sounding.end(); ++it)
            if (it->note == note)
            {
                out.addEvent (juce::MidiMessage::noteOff (1, note), offset);
                sounding.erase (it);
                break;
            }
        out.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), offset);
        sounding.push_back ({ note, now + std::max<int64_t> (1, (int64_t) lengthSamples) });
    };

    if (mode == 1)
    {
        if (sequence.empty()) return;
        const int len = ap != nullptr ? juce::jlimit (1, 32, ap->length) : 16;
        const int idx = (int) (step % len);
        currentStep = idx;
        const ArpStep st = ap != nullptr ? ap->steps[(size_t) idx] : ArpStep();
        if (! st.on) return;
        if (rng.uni() > p.get (ARP (arp::Probability))) return;

        const double gate = p.get (ARP (arp::Gate)) * st.gate;
        const int pattern = p.geti (ARP (arp::Pattern));
        const int velMode = p.geti (ARP (arp::VelMode));
        auto velFor = [&] (int playedVel)
        {
            if (velMode == 1) return (int) (100.0f * st.velocity);
            if (velMode == 2) return (int) (127.0f * st.velocity);
            return playedVel;
        };

        if (pattern == 6) // chord
        {
            for (size_t i = 0; i < sequence.size(); ++i)
                play (sequence[i] + 12 * st.octave, velFor (sequenceVel[i]), gate * stepSamples);
        }
        else
        {
            int i;
            if (pattern == 5) i = (int) (rng.next() % (uint32_t) sequence.size());
            else i = arpIndex % (int) sequence.size();
            ++arpIndex;
            play (sequence[(size_t) i] + 12 * st.octave, velFor (sequenceVel[(size_t) i]), gate * stepSamples);
        }
    }
    else if (mode == 2 && sp != nullptr)
    {
        const int len = p.geti (SEQ (seq::Length));
        const int idx = (int) (step % len);
        currentStep = idx;
        const auto& st = sp->steps[(size_t) idx];
        seqMod = st.mod;
        if (! st.on) return;
        if (st.tie)
        {
            // extend currently sounding notes by one step
            for (auto& s : sounding) s.offTime += (int64_t) stepSamples;
            return;
        }
        if (rng.uni() > st.probability) return;

        int transpose = 0;
        const bool latch = p.getb (ARP (arp::Latch));
        const auto& src = latch && latchActive ? latched : held;
        if (p.getb (SEQ (seq::Follow)) && ! src.empty())
        {
            const auto last = std::max_element (src.begin(), src.end(), [] (const Held& a, const Held& b) { return a.order < b.order; });
            transpose = last->note - 60;
        }
        const int vel = (int) std::round (st.velocity * p.get (SEQ (seq::VelScale)) * 127.0f);
        const double len2 = st.gate * p.get (SEQ (seq::Gate)) * stepSamples;
        for (int n : st.notes)
            if (n != SeqStep::kNone)
                play (60 + transpose + n, vel, len2);
    }
}

void ArpSeq::process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples, const ParamSnapshot& p,
                      const ArpPattern* ap, const SeqPattern* sp, const TransportInfo& transport)
{
    const int mode = p.geti (ARP (arp::Mode));
    const bool latch = p.getb (ARP (arp::Latch));

    if (mode != lastMode)
    {
        allSoundingOff (out, 0);
        if (lastMode == 0)
        {
            // keys held while switching on are taken over by the arp/seq
        }
        lastMode = mode;
        lastStep = -1;
        arpIndex = 0;
    }

    if (mode == 0)
    {
        // pass-through: keep track of held keys so switching modes later behaves
        for (const auto meta : in)
        {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn()) noteOnInput (msg.getNoteNumber(), msg.getVelocity(), false);
            else if (msg.isNoteOff()) noteOffInput (msg.getNoteNumber(), false);
            out.addEvent (msg, meta.samplePosition);
        }
        sampleClock += numSamples;
        return;
    }

    if (! latch) { latched.clear(); latchActive = false; }

    // consume note input; pass other events through
    const bool wasEmpty = (latch && latchActive ? latched.empty() : held.empty());
    int firstNoteOffset = -1;
    for (const auto meta : in)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            noteOnInput (msg.getNoteNumber(), msg.getVelocity(), latch);
            if (firstNoteOffset < 0) firstNoteOffset = meta.samplePosition;
        }
        else if (msg.isNoteOff())
            noteOffInput (msg.getNoteNumber(), latch);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            held.clear(); latched.clear(); latchActive = false;
            allSoundingOff (out, meta.samplePosition);
            out.addEvent (msg, meta.samplePosition);
        }
        else
            out.addEvent (msg, meta.samplePosition);
    }
    buildArpSequence (p);

    const auto& active = latch && latchActive ? latched : held;
    const bool running = ! active.empty();

    const int rateIdx = mode == 1 ? p.geti (ARP (arp::Rate)) : p.geti (SEQ (seq::Rate));
    const double stepBeats = ParamRegistry::syncDivisionBeats (rateIdx);
    const double bpm = juce::jlimit (20.0, 999.0, transport.bpm);
    const double samplesPerBeat = sr * 60.0 / bpm;
    const double stepSamples = stepBeats * samplesPerBeat;
    const double swing = mode == 1 ? p.get (ARP (arp::Swing)) : p.get (SEQ (seq::Swing));

    flushOffs (out, sampleClock, numSamples);

    if (! running)
    {
        // release generated notes when no keys are held any more
        if (wasRunning) allSoundingOff (out, numSamples - 1);
        wasRunning = false;
        freeBeat += numSamples / samplesPerBeat;
        sampleClock += numSamples;
        currentStep = -1;
        return;
    }

    const bool hostSync = transport.isPlaying && transport.hasPpq;
    double beatStart;
    if (hostSync)
        beatStart = transport.ppqPosition;
    else
    {
        if (wasEmpty && firstNoteOffset >= 0)
        {
            // start the free-running clock exactly on the first key
            freeBeat = -(double) firstNoteOffset / samplesPerBeat;
            lastStep = -1;
            arpIndex = 0;
        }
        beatStart = freeBeat;
    }
    const double beatEnd = beatStart + numSamples / samplesPerBeat;

    // all step triggers whose (swung) time lies in [beatStart, beatEnd)
    int64_t firstStep = (int64_t) std::floor (beatStart / stepBeats) - 1;
    for (int64_t s = std::max<int64_t> (0, firstStep); ; ++s)
    {
        const double trig = (double) s * stepBeats + ((s & 1) ? swing * 0.5 * stepBeats : 0.0);
        if (trig >= beatEnd) break;
        if (trig < beatStart - 1.0e-9 || s <= lastStep) continue;
        const int offset = juce::jlimit (0, numSamples - 1, (int) std::floor ((trig - beatStart) * samplesPerBeat));
        triggerStep (s, offset, out, p, stepSamples, ap, sp);
        lastStep = s;
    }

    wasRunning = true;
    freeBeat = hostSync ? beatEnd : beatEnd;
    sampleClock += numSamples;
}

} // namespace vsx
