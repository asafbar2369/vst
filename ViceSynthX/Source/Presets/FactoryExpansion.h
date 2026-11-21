#pragma once

#include "PresetFormat.h"
#include <vector>

/**
    Factory library expansion (2026-11-21): presets assembled from curated building blocks.

    Every category has hand-designed VOICINGS (oscillator architecture: which engines, shapes, detune, layering) and
    TREATMENTS (filter model and response, envelopes, motion, effects). A preset is one voicing with one treatment;
    combinations are chosen so that no two presets in a category share either building block with the same partner,
    and the library test rejects acoustic near-duplicates. Descriptions are generated from the actual choices.
    FM presets use original six-operator voices designed by fmDesigns() (no ROM data).
    Names are generic (no trademarks or people). Levels are calibrated by the library test (Preset Volume).
*/
namespace vsx::FactoryExpansion
{
std::vector<PatchSpec> build();
}
