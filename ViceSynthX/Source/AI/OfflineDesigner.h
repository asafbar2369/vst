#pragma once

#include "PatchOps.h"

namespace vsx
{
struct AudioFeatures;

/**
    Built-in, fully offline sound designer. Interprets English requests with a keyword /
    phrase grammar (instruments, styles, eras, hardware references, adjectives, comparatives,
    intensity words, variation requests) and produces real patches through the same recipe
    and transaction layer the cloud AI uses. Works without any network or AI service.
*/
class OfflineDesigner
{
public:
    DesignResult design (const juce::String& prompt, const PatchSpec& current, const AudioFeatures* reference = nullptr) const;

    /** Seeded musical variations of a patch. */
    static std::vector<PatchSpec> makeVariations (const PatchSpec& base, int count, uint32_t seed, juce::StringArray& notes);
};

} // namespace vsx
