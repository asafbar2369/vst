#pragma once

#include "../Core/PatchState.h"
#include "Modulation.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace vsx
{
struct TransportInfo
{
    bool isPlaying = false;
    bool hasPpq = false;
    double bpm = 120.0;
    double ppqPosition = 0.0;
};

/**
    Arpeggiator and polyphonic step sequencer. Converts incoming key presses into a
    sample-accurately timed note stream. Tempo-synced to the host transport when it is
    playing (ppq position), otherwise free-running from the first held key.
*/
class ArpSeq
{
public:
    void prepare (double sampleRate);
    void reset();

    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples, const ParamSnapshot& p,
                  const ArpPattern* arpPattern, const SeqPattern* seqPattern, const TransportInfo& transport);

    float getSeqMod() const noexcept { return seqMod; }
    int getCurrentStep() const noexcept { return currentStep; }
    int getMode() const noexcept { return lastMode; }

private:
    struct Held { int note, velocity; uint32_t order; };
    struct Sounding { int note; int64_t offTime; };

    void noteOnInput (int note, int vel, bool latch);
    void noteOffInput (int note, bool latch);
    void buildArpSequence (const ParamSnapshot& p);
    void triggerStep (int64_t step, int offset, juce::MidiBuffer& out, const ParamSnapshot& p, double stepSamples,
                      const ArpPattern* ap, const SeqPattern* sp);
    void allSoundingOff (juce::MidiBuffer& out, int offset);
    void flushOffs (juce::MidiBuffer& out, int64_t blockStart, int numSamples);

    double sr = 44100.0;
    std::vector<Held> held;
    std::vector<Held> latched;
    bool latchActive = false;
    uint32_t orderCounter = 0;
    std::vector<int> sequence;     // arp note sequence
    std::vector<int> sequenceVel;
    // Reusable working storage, reserved in prepare() before audio processing.
    std::vector<Held> scratchNotes;
    std::vector<int> scratchOctaves, scratchBase, scratchVel, scratchOrder, scratchOrderVel;
    int arpIndex = 0;
    std::vector<Sounding> sounding;
    int64_t sampleClock = 0;       // absolute samples processed
    double freeBeat = 0.0;         // free-running beat position
    double expectedBeat = -1.0e9;  // host beat expected at the next block (relocation detection)
    int64_t lastStep = -1;
    bool wasRunning = false;
    int lastMode = 0;
    float seqMod = 0.0f;
    int currentStep = -1;
    Rng rng { 0xC0FFEEu };
    int heldChecksum = 0;
};

} // namespace vsx
