#include "HebrewLexicon.h"
#include <cstring>
#include <map>

namespace vsx
{
namespace
{
    // UTF-8 source text: every entry is converted with juce::String::fromUTF8.
    struct Entry { const char* he; const char* en; };

    // Multi-word phrases are matched first (longest first).
    const Entry kPhrases[] = {
        { "פסנתר חשמלי", "electric piano" }, { "קופסת נגינה", "music box" }, { "כלי נשיפה", "brass" },
        { "טבלת גלים", "wavetable" }, { "שנות השמונים", "1980s" }, { "שנות התשעים", "1990s" },
        { "שנות השבעים", "1970s" }, { "שנות ה-80", "1980s" }, { "שנות ה-90", "1990s" }, { "שנות ה-70", "1970s" },
        { "שנות ה80", "1980s" }, { "שנות ה90", "1990s" }, { "שנות ה70", "1970s" }, { "מייקל ג'קסון", "michael jackson" },
        { "מייקל גקסון", "michael jackson" }, { "אני רוצה", "i want" }, { "תדר נמוך", "low end" },
        { "פחות חד", "less harsh" }, { "פחות צורם", "less harsh" }, { "יותר רחב", "wider" }, { "יותר כהה", "darker" },
        { "יותר בהיר", "brighter" }, { "יותר חם", "warmer" }, { "יותר רך", "softer" }, { "יותר מרחב", "more space" },
        { "יותר תנועה", "more movement" }, { "התקפה רכה", "soft attack" }, { "שחרור ארוך", "long release" },
        { "דרום אנד בס", "drum and bass" }, { "דראם אנד בס", "drum and bass" },
    };

    const Entry kWords[] = {
        // instruments / roles
        { "בס", "bass" }, { "באס", "bass" }, { "בסים", "bass" }, { "סאב", "sub" }, { "פד", "pad" }, { "פדים", "pads" },
        { "ליד", "lead" }, { "לידים", "lead" }, { "סולו", "solo" }, { "מלודיה", "melody" }, { "מנגינה", "melody" },
        { "מקהלה", "choir" }, { "מקהלות", "choir" }, { "קולות", "voices" }, { "קול", "voice" }, { "ווקאלי", "vocal" },
        { "ווקאל", "vocal" }, { "מיתרים", "strings" }, { "כינור", "violin" }, { "כינורות", "violins" }, { "צ'לו", "cello" },
        { "תזמורת", "orchestra" }, { "תזמורתי", "orchestral" }, { "אנסמבל", "ensemble" }, { "חצוצרה", "trumpet" },
        { "חצוצרות", "trumpets" }, { "טרומבון", "trombone" }, { "קרן", "horn" }, { "קרניים", "horns" }, { "ברס", "brass" },
        { "סטאב", "stab" }, { "פלאק", "pluck" }, { "פליק", "pluck" }, { "פריטה", "pluck" }, { "פיציקטו", "pizzicato" },
        { "גיטרה", "guitar" }, { "קלימבה", "kalimba" }, { "אורגן", "organ" }, { "עוגב", "organ" }, { "המונד", "hammond" },
        { "פסנתר", "piano" }, { "קלידים", "keys" }, { "רודס", "rhodes" }, { "וורלי", "wurli" }, { "קלאביקורד", "clav" },
        { "קלאבינט", "clavinet" }, { "פעמון", "bell" }, { "פעמונים", "bells" }, { "צלצול", "chime" }, { "מרימבה", "marimba" },
        { "ויברפון", "vibraphone" }, { "גלוקנשפיל", "glockenspiel" }, { "ארפג'יו", "arpeggio" }, { "ארפגיו", "arpeggio" },
        { "ארפג'יאטור", "arpeggiator" }, { "ארפ", "arp" }, { "סקוונס", "sequence" }, { "סיקוונס", "sequence" },
        { "סקוונסר", "sequencer" }, { "רצף", "sequence" }, { "דרון", "drone" }, { "טקסטורה", "texture" },
        { "טקסטורות", "textures" }, { "אווירה", "atmosphere" }, { "אטמוספרה", "atmosphere" }, { "אטמוספרי", "atmospheric" },
        { "נוף", "soundscape" }, { "אפקט", "fx" }, { "אפקטים", "fx" }, { "ריזר", "riser" }, { "לייזר", "laser" },
        { "סירנה", "siren" }, { "פיצוץ", "impact" }, { "סוויפ", "sweep" },
        // synthesis vocabulary
        { "גל", "wave" }, { "גלים", "waves" }, { "הרמוניות", "harmonics" }, { "הרמוני", "harmonic" }, { "גרנולרי", "granular" },
        { "גרנולרית", "granular" }, { "דגימה", "sample" }, { "דגימות", "samples" }, { "סאמפל", "sample" }, { "פילטר", "filter" },
        { "מסנן", "filter" }, { "רזוננס", "resonance" }, { "תהודה", "resonance" }, { "מעטפת", "envelope" }, { "התקפה", "attack" },
        { "אטאק", "attack" }, { "שחרור", "release" }, { "ריליס", "release" }, { "ויברטו", "vibrato" }, { "טרמולו", "tremolo" },
        { "יוניסון", "unison" }, { "דיטיון", "detune" }, { "רוורב", "reverb" }, { "הדהוד", "reverb" }, { "הד", "echo" },
        { "דיליי", "delay" }, { "השהיה", "delay" }, { "כורוס", "chorus" }, { "פלנג'ר", "flanger" }, { "פייזר", "phaser" },
        { "דיסטורשן", "distortion" }, { "עיוות", "distortion" }, { "סטריאו", "stereo" }, { "גלישה", "glide" }, { "פורטמנטו", "portamento" },
        // adjectives
        { "חם", "warm" }, { "חמים", "warm" }, { "חמה", "warm" }, { "חמימות", "warm" }, { "קר", "cold" }, { "קרה", "cold" },
        { "כהה", "dark" }, { "כהים", "dark" }, { "אפל", "dark" }, { "אפלה", "dark" }, { "עמום", "dark" }, { "בהיר", "bright" },
        { "בהירה", "bright" }, { "מואר", "bright" }, { "חד", "sharp" }, { "חדה", "sharp" }, { "צורם", "harsh" },
        { "צורמני", "harsh" }, { "רחב", "wide" }, { "רחבה", "wide" }, { "רחבים", "wide" }, { "צר", "narrow" },
        { "רך", "soft" }, { "רכה", "soft" }, { "רכים", "soft" }, { "עדין", "gentle" }, { "עדינה", "gentle" },
        { "אגרסיבי", "aggressive" }, { "אגרסיבית", "aggressive" }, { "שמן", "fat" }, { "שמנה", "fat" }, { "עבה", "thick" },
        { "דק", "thin" }, { "נקי", "clean" }, { "נקייה", "clean" }, { "מלוכלך", "dirty" }, { "מעוות", "distorted" },
        { "מרחב", "space" }, { "מרווח", "spacious" }, { "רטוב", "wet" }, { "יבש", "dry" }, { "יבשה", "dry" },
        { "תנועה", "movement" }, { "זז", "moving" }, { "נע", "moving" }, { "מתפתח", "evolving" }, { "מתפתחת", "evolving" },
        { "משתנה", "evolving" }, { "קולנועי", "cinematic" }, { "קולנועית", "cinematic" }, { "סינמטי", "cinematic" },
        { "עתידני", "futuristic" }, { "עתידנית", "futuristic" }, { "וינטג'", "vintage" }, { "וינטאג'", "vintage" },
        { "רטרו", "retro" }, { "אנלוגי", "analog" }, { "אנלוגית", "analog" }, { "אנלוגיים", "analog" }, { "דיגיטלי", "digital" },
        { "דיגיטלית", "digital" }, { "דיגיטליים", "digital" }, { "זכוכית", "glass" }, { "זכוכיתי", "glassy" }, { "קריסטל", "crystal" },
        { "נוצץ", "shimmering" }, { "נוצצת", "shimmering" }, { "מנצנץ", "shimmering" }, { "חלומי", "dreamy" }, { "חלומית", "dreamy" },
        { "עמוק", "deep" }, { "עמוקה", "deep" }, { "עוצמתי", "powerful" }, { "עוצמתית", "powerful" }, { "חזק", "powerful" },
        { "פאנצ'י", "punchy" }, { "פאנצי", "punchy" }, { "חובט", "punchy" }, { "ארוך", "long" }, { "ארוכה", "long" },
        { "קצר", "short" }, { "קצרה", "short" }, { "איטי", "slow" }, { "איטית", "slow" }, { "מהיר", "fast" }, { "מהירה", "fast" },
        { "אוורירי", "airy" }, { "אוורירית", "airy" }, { "נשימתי", "breathy" }, { "עשיר", "rich" }, { "עשירה", "rich" },
        { "מלא", "full" }, { "מלאה", "full" }, { "אורגני", "organic" }, { "אקוסטי", "acoustic" }, { "חשמלי", "electric" },
        { "חומצי", "acid" }, { "אסיד", "acid" }, { "פשוט", "simple" }, { "מודרני", "modern" }, { "קלאסי", "classic" },
        { "לבד", "solo" }, { "מונו", "mono" }, { "פוליפוני", "poly" },
        // genres / eras / names (descriptive references)
        { "אמביינט", "ambient" }, { "סינתוויב", "synthwave" }, { "פופ", "pop" }, { "פאנק", "funk" }, { "פאנקי", "funky" },
        { "ג'אז", "jazz" }, { "טראנס", "trance" }, { "האוס", "house" }, { "טכנו", "techno" }, { "דאבסטפ", "dubstep" },
        { "אייטיז", "80s" }, { "ניינטיז", "90s" }, { "מוג", "moog" }, { "יופיטר", "jupiter" }, { "ג'ונו", "juno" },
        { "פרופט", "prophet" }, { "אוברהיים", "oberheim" }, { "רולנד", "roland" }, { "ימהה", "yamaha" }, { "קורג", "korg" },
        { "וייבסטיישן", "wavestation" },
        // verbs / requests
        { "צור", "create" }, { "תיצור", "create" }, { "תצור", "create" }, { "צרי", "create" }, { "בנה", "build" },
        { "תבנה", "build" }, { "עצב", "design" }, { "תעצב", "design" }, { "תן", "give" }, { "תני", "give" }, { "הכן", "make" },
        { "תכין", "make" }, { "תעשה", "make" }, { "עשה", "make" }, { "תעשי", "make" }, { "הפוך", "make" }, { "תהפוך", "make" },
        { "שנה", "change" }, { "תשנה", "change" }, { "רוצה", "want" }, { "צריך", "need" }, { "חדש", "new" }, { "חדשה", "new" },
        { "הוסף", "add" }, { "תוסיף", "add" }, { "הורד", "less" }, { "תוריד", "less" }, { "התאם", "match" }, { "דומה", "similar" },
        // references to the current sound
        { "זה", "this" }, { "זאת", "this" }, { "אותו", "it" }, { "אותה", "it" }, { "נוכחי", "current" }, { "צליל", "sound" },
        { "סאונד", "sound" }, { "פריסט", "preset" }, { "פאץ'", "patch" }, { "וריאציה", "variation" }, { "וריאציות", "variations" },
        { "גרסה", "version" }, { "גרסאות", "versions" }, { "חלופות", "alternatives" }, { "רפרנס", "reference" }, { "קובץ", "file" },
        // comparatives / intensity / numbers
        { "יותר", "more" }, { "פחות", "less" }, { "מאוד", "very" }, { "ממש", "really" }, { "קצת", "bit" }, { "טיפה", "touch" },
        { "מעט", "slightly" }, { "הרבה", "much" }, { "בלי", "without" }, { "ללא", "without" }, { "עם", "with" },
        { "אחת", "one" }, { "אחד", "one" }, { "שתיים", "two" }, { "שתי", "two" }, { "שניים", "two" }, { "שני", "two" },
        { "שלוש", "three" }, { "שלושה", "three" }, { "ארבע", "four" }, { "ארבעה", "four" }, { "חמש", "five" },
        { "חמישה", "five" }, { "שש", "six" }, { "שישה", "six" }, { "שבע", "seven" }, { "שבעה", "seven" }, { "שמונה", "eight" },
        // function words that carry no meaning for the designer
        { "של", "" }, { "את", "" }, { "על", "" }, { "גם", "" }, { "אבל", "" }, { "כמו", "like" }, { "בסגנון", "style" },
        { "סגנון", "style" }, { "עבור", "for" }, { "בשביל", "for" }, { "לי", "" }, { "אני", "i" }, { "שלי", "" },
        { "בבקשה", "" }, { "אנא", "" }, { "משהו", "something" }, { "שיר", "track" }, { "הפקה", "production" },
        { "מתאים", "suitable" }, { "מתאימה", "suitable" }, { "לשיר", "track" },
    };

    const std::map<juce::String, juce::String>& words()
    {
        static const auto m = []
        {
            std::map<juce::String, juce::String> out;
            for (const auto& e : kWords) out[juce::String::fromUTF8 (e.he)] = e.en;
            return out;
        }();
        return m;
    }

    bool isHebrewChar (juce::juce_wchar c) { return c >= 0x05D0 && c <= 0x05EA; }

    bool hasHebrew (const juce::String& s)
    {
        for (auto p = s.getCharPointer(); ! p.isEmpty(); ++p) if (isHebrewChar (*p)) return true;
        return false;
    }

    /** Looks a Hebrew token up, trying attached prefixes (up to two) and plural / feminine endings. */
    bool lookup (const juce::String& token, juce::String& out)
    {
        const auto& m = words();
        const juce::String prefixes = juce::String::fromUTF8 ("והבלמשכ");
        auto tryWord = [&] (const juce::String& w) -> bool
        {
            if (w.length() < 1) return false;
            if (auto it = m.find (w); it != m.end()) { out = it->second; return true; }
            // final-letter forms and common endings
            for (const char* suffix : { "ים", "ות", "ה", "ת", "י" })
            {
                const auto suf = juce::String::fromUTF8 (suffix);
                if (w.length() > suf.length() + 1 && w.endsWith (suf))
                    if (auto it2 = m.find (w.dropLastCharacters (suf.length())); it2 != m.end()) { out = it2->second; return true; }
            }
            return false;
        };
        if (tryWord (token)) return true;
        juce::String w = token;
        for (int i = 0; i < 2 && w.length() > 2 && prefixes.containsChar (w[0]); ++i)
        {
            w = w.substring (1);
            if (w.startsWithChar ('-')) w = w.substring (1);
            if (tryWord (w)) return true;
        }
        return false;
    }
}

HebrewLexicon::Translation HebrewLexicon::translate (const juce::String& prompt)
{
    Translation t;
    juce::String text = prompt.replace (juce::String::fromUTF8 ("״"), "\"").replace (juce::String::fromUTF8 ("׳"), "'");

    // phrases first (longest first)
    std::vector<const Entry*> phrases;
    for (const auto& e : kPhrases) phrases.push_back (&e);
    std::sort (phrases.begin(), phrases.end(), [] (const Entry* a, const Entry* b) { return std::strlen (a->he) > std::strlen (b->he); });
    for (const auto* e : phrases)
    {
        const auto he = juce::String::fromUTF8 (e->he);
        if (text.contains (he)) { text = text.replace (he, " " + juce::String (e->en) + " "); ++t.recognised; }
    }

    // word by word
    juce::StringArray out;
    juce::String token;
    auto flush = [&]
    {
        if (token.isEmpty()) return;
        if (hasHebrew (token))
        {
            juce::String en;
            if (lookup (token, en)) { if (en.isNotEmpty()) out.add (en); ++t.recognised; }
            else t.unknownWords.addIfNotAlreadyThere (token);
        }
        else out.add (token);
        token.clear();
    };
    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = *p;
        const bool wordChar = isHebrewChar (c) || juce::CharacterFunctions::isLetterOrDigit (c) || c == '\'' || c == '-' || c == '"';
        if (wordChar) token += juce::String::charToString (c);
        else { flush(); if (! juce::CharacterFunctions::isWhitespace (c)) out.add (juce::String::charToString (c)); }
    }
    flush();
    t.english = out.joinIntoString (" ").replace (" ,", ",").replace (" .", ".").trim();
    return t;
}

} // namespace vsx
