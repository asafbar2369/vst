// VICE SYNTH X shim (original): MTS-ESP microtuning is not used; this stub always reports "no master",
// so Dx7Note uses its standard tuning path.
#pragma once
#include <cmath>
struct MTSClient {};
inline bool MTS_HasMaster (const MTSClient*) { return false; }
inline double MTS_NoteToFrequency (const MTSClient*, char note, char) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }
