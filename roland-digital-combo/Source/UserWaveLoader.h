#pragma once
// Loads a folder of the user's own WAV/AIFF files into a UserWaveSet.
//
// File naming: the file name must START with the wave number, e.g. "001 Marimba.wav",
// "48-drawbars.aif", "12.wav". D-50 folder: numbers 1-100 (PCM Wave Number as shown on the D-50).
// JD-800 folder: numbers 1-108 (Waveform number). Anything else is skipped and reported.
//
// Pitch: the root key comes from the file's sampler data (WAV "smpl" MIDI unity note, AIFF
// "INST" base note); without it the file is taken to be recorded at middle C (note 60).
// Loop: the first sustain loop (WAV smpl loop 0, AIFF sustain loop markers) is used; without one
// the file plays once (one-shot). Stereo files are mixed to mono.

#include <juce_audio_formats/juce_audio_formats.h>
#include "UserWaves.h"

namespace userwaves
{
constexpr int kMaxSecondsPerFile = 60;

struct Report
{
    int loaded = 0;
    juce::StringArray messages; // one line per skipped file / problem
};

// Reads one file. Returns false (and a reason) if it cannot be used.
bool readWaveFile (juce::AudioFormatManager&, const juce::File&, dc::SampleWave& out, juce::String& whyNot);

// Leading number of a file name, or -1.
int leadingNumber (const juce::String& fileName);

// Fills set.d50 (jd == false) or set.jd (jd == true) from the folder's files.
Report loadFolder (const juce::File& folder, bool jd, dc::UserWaveSet& set);
} // namespace userwaves
