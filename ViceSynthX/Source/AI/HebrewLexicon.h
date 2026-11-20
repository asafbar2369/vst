#pragma once

#include <juce_core/juce_core.h>

namespace vsx
{
/**
    Hebrew support for the offline (rule-based) designer: maps Hebrew sound-design vocabulary
    (instruments, adjectives, comparatives, verbs, numbers, eras, synth names) onto the English
    grammar the designer understands. Handles the common attached prefixes (ו ה ב ל מ ש כ) and
    plural / feminine endings. Cloud and local LLM providers receive the original Hebrew text.
*/
namespace HebrewLexicon
{
    struct Translation
    {
        juce::String english;          // translated prompt (Hebrew words replaced, other text kept)
        juce::StringArray unknownWords; // Hebrew words that were not recognised
        int recognised = 0;
    };

    Translation translate (const juce::String& prompt);
}

} // namespace vsx
