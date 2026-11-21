// VICE SYNTH X factory library: original sound design built from code.
#include "PresetManager.h"
#include "SoundRecipes.h"
#include "FactoryExpansion.h"
#include <set>
#include "../Synth/FmVoice.h"

namespace vsx::FactoryPresets
{
using namespace Recipes;
using pid::FxDistortion; using pid::FxTube; using pid::FxTape; using pid::FxEq; using pid::FxMbComp; using pid::FxFreqShift;
using pid::FxRingMod; using pid::FxChorus; using pid::FxDimension; using pid::FxFlanger; using pid::FxPhaser; using pid::FxStereoDelay;
using pid::FxTapeDelay; using pid::FxDigitalDelay; using pid::FxReverb; using pid::FxConvolution; using pid::FxStereo; using pid::FxLimiter;
using A = Archetype;

juce::StringArray categories()
{
    return { "Analog", "Digital", "Bass", "Pads", "Leads", "Keys", "Plucks", "Strings", "Choirs", "Brass", "Bells", "Textures",
             "Cinematic", "Sound Effects", "Sequences", "Vintage 1980s", "Vintage 1990s", "FM" };
}

namespace
{
    float levelTrim (const juce::String& name);

    struct Lib
    {
        std::vector<PatchSpec> v;
        template <typename Fn>
        void add (const char* name, const char* category, std::initializer_list<const char*> tags, const char* desc, Fn&& fn)
        {
            PatchSpec s = init (name, category);
            fn (s);
            s.meta.name = name;
            s.meta.category = category;
            s.meta.description = desc;
            s.meta.source = "factory";
            s.meta.author = "VICE Factory";
            s.meta.tags.clear();
            for (auto* t : tags) s.meta.tags.add (t);
            s.set ("master_volume", levelTrim (name));
            v.push_back (std::move (s));
        }
    };

    /** Loudness calibration: master volume per preset so the loudest 400 ms window sits near
        -16 dBFS RMS with peaks <= -4 dBFS (measured offline by the calibration test, see Tests/TestPresets.cpp). */
    float levelTrim (const juce::String& name)
    {
        static const std::map<juce::String, float> trims {
        { "Golden Anthem", -14.8f },
        { "Royal Anthem", -9.6f },
        { "Sharp Arc", -9.0f },
        { "Neon Lead", -9.4f },
        { "Tender Sonata", -2.1f },
        { "Solemn Section", 2.0f },
        { "Royal Adagio", -11.0f },
        { "Warm Haze", -9.3f },
        { "Round Floor", -7.6f },
        { "Iron Basement", -7.3f },
        { "Euro Riser", -12.0f },
        { "Chrome Club", -7.0f },
        { "Neon Highway", -10.9f },
        { "Amber Voltage", -8.0f },
        { "Warm Voltage", -3.2f },
        { "Silver Engine", -6.0f },
        { "Crimson Drone", -8.4f },
        { "Bright FM Clav", -3.4f },
        { "Wah FM Clav", -6.4f },
        { "Muted FM Clav", -5.2f },
        { "Funk FM Clav", -3.8f },
        { "Flute FM Lead", -3.9f },
        { "Sync FM Lead", -6.0f },
        { "Hollow FM Lead", -4.7f },
        { "Saw FM Lead", -3.6f },
        { "Choir FM Pad", -3.0f },
        { "Metal FM Pad", -2.2f },
        { "Warm FM Pad", -2.3f },
        { "Crystal FM Pad", -4.3f },
        { "Spark FM Pluck", -6.2f },
        { "Nylon FM Pluck", -5.7f },
        { "Harp FM Pluck", -7.0f },
        { "Koto FM Pluck", -6.3f },
        { "Toy FM Mallet", -1.5f },
        { "Wood FM Block", -5.1f },
        { "Steel FM Vibes", -5.3f },
        { "Rose FM Marimba", -5.3f },
        { "Odd Harmonic Organ", -2.7f },
        { "Jazz FM Organ", -3.2f },
        { "Bright FM Organ", -2.1f },
        { "Full FM Drawbars", -1.8f },
        { "Punch FM Brass", -4.6f },
        { "Dark FM Horns", -6.2f },
        { "Bright FM Brass", -3.7f },
        { "Soft FM Horns", -3.4f },
        { "Picked FM Bass", -4.4f },
        { "Synth FM Bass", -0.9f },
        { "Wood FM Bass", -3.2f },
        { "Slap FM Bass", -4.7f },
        { "Tiny FM Bell", -5.4f },
        { "Temple FM Bell", -5.2f },
        { "Ice FM Bell", -6.3f },
        { "Cathedral FM Bell", -6.7f },
        { "Bell Tine EP", -5.4f },
        { "Round Body EP", -3.2f },
        { "Bark Tine EP", -7.3f },
        { "Satin Tine EP", -3.5f },
        { "Hidden Ruins", -7.4f },
        { "Deep Abyss", -6.5f },
        { "Shadow Vault", 9.0f },
        { "Eerie Signal", -5.9f },
        { "Burning Frontier", -3.4f },
        { "Ominous Nebula", 2.1f },
        { "Deep Eclipse", -0.0f },
        { "Shadow Ruins", -7.9f },
        { "Eerie Abyss", 1.5f },
        { "Burning Vault", -5.4f },
        { "Ominous Signal", -3.6f },
        { "Distant Frontier", 9.6f },
        { "Dark Nebula", 0.0f },
        { "Silent Horizon", -16.9f },
        { "Cold Expanse", 3.4f },
        { "Vast Monolith", -4.6f },
        { "Fading Omen", 3.3f },
        { "Hidden Descent", 6.4f },
        { "Silent Frontier", -7.5f },
        { "Cold Nebula", -13.7f },
        { "Vast Horizon", 3.5f },
        { "Fading Expanse", 0.7f },
        { "Hidden Monolith", 2.1f },
        { "Deep Omen", 1.8f },
        { "Shadow Descent", -6.1f },
        { "Eerie Eclipse", -10.6f },
        { "Burning Ruins", 10.3f },
        { "Ominous Abyss", 0.4f },
        { "Distant Vault", -5.6f },
        { "Dark Signal", 2.3f },
        { "Rainy Keys", -7.1f },
        { "Jazz Club", -8.8f },
        { "Soft Keys", -10.1f },
        { "Late Club", -7.4f },
        { "Warm Keys", -1.3f },
        { "Smoky Club", -5.5f },
        { "Bright Keys", -8.0f },
        { "Vintage Club", -8.8f },
        { "Mellow Keys", -9.4f },
        { "Velvet Club", -8.4f },
        { "Late Groove", -1.3f },
        { "Warm Ballad", -5.9f },
        { "Smoky Groove", -6.6f },
        { "Bright Ballad", -6.3f },
        { "Vintage Groove", -11.1f },
        { "Mellow Ballad", -8.3f },
        { "Velvet Groove", -1.3f },
        { "Rainy Ballad", -5.5f },
        { "Jazz Groove", -7.0f },
        { "Soft Ballad", -8.8f },
        { "Bright Session", -11.1f },
        { "Vintage Tines", -8.3f },
        { "Mellow Session", -0.4f },
        { "Velvet Tines", -4.7f },
        { "Rainy Session", -7.1f },
        { "Jazz Tines", -8.8f },
        { "Soft Session", -11.2f },
        { "Late Tines", -7.8f },
        { "Warm Session", -1.2f },
        { "Smoky Tines", -5.7f },
        { "Blazing Horns", -12.4f },
        { "Heroic Herald", -8.0f },
        { "Royal Horns", -8.0f },
        { "Proud Herald", 5.5f },
        { "Polished Horns", -10.2f },
        { "Bold Herald", -7.3f },
        { "Golden Horns", -7.6f },
        { "Mighty Herald", -6.2f },
        { "Gleaming Horns", -7.4f },
        { "Brassy Herald", 0.8f },
        { "Proud Call", -12.8f },
        { "Polished Trumpets", -4.8f },
        { "Bold Call", -6.8f },
        { "Golden Trumpets", -7.0f },
        { "Mighty Call", -10.9f },
        { "Gleaming Trumpets", -3.4f },
        { "Brassy Call", -9.4f },
        { "Blazing Trumpets", -3.8f },
        { "Heroic Call", -5.2f },
        { "Royal Trumpets", -9.6f },
        { "Mighty Brass", 0.2f },
        { "Gleaming Anthem", -7.5f },
        { "Brassy Brass", -3.1f },
        { "Blazing Anthem", -8.6f },
        { "Heroic Brass", -11.5f },
        { "Proud Brass", 2.8f },
        { "Polished Anthem", -8.1f },
        { "Bold Brass", -5.5f },
        { "Solar Signal", -6.6f },
        { "Electric Flare", -9.2f },
        { "Molten Phoenix", -4.0f },
        { "Neon Voice", -4.5f },
        { "Lucid Lead", -7.7f },
        { "Solar Blade", -8.3f },
        { "Electric Solo", -10.5f },
        { "Molten Signal", -8.6f },
        { "Neon Flare", -6.0f },
        { "Bright Phoenix", -9.2f },
        { "Burning Voice", -2.4f },
        { "Screaming Hero", -4.6f },
        { "Brave Comet", -10.8f },
        { "Singing Arc", -7.2f },
        { "Cutting Beam", -10.0f },
        { "Sharp Line", -7.3f },
        { "Screaming Phoenix", -4.3f },
        { "Brave Voice", -7.6f },
        { "Singing Hero", -4.1f },
        { "Cutting Comet", -4.1f },
        { "Lucid Beam", -5.9f },
        { "Solar Line", -8.9f },
        { "Electric Lead", -7.2f },
        { "Molten Blade", -4.4f },
        { "Neon Solo", -7.7f },
        { "Bright Signal", -5.2f },
        { "Burning Flare", -4.5f },
        { "Electric Beam", -7.7f },
        { "Molten Line", -5.6f },
        { "Bright Blade", -9.6f },
        { "Burning Solo", -6.1f },
        { "Brave Flare", -3.8f },
        { "Singing Phoenix", -3.6f },
        { "Cutting Voice", -7.5f },
        { "Sharp Hero", -6.9f },
        { "Lucid Comet", -12.3f },
        { "Solar Arc", -8.6f },
        { "Silver Spire", -8.5f },
        { "Tiny Bell", -3.1f },
        { "Glass Toll", -6.2f },
        { "Golden Gong", 0.2f },
        { "Shining Lantern", -1.4f },
        { "Temple Tines", -6.3f },
        { "Tiny Prism", -9.5f },
        { "Glass Spire", -0.3f },
        { "Golden Bell", -6.6f },
        { "Shining Toll", -0.2f },
        { "Temple Gong", -1.4f },
        { "Winter Lantern", -5.8f },
        { "Crystal Tines", -9.5f },
        { "Distant Chime", -1.5f },
        { "Ancient Peal", -9.2f },
        { "Frozen Carillon", -0.8f },
        { "Moon Glimmer", -1.2f },
        { "Silver Bowl", -4.5f },
        { "Distant Lantern", -6.6f },
        { "Ancient Tines", -3.6f },
        { "Frozen Chime", -7.8f },
        { "Moon Peal", -1.1f },
        { "Silver Carillon", 0.3f },
        { "Tiny Glimmer", -5.0f },
        { "Glass Bowl", -10.1f },
        { "Golden Prism", -2.5f },
        { "Shining Spire", -7.1f },
        { "Temple Bell", 0.9f },
        { "Winter Toll", -0.4f },
        { "Crystal Gong", -7.4f },
        { "Grand Strings", -4.6f },
        { "Distant Quartet", 3.1f },
        { "Lyrical Adagio", -16.7f },
        { "Bright Bows", 2.3f },
        { "Noble Sonata", -13.9f },
        { "Bowed Chamber", -9.9f },
        { "Distant Motif", -6.4f },
        { "Lyrical Strings", -7.1f },
        { "Bright Quartet", -4.6f },
        { "Noble Adagio", 3.1f },
        { "Bowed Bows", -12.2f },
        { "Royal Sonata", -5.9f },
        { "Silken Chamber", -15.6f },
        { "Tender Section", -10.7f },
        { "Somber Theme", 8.9f },
        { "Solemn Ensemble", -10.0f },
        { "Velvet Elegy", -16.0f },
        { "Grand Overture", -4.6f },
        { "Somber Chamber", -8.0f },
        { "Velvet Theme", -18.6f },
        { "Grand Ensemble", -4.1f },
        { "Distant Elegy", -12.2f },
        { "Lyrical Overture", -2.9f },
        { "Bright Motif", -5.5f },
        { "Noble Strings", 7.4f },
        { "Bowed Quartet", -8.7f },
        { "Silken Bows", -10.4f },
        { "Gentle Horizon", -11.1f },
        { "Floating Reverie", -11.4f },
        { "Opal Bloom", -8.4f },
        { "Velvet Horizon", -3.7f },
        { "Silent Reverie", -16.2f },
        { "Aurora Bloom", -9.3f },
        { "Endless Horizon", -0.4f },
        { "Misty Reverie", -1.4f },
        { "Frozen Bloom", -7.7f },
        { "Misty Echo", -9.6f },
        { "Frozen Cloud", -13.6f },
        { "Golden Echo", -5.8f },
        { "Lunar Cloud", -13.4f },
        { "Distant Haze", -20.6f },
        { "Polar Echo", 4.3f },
        { "Hollow Cloud", 1.1f },
        { "Gentle Haze", -7.1f },
        { "Floating Echo", -9.0f },
        { "Opal Cloud", -12.8f },
        { "Velvet Haze", -15.8f },
        { "Silent Echo", -7.1f },
        { "Aurora Cloud", -23.4f },
        { "Endless Haze", -14.2f },
        { "Aurora Sanctuary", 6.2f },
        { "Endless Garden", 2.1f },
        { "Misty Tide", -7.0f },
        { "Frozen Sanctuary", -9.5f },
        { "Warm Garden", -21.5f },
        { "Golden Tide", -11.5f },
        { "Lunar Sanctuary", -11.8f },
        { "Distant Garden", -18.7f },
        { "Polar Tide", -11.4f },
        { "Hollow Sanctuary", 8.0f },
        { "Gentle Garden", 7.0f },
        { "Floating Tide", -9.1f },
        { "Opal Sanctuary", -14.9f },
        { "Velvet Garden", -14.0f },
        { "Silent Tide", -10.8f },
        { "Velvet Floor", -8.9f },
        { "Rubber Weight", 4.7f },
        { "Tight Groove", -10.1f },
        { "Concrete Floor", -4.3f },
        { "Fat Weight", -11.3f },
        { "Thick Groove", -8.4f },
        { "Grim Floor", -8.9f },
        { "Dark Weight", -9.3f },
        { "Liquid Groove", -9.2f },
        { "Deep Weight", 3.7f },
        { "Sub Groove", -6.6f },
        { "Low Floor", -6.6f },
        { "Iron Weight", -5.3f },
        { "Heavy Groove", -8.8f },
        { "Heavy Rumble", -11.9f },
        { "Velvet Thump", -5.2f },
        { "Tight Rumble", 1.4f },
        { "Fat Basement", -2.1f },
        { "Thick Rumble", -9.6f },
        { "Grim Thump", -7.4f },
        { "Liquid Rumble", -8.1f },
        { "Round Thump", -9.7f },
        { "Deep Basement", -5.6f },
        { "Sub Rumble", 2.7f },
        { "Low Thump", -5.2f },
        { "Sub Root", -3.4f },
        { "Low Pulse", -7.9f },
        { "Iron Bounce", -10.0f },
        { "Heavy Root", -7.4f },
        { "Velvet Pulse", -12.4f },
        { "Rubber Bounce", -4.3f },
        { "Concrete Pulse", 6.5f },
        { "Fat Bounce", -8.2f },
        { "Thick Root", -4.4f },
        { "Grim Pulse", -10.9f },
        { "Dark Bounce", -6.8f },
        { "Round Pulse", -7.2f },
        { "Deep Bounce", -7.8f },
        { "Acid Signal", -8.6f },
        { "Ultra Stab", -7.1f },
        { "Acid Sequence", -7.3f },
        { "Ultra Rush Hour", -4.7f },
        { "Acid Hoover", -13.7f },
        { "Ultra Matrix", 2.3f },
        { "Acid Rush", -4.3f },
        { "Strobe Theory", -7.9f },
        { "Euro Engine", -8.3f },
        { "Strobe Pulse", -4.6f },
        { "Strobe Signal", -10.2f },
        { "Euro Stab", -3.6f },
        { "Strobe Sequence", 1.1f },
        { "Euro Rush Hour", -6.6f },
        { "Strobe Hoover", -8.0f },
        { "Euro Matrix", -6.7f },
        { "Strobe Rush", -9.0f },
        { "Euro Anthem", -0.5f },
        { "Strobe Vortex", -0.0f },
        { "Euro Gate", -9.2f },
        { "Warehouse Riser", 1.9f },
        { "Hyper Signal", -7.5f },
        { "Warehouse Stab", -6.0f },
        { "Hyper Sequence", -9.2f },
        { "Warehouse Rush Hour", -0.6f },
        { "Hyper Hoover", -2.1f },
        { "Warehouse Matrix", -5.8f },
        { "Hyper Rush", -11.5f },
        { "Warehouse Anthem", -0.9f },
        { "Hyper Vortex", -2.5f },
        { "Warehouse Gate", -10.0f },
        { "Hyper Theory", 0.1f },
        { "Warehouse Engine", -4.1f },
        { "Hyper Pulse", -8.6f },
        { "Ocean Avenue", -0.8f },
        { "Chrome Palms", -12.4f },
        { "Ocean Motion", -11.8f },
        { "Chrome Skyline", -4.3f },
        { "Ocean Cruise", -10.2f },
        { "Chrome Dream", -5.2f },
        { "Night Highway", -8.4f },
        { "Laser Signal", -7.4f },
        { "Night Horizon", -9.4f },
        { "Laser Glow", -4.7f },
        { "Night Club", -4.1f },
        { "Laser Avenue", -12.0f },
        { "Night Palms", -4.4f },
        { "Laser Motion", -4.4f },
        { "Night Skyline", -8.5f },
        { "Laser Cruise", -14.9f },
        { "Night Dream", -1.7f },
        { "Laser Drive", -8.5f },
        { "Night Heat", -10.8f },
        { "Laser Fantasy", -5.9f },
        { "Video Glow", -4.5f },
        { "Neon Club", -5.2f },
        { "Video Avenue", -14.4f },
        { "Neon Palms", -6.8f },
        { "Video Motion", -4.9f },
        { "Neon Skyline", -4.4f },
        { "Video Cruise", -4.3f },
        { "Neon Dream", -6.0f },
        { "Video Drive", -6.7f },
        { "Neon Heat", -11.9f },
        { "Video Fantasy", -8.9f },
        { "Video Signal", -1.7f },
        { "Neon Horizon", -3.7f },
        { "Solar Engine", -8.4f },
        { "Faded Keys", -14.2f },
        { "Silver Circuit", -3.7f },
        { "Velvet Saw", -10.2f },
        { "Dusty Glow", -6.4f },
        { "Electric Stab", -9.0f },
        { "Rust Current", -6.9f },
        { "Copper Wave", -8.4f },
        { "Crimson Drift", -11.8f },
        { "Analog Pulse", -11.7f },
        { "Royal Motion", -10.9f },
        { "Golden Brass", -7.4f },
        { "Crimson Current", -8.0f },
        { "Analog Wave", -11.6f },
        { "Royal Drift", -3.6f },
        { "Golden Pulse", -10.7f },
        { "Amber Motion", -13.6f },
        { "Bronze Brass", -7.0f },
        { "Smoky Poly", -11.1f },
        { "Cobalt Signal", -5.5f },
        { "Solar Ladder", -9.6f },
        { "Faded Sweep", -9.6f },
        { "Velvet Keys", -7.7f },
        { "Dusty Circuit", -13.8f },
        { "Midnight Saw", -7.4f },
        { "Electric Glow", -9.9f },
        { "Rust Drone", -8.9f },
        { "Copper Stab", -8.6f },
        { "Dusty Engine", -7.5f },
        { "Midnight Keys", -10.5f },
        { "Electric Circuit", -4.3f },
        { "Rust Saw", -8.5f },
        { "Copper Glow", -11.4f },
        { "Analog Stab", -11.2f },
        { "Royal Current", -6.4f },
        { "Golden Wave", -7.2f },
        { "Amber Drift", -12.3f },
        { "Bronze Pulse", -7.9f },
        { "Warm Motion", -6.4f },
        { "Smoky Brass", -7.4f },
        { "Cobalt Voltage", -13.0f },
        { "Solar Poly", -5.1f },
        { "Faded Signal", -11.0f },
        { "Silver Ladder", -9.6f },
        { "Velvet Sweep", -9.7f },
        { "Tubular Chimes", -6.9f },
        { "Digital Strings FM", -3.5f },
        { "FM Clav Funk", -6.9f },
        { "Feedback Lead", -3.0f },
        { "Metal Pad FM", -4.0f },
        { "FM Harp", -6.6f },
        { "FM Drawbars", -2.5f },
        { "FM Mallets", -1.6f },
        { "FM Brass Section", -4.9f },
        { "Solid FM Bass", 2.3f },
        { "Glass Bell FM", -5.4f },
        { "Tine Bloom EP", -4.5f },
        { "Ocean Drive Poly", -13.3f },
        { "Ladder Saw Stack", -9.5f },
        { "PWM Velvet", -12.5f },
        { "Five Voice Brass", -9.2f },
        { "Diode Growler", -6.8f },
        { "Glass Harmonics", -12.9f },
        { "FM Tine Machine", -8.9f },
        { "Spectral Bloom", -13.9f },
        { "Bitcrush Arcade", -1.2f },
        { "Comb Morph Keys", -6.9f },
        { "Ladder Mono Bass", -10.8f },
        { "Acid Squelch Line", -4.3f },
        { "Reese Tide", -11.9f },
        { "FM Pluck Bass", -6.5f },
        { "Neon Wobble", -7.3f },
        { "Sunset Boulevard Pad", -14.0f },
        { "Crystal Sky", -3.6f },
        { "Vowel Drift", -17.0f },
        { "Angel Choir Pad", -7.1f },
        { "Velvet Strings Pad", -6.9f },
        { "Miami Hero Lead", -9.3f },
        { "Square Runner", -8.8f },
        { "Sync Screamer", -6.2f },
        { "FM Whistle", -5.1f },
        { "Sallen Scream", -5.8f },
        { "Tine Electric Piano", -5.1f },
        { "Felt Upright", -1.6f },
        { "Digital EP", -8.6f },
        { "Funky Clav", -11.1f },
        { "Club Organ", -1.7f },
        { "Palm Pluck", -6.2f },
        { "Tine Drops", -5.2f },
        { "Kalimba Sun", -3.0f },
        { "Nylon Guitar", -2.3f },
        { "Glass Pizz", -8.1f },
        { "Ensemble Section", -5.5f },
        { "String Machine Night", -12.3f },
        { "Spectral Violins", -11.6f },
        { "Staccato Strings", -0.4f },
        { "Cinema Swell", -12.7f },
        { "Cathedral Aah", -6.4f },
        { "Soft Ooh", -6.4f },
        { "Synth Vox", -9.9f },
        { "Breath Choir", -6.7f },
        { "Dark Monks", -5.2f },
        { "Brass Section", -1.5f },
        { "Synth Brass Stab", -7.5f },
        { "Wavetable Horns", -5.1f },
        { "Rain City Brass", -11.9f },
        { "Horn Swell", -2.1f },
        { "FM Glass Bells", -8.8f },
        { "Crystal Mallets", -3.7f },
        { "Singing Bowl", -2.9f },
        { "Tubular Night", -8.5f },
        { "Music Box", -2.5f },
        { "Granular Fog", 1.3f },
        { "Breathing Room", -3.3f },
        { "Spectral Tides", -10.1f },
        { "Lo-Fi Memory", -14.5f },
        { "Neon Rain", 2.0f },
        { "Deep Space Drone", -15.3f },
        { "Trailer Impact", -10.2f },
        { "Tension Riser", -9.8f },
        { "Heroic Swell", -10.5f },
        { "Vice City Dusk", -15.5f },
        { "Laser Zap", -3.8f },
        { "Police Siren", -9.8f },
        { "White Noise Sweep", 5.2f },
        { "Robot Talk", -10.7f },
        { "Arcade Coin", -5.9f },
        { "Night Drive Sequence", -4.7f },
        { "Arp Lights", -6.1f },
        { "Bassline Engine", -0.9f },
        { "Kalimba Arp", -2.6f },
        { "Gated Chord Seq", -12.1f },
        { "Eighties Poly Brass", -7.9f },
        { "Crystal EP 85", -8.1f },
        { "Synthwave Lead", -8.7f },
        { "Pop Synth Pad 84", -13.4f },
        { "Gated Drum Synth", -7.8f },
        { "Nineties Digital Pad", -4.8f },
        { "Rave Hoover", -12.1f },
        { "House Organ Bass", -0.2f },
        { "Trance Supersaw", -11.0f },
        { "Pop Bell Pad 92", -5.4f },
        };
        auto it = trims.find (name);
        return it != trims.end() ? it->second : -10.5f; // uncalibrated: the Preset Volume default
    }

    void reverb (PatchSpec& s, float mix, float size, float decay) { fx (s, FxReverb, mix, { { "size", size }, { "decay", decay } }); }

    std::vector<PatchSpec> build()
    {
        Lib L;

        // ---------------- Analog -------------------------------------------------------
        L.add ("Ocean Drive Poly", "Analog", { "poly", "warm", "ota-poly" }, "Classic warm poly: detuned saws through the JN OTA filter with dimension chorus.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 0); env (s, 0, 0.02f, 1.2f, 0.75f, 0.6f); s.p ("flt1_cutoff", 3200.0f).p ("flt1_env_amount", 0.18f); });
        L.add ("Ladder Saw Stack", "Analog", { "saw", "ladder", "fat" }, "Three detuned analog saws into a driven transistor ladder.", [] (PatchSpec& s)
        { analog (s, 0, 0.67f, 0.7f); unison (s, 0, 3, 0.2f, 0.6f); analog (s, 1, 0.67f, 0.5f); pitch (s, 1, -1); filter (s, 0, MG, LP24, 1500.0f, 0.3f, 0.5f, 0.3f); env (s, 1, 0.005f, 0.8f, 0.4f, 0.4f); env (s, 0, 0.005f, 0.5f, 0.8f, 0.3f); });
        L.add ("PWM Velvet", "Analog", { "pwm", "soft", "pad" }, "Slow PWM with a smooth SEM low-pass and long release.", [] (PatchSpec& s)
        { analog (s, 0, 1.0f, 0.75f, 0.4f); unison (s, 0, 2, 0.08f, 0.7f); lfo (s, 0, 1, 0.35f); s.addMod (modsrc::Lfo1, "osc_a_pulse_width", 0.25f); filter (s, 0, SEM, LP24, 2400.0f, 0.2f); env (s, 0, 0.3f, 1.0f, 0.8f, 1.2f); fx (s, FxChorus, 0.4f); reverb (s, 0.3f, 0.7f, 3.0f); });
        L.add ("Five Voice Brass", "Analog", { "brass", "poly-brass", "poly" }, "Punchy poly-brass in the spirit of late-70s five-voice synths (P5 cascade filter).", [] (PatchSpec& s)
        { archetype (s, A::Brass, 1); s.p ("flt1_model", (float) P5).p ("flt1_cutoff", 1000.0f); });
        L.add ("Diode Growler", "Analog", { "diode", "aggressive", "mono" }, "Square + saw through a resonant diode ladder with glide.", [] (PatchSpec& s)
        { archetype (s, A::Bass, 0); s.p ("flt1_model", (float) DIODE).p ("flt1_resonance", 0.6f).p ("flt1_cutoff", 500.0f); pitch (s, 0, 1); pitch (s, 1, 0); });

        // ---------------- Digital ------------------------------------------------------
        L.add ("Glass Harmonics", "Digital", { "wavetable", "bright", "glass" }, "Glass wavetable scanned by a slow LFO with stereo delay.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 3); env (s, 0, 0.05f, 2.0f, 0.6f, 2.0f); });
        L.add ("FM Tine Machine", "Digital", { "fm", "keys", "digital" }, "Two-operator style FM tine with velocity brightness.", [] (PatchSpec& s)
        { archetype (s, A::Keys, 2); s.addMod (modsrc::Velocity, "osc_a_position", 0.3f); });
        L.add ("Spectral Bloom", "Digital", { "spectral", "evolving", "additive" }, "Additive resynthesis of the Soft Pad table with moving tilt and formant.", [] (PatchSpec& s)
        { spectral (s, 0, "Soft Pad", false, 0.75f); unison (s, 0, 3, 0.1f, 0.8f); lfo (s, 0, 0, 0.09f); s.addMod (modsrc::Lfo1, "osc_a_spec_tilt", 0.5f); lfo (s, 1, 6, 0.13f); s.addMod (modsrc::Lfo2, "osc_a_spec_formant", 0.2f); filterOff (s, 0); env (s, 0, 0.9f, 1.0f, 0.9f, 2.0f); reverb (s, 0.4f, 0.85f, 6.0f); });
        L.add ("Bitcrush Arcade", "Digital", { "chip", "8-bit", "lead" }, "Stepped chip waveforms with a quantise warp.", [] (PatchSpec& s)
        { wavetable (s, 0, "Chip Pulse", 0.2f, 0.7f); warp (s, 0, 7, 0.3f); filterOff (s, 0); voice (s, 1, 0.03f, 1); env (s, 0, 0.001f, 0.3f, 0.7f, 0.1f); fx (s, FxDigitalDelay, 0.25f, { { "crush", 0.5f } }); });
        L.add ("Comb Morph Keys", "Digital", { "comb", "wavetable", "keys" }, "Comb-sweep wavetable with envelope-driven position.", [] (PatchSpec& s)
        { wavetable (s, 0, "Comb Sweep", 0.1f, 0.75f); env (s, 2, 0.0f, 0.9f, 0.2f, 0.6f); s.addMod (modsrc::Env3, "osc_a_position", 0.6f); filter (s, 0, SVF, LP12, 7000.0f, 0.1f); env (s, 0, 0.002f, 1.5f, 0.3f, 0.6f); reverb (s, 0.2f, 0.6f, 2.0f); });

        // ---------------- FM (original six-operator voices, see docs/FM_ENGINE.md) ----------------
        auto fmVoice = [] (PatchSpec& s, int index, float level = 0.8f)
        {
            const auto& v = fm::factoryVoices()[(size_t) index];
            s.p ("osc_a_mode", 6.0f).p ("osc_a_level", level);
            s.fmVoice[0] = v.toHex();
            filterOff (s, 0);
            filterOff (s, 1);
            env (s, 0, 0.001f, 0.5f, 1.0f, fm::releaseSeconds (v));
        };
        L.add ("Tine Bloom EP", "FM", { "fm", "ep", "keys" }, "Six-operator tine piano: three operator pairs, velocity-bright tines, light chorus.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 0); fx (s, FxChorus, 0.3f); reverb (s, 0.18f, 0.5f, 1.8f); s.p ("fx_reverb_program", 1.0f); });
        L.add ("Glass Bell FM", "FM", { "fm", "bell", "glass" }, "Inharmonic 3.5-ratio modulators over sine carriers, long natural decay.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 1); reverb (s, 0.3f, 0.75f, 4.0f); });
        L.add ("Solid FM Bass", "FM", { "fm", "bass", "punchy" }, "One carrier with a stacked modulator chain and feedback.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 2, 0.85f); voice (s, 1, 0.0f, 1); });
        L.add ("FM Brass Section", "FM", { "fm", "brass", "section" }, "One modulator shared by four carriers; slow attack, detuned carriers.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 3); reverb (s, 0.22f, 0.6f, 2.4f); });
        L.add ("FM Mallets", "FM", { "fm", "mallet", "percussive" }, "Woody mallet: fast-decaying high-ratio modulators.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 4); reverb (s, 0.2f, 0.5f, 1.6f); s.p ("fx_reverb_program", 2.0f); });
        L.add ("FM Drawbars", "FM", { "fm", "organ", "drawbar" }, "Six sine carriers at drawbar-like ratios with a touch of feedback.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 5, 0.7f); fx (s, FxChorus, 0.35f); reverb (s, 0.15f, 0.5f, 1.5f); s.p ("fx_reverb_program", 3.0f); });
        L.add ("FM Harp", "FM", { "fm", "harp", "pluck" }, "Plucked harp: two operator stacks with short modulator envelopes.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 6); reverb (s, 0.25f, 0.7f, 2.8f); });
        L.add ("Metal Pad FM", "FM", { "fm", "pad", "metallic" }, "Slow-moving metallic pad from a six-operator stack.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 7); reverb (s, 0.35f, 0.85f, 6.0f); s.p ("fx_reverb_program", 4.0f); });
        L.add ("Feedback Lead", "FM", { "fm", "lead", "bright" }, "Saw-like lead from maximum operator feedback.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 8, 0.7f); fx (s, FxStereoDelay, 0.2f, { { "feedback", 0.3f } }); });
        L.add ("FM Clav Funk", "FM", { "fm", "clav", "funk" }, "Percussive clav-like keys: odd-ratio modulator chain.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 9, 1.0f); fx (s, FxPhaser, 0.25f); });
        L.add ("Digital Strings FM", "FM", { "fm", "strings", "ensemble" }, "Slow-attack digital strings with gentle modulation and ensemble chorus.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 10); fx (s, FxChorus, 0.4f); reverb (s, 0.3f, 0.8f, 3.5f); });
        L.add ("Tubular Chimes", "FM", { "fm", "chime", "bell" }, "Inharmonic chime with long decay in a large hall.", [fmVoice] (PatchSpec& s)
        { fmVoice (s, 11); reverb (s, 0.32f, 0.9f, 5.0f); });

        // ---------------- Bass ---------------------------------------------------------
        L.add ("Ladder Mono Bass", "Bass", { "ladder", "mono", "classic" }, "Saw + square + sub through a driven transistor ladder with filter envelope.", [] (PatchSpec& s) { archetype (s, A::Bass, 0); });
        L.add ("Acid Squelch Line", "Bass", { "acid", "resonant", "squelch" }, "Squelchy acid bass: Acid Diode filter with high resonance, accent via velocity.", [] (PatchSpec& s) { archetype (s, A::Bass, 1); });
        L.add ("Reese Tide", "Bass", { "reese", "dnb", "detuned" }, "Detuned stacked saws with slow filter motion.", [] (PatchSpec& s) { archetype (s, A::Bass, 2); });
        L.add ("FM Pluck Bass", "Bass", { "fm", "digital", "punchy" }, "Digital FM bass with envelope-swept index.", [] (PatchSpec& s) { archetype (s, A::Bass, 3); });
        L.add ("Neon Wobble", "Bass", { "wobble", "dubstep", "synced" }, "Tempo-synced growl wobble with MS filter and distortion.", [] (PatchSpec& s) { archetype (s, A::Bass, 5); });

        // ---------------- Pads ----------------------------------------------------------
        L.add ("Sunset Boulevard Pad", "Pads", { "warm", "lush", "analog" }, "Wide analog saw pad with dimension chorus and hall reverb.", [] (PatchSpec& s) { archetype (s, A::Pad, 0); });
        L.add ("Crystal Sky", "Pads", { "digital", "shimmer", "bright" }, "Harmonic-sweep wavetable pad with glass layer and long reverb.", [] (PatchSpec& s) { archetype (s, A::Pad, 1); });
        L.add ("Vowel Drift", "Pads", { "evolving", "formant", "spectral" }, "Formant wavetable and spectral layer drifting through vowels.", [] (PatchSpec& s) { archetype (s, A::Pad, 2); });
        L.add ("Angel Choir Pad", "Pads", { "choir", "vocal", "airy" }, "Built-in choir multisample blended with a vocal wavetable.", [] (PatchSpec& s) { archetype (s, A::Pad, 4); });
        L.add ("Velvet Strings Pad", "Pads", { "strings", "soft", "cinematic" }, "Ensemble strings layered with soft analog saws.", [] (PatchSpec& s) { archetype (s, A::Pad, 5); });

        // ---------------- Leads -----------------------------------------------------------
        L.add ("Miami Hero Lead", "Leads", { "saw", "classic", "glide" }, "Detuned saw lead with ladder filter, glide and mod-wheel vibrato.", [] (PatchSpec& s) { archetype (s, A::Lead, 0); });
        L.add ("Square Runner", "Leads", { "pwm", "square", "retro" }, "PWM square lead with an IR3109-style OTA filter.", [] (PatchSpec& s) { archetype (s, A::Lead, 1); });
        L.add ("Sync Screamer", "Leads", { "sync", "aggressive", "rock" }, "Sweeping hard-sync wavetable with envelope and drive.", [] (PatchSpec& s) { archetype (s, A::Lead, 2); });
        L.add ("FM Whistle", "Leads", { "fm", "thin", "expressive" }, "Through-zero FM between oscillators for a whistling digital lead.", [] (PatchSpec& s) { archetype (s, A::Lead, 3); });
        L.add ("Sallen Scream", "Leads", { "sallen-key", "resonant", "dirty" }, "Saw into a screaming Sallen-Key filter with nonlinear feedback.", [] (PatchSpec& s) { archetype (s, A::Lead, 4); s.p ("flt1_drive", 0.35f).p ("flt1_cutoff", 2600.0f); });

        // ---------------- Keys --------------------------------------------------------------
        L.add ("Tine Electric Piano", "Keys", { "ep", "tine", "chorus" }, "Built-in tine EP multisample with auto-pan and chorus.", [] (PatchSpec& s) { archetype (s, A::Keys, 0); });
        L.add ("Felt Upright", "Keys", { "piano", "felt", "intimate" }, "Soft modal-synthesis felt piano.", [] (PatchSpec& s) { archetype (s, A::Keys, 1); });
        L.add ("Digital EP", "Keys", { "fm", "ep", "80s" }, "FM-style electric piano with decaying modulation index.", [] (PatchSpec& s) { archetype (s, A::Keys, 2); });
        L.add ("Funky Clav", "Keys", { "clav", "funk", "pulse" }, "Narrow pulse through an enveloped ladder plus phaser.", [] (PatchSpec& s) { archetype (s, A::Keys, 3); });
        L.add ("Club Organ", "Keys", { "organ", "house", "drawbar" }, "Drawbar organ with rotary-style chorus and tube drive.", [] (PatchSpec& s) { archetype (s, A::Organ, 0); });

        // ---------------- Plucks -------------------------------------------------------------
        L.add ("Palm Pluck", "Plucks", { "analog", "pluck", "delay" }, "Snappy analog pluck with tempo delay.", [] (PatchSpec& s) { archetype (s, A::Pluck, 0); });
        L.add ("Tine Drops", "Plucks", { "digital", "bell", "pluck" }, "Electric-tine wavetable pluck.", [] (PatchSpec& s) { archetype (s, A::Pluck, 1); });
        L.add ("Kalimba Sun", "Plucks", { "kalimba", "sample", "organic" }, "Modal kalimba sample with a sine overtone.", [] (PatchSpec& s) { archetype (s, A::Pluck, 2); });
        L.add ("Nylon Guitar", "Plucks", { "guitar", "nylon", "physical" }, "Karplus-Strong nylon pluck multisample.", [] (PatchSpec& s) { archetype (s, A::Pluck, 3); s.p ("osc_a_level", 1.0f); });
        L.add ("Glass Pizz", "Plucks", { "glass", "pizzicato", "short" }, "Glass wavetable with a fast filter envelope.", [] (PatchSpec& s)
        { archetype (s, A::Pluck, 0); wavetable (s, 0, "Glass", 0.3f, 0.75f); s.p ("osc_b_on", 0.0f).p ("flt1_model", (float) SVF).p ("flt1_cutoff", 800.0f); });

        // ---------------- Strings -----------------------------------------------------------
        L.add ("Ensemble Section", "Strings", { "ensemble", "orchestral", "sample" }, "Built-in ensemble strings with vibrato on the mod wheel.", [] (PatchSpec& s) { archetype (s, A::Strings, 0); });
        L.add ("String Machine Night", "Strings", { "string machine", "analog", "vintage" }, "String-machine style saws with deep dimension chorus.", [] (PatchSpec& s) { archetype (s, A::Strings, 1); });
        L.add ("Spectral Violins", "Strings", { "spectral", "additive", "expressive" }, "Additive strings resynthesised from a bowed spectrum.", [] (PatchSpec& s) { archetype (s, A::Strings, 2); });
        L.add ("Staccato Strings", "Strings", { "short", "ensemble", "rhythmic" }, "Short ensemble articulation for rhythmic parts.", [] (PatchSpec& s)
        { archetype (s, A::Strings, 0); s.p ("osc_a_position", 0.09f).p ("osc_a_level", 1.0f); env (s, 0, 0.004f, 0.6f, 0.12f, 0.15f); });
        L.add ("Cinema Swell", "Strings", { "swell", "slow", "film" }, "Slow-attack strings with filter swell.", [] (PatchSpec& s)
        { archetype (s, A::Strings, 1); env (s, 0, 1.5f, 1.0f, 1.0f, 2.5f); env (s, 1, 2.0f, 1.0f, 0.8f, 2.0f); s.p ("flt1_env_amount", 0.25f).p ("flt1_cutoff", 1500.0f); });

        // ---------------- Choirs --------------------------------------------------------------
        L.add ("Cathedral Aah", "Choirs", { "choir", "aah", "sacred" }, "Formant-synthesised choir 'aah' with cathedral reverb.", [] (PatchSpec& s) { archetype (s, A::Choir, 0); });
        L.add ("Soft Ooh", "Choirs", { "choir", "ooh", "soft" }, "Rounded 'ooh' choir.", [] (PatchSpec& s) { archetype (s, A::Choir, 1); });
        L.add ("Synth Vox", "Choirs", { "vocal", "wavetable", "moving" }, "Vowel-morphing synthetic voices.", [] (PatchSpec& s) { archetype (s, A::Choir, 2); });
        L.add ("Breath Choir", "Choirs", { "breathy", "air", "layer" }, "Choir with a breath-noise layer.", [] (PatchSpec& s)
        { archetype (s, A::Choir, 0); sample (s, 2, "Breath Pad", 0.3f, 60, 1); });
        L.add ("Dark Monks", "Choirs", { "low", "dark", "ritual" }, "Low male-choir register with dark filter.", [] (PatchSpec& s)
        { archetype (s, A::Choir, 1); pitch (s, 0, -1); s.p ("flt1_cutoff", 1800.0f); });

        // ---------------- Brass ---------------------------------------------------------------
        L.add ("Brass Section", "Brass", { "brass", "section", "sample" }, "Built-in brass section multisample.", [] (PatchSpec& s) { archetype (s, A::Brass, 0); });
        L.add ("Synth Brass Stab", "Brass", { "stab", "analog", "80s" }, "Bright analog brass stab with filter attack.", [] (PatchSpec& s) { archetype (s, A::Brass, 1); });
        L.add ("Wavetable Horns", "Brass", { "wavetable", "horns", "swell" }, "Brass wavetable with enveloped brightness.", [] (PatchSpec& s) { archetype (s, A::Brass, 2); });
        L.add ("Rain City Brass", "Brass", { "poly-brass", "cinematic", "expressive" }, "Expressive poly brass with aftertouch-controlled brightness and vibrato.", [] (PatchSpec& s)
        { archetype (s, A::Brass, 1); env (s, 0, 0.12f, 1.0f, 0.9f, 1.5f); s.addMod (modsrc::Aftertouch, "flt1_cutoff", 0.3f); reverb (s, 0.45f, 0.95f, 6.0f); });
        L.add ("Horn Swell", "Brass", { "soft", "swell", "pad" }, "Soft swelling horn pad.", [] (PatchSpec& s)
        { archetype (s, A::Brass, 0); env (s, 0, 0.6f, 1.0f, 0.9f, 1.0f); });

        // ---------------- Bells ---------------------------------------------------------------
        L.add ("FM Glass Bells", "Bells", { "fm", "bell", "glass" }, "FM ratio wavetable with stretched spectral partials.", [] (PatchSpec& s) { archetype (s, A::Bell, 0); });
        L.add ("Crystal Mallets", "Bells", { "mallet", "modal", "sample" }, "Modal glass mallet multisample.", [] (PatchSpec& s) { archetype (s, A::Bell, 1); });
        L.add ("Singing Bowl", "Bells", { "bowl", "spectral", "meditation" }, "Spectral resynthesis of a singing bowl.", [] (PatchSpec& s) { archetype (s, A::Bell, 2); });
        L.add ("Tubular Night", "Bells", { "tubular", "inharmonic", "dark" }, "Inharmonic stretched bell with low-pass.", [] (PatchSpec& s)
        { archetype (s, A::Bell, 0); s.p ("osc_b_spec_stretch", 0.6f); filter (s, 0, SVF, LP12, 3500.0f, 0.1f); });
        L.add ("Music Box", "Bells", { "music box", "toy", "high" }, "Tiny music-box tines an octave up.", [] (PatchSpec& s)
        { archetype (s, A::Bell, 1); pitch (s, 0, 1); env (s, 0, 0.001f, 1.2f, 0.0f, 0.8f); });

        // ---------------- Textures -----------------------------------------------------------
        L.add ("Granular Fog", "Textures", { "granular", "cloud", "ambient" }, "Granular cloud scanning an original texture.", [] (PatchSpec& s) { archetype (s, A::Texture, 0); });
        L.add ("Breathing Room", "Textures", { "breath", "air", "granular" }, "Breath sample layered with granular breath.", [] (PatchSpec& s) { archetype (s, A::Texture, 1); });
        L.add ("Spectral Tides", "Textures", { "spectral", "scan", "drone" }, "Slow spectral scan of an analog drone.", [] (PatchSpec& s) { archetype (s, A::Texture, 2); });
        L.add ("Lo-Fi Memory", "Textures", { "vinyl", "lofi", "nostalgic" }, "Vinyl texture under a soft wavetable pad.", [] (PatchSpec& s) { archetype (s, A::Texture, 3); });
        L.add ("Neon Rain", "Textures", { "rain", "noise", "granular" }, "Sparse high grains over filtered noise.", [] (PatchSpec& s)
        { archetype (s, A::Texture, 0); pitch (s, 0, 1); s.p ("osc_a_grain_size", 40.0f).p ("osc_a_grain_density", 60.0f); noise (s, 0.15f, 1, 0.3f); });

        // ---------------- Cinematic -----------------------------------------------------------
        L.add ("Deep Space Drone", "Cinematic", { "drone", "dark", "evolving" }, "Low detuned saws and granular drone through a slow SEM filter.", [] (PatchSpec& s) { archetype (s, A::Drone, 0); });
        L.add ("Trailer Impact", "Cinematic", { "impact", "hit", "boom" }, "Sub drop, noise burst and distortion for trailer hits.", [] (PatchSpec& s) { archetype (s, A::Sfx, 2); });
        L.add ("Tension Riser", "Cinematic", { "riser", "build", "noise" }, "Six-second rising sweep.", [] (PatchSpec& s) { archetype (s, A::Sfx, 0); });
        L.add ("Heroic Swell", "Cinematic", { "brass", "strings", "epic" }, "Layered strings and brass swell.", [] (PatchSpec& s)
        { archetype (s, A::Strings, 0); multisample (s, 1, "Brass Section", 0.5f); env (s, 0, 0.8f, 1.0f, 1.0f, 2.0f); reverb (s, 0.4f, 0.9f, 5.0f); });
        L.add ("Vice City Dusk", "Cinematic", { "pad", "nostalgic", "wide" }, "Wide nostalgic pad with tape delay and long hall.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 0); fx (s, FxTapeDelay, 0.25f); reverb (s, 0.5f, 0.95f, 7.0f); });

        // ---------------- Sound Effects ---------------------------------------------------------
        L.add ("Laser Zap", "Sound Effects", { "laser", "zap", "pitch" }, "Fast downward pitch sweep.", [] (PatchSpec& s) { archetype (s, A::Sfx, 1); });
        L.add ("Police Siren", "Sound Effects", { "siren", "lfo", "alarm" }, "Square-wave siren with band-pass filter.", [] (PatchSpec& s) { archetype (s, A::Sfx, 3); });
        L.add ("White Noise Sweep", "Sound Effects", { "noise", "sweep", "transition" }, "Filtered noise sweep for transitions.", [] (PatchSpec& s)
        { s.p ("osc_a_on", 0.0f); noise (s, 1.0f, 1, 0.0f); filter (s, 0, SVF, BP, 300.0f, 0.08f); lfo (s, 0, 2, 0.25f, false, 10, 2); s.addMod (modsrc::Lfo1, "flt1_cutoff", 0.7f); env (s, 0, 0.05f, 0.1f, 1.0f, 1.0f); reverb (s, 0.3f, 0.7f, 3.0f); });
        L.add ("Robot Talk", "Sound Effects", { "robot", "formant", "ring" }, "Ring-modulated formant wavetable.", [] (PatchSpec& s)
        { wavetable (s, 0, "Formant Vowels", 0.0f, 0.8f); lfo (s, 0, 5, 8.0f); s.addMod (modsrc::Lfo1, "osc_a_position", 0.8f, 2); fx (s, FxRingMod, 0.6f, { { "freq", 120.0f } }); filterOff (s, 0); });
        L.add ("Arcade Coin", "Sound Effects", { "chip", "game", "blip" }, "Two-step chip blip.", [] (PatchSpec& s)
        { wavetable (s, 0, "Chip Pulse", 0.0f, 0.7f); pitchEnv (s, 2, 1, -12.0f, 0.08f); s.p ("env3_sustain", 0.0f); env (s, 0, 0.001f, 0.25f, 0.0f, 0.1f); filterOff (s, 0); });

        // ---------------- Sequences -----------------------------------------------------------
        L.add ("Night Drive Sequence", "Sequences", { "sequencer", "pluck", "driving" }, "16-step sequence with analog pluck and synced delay (hold a key).", [] (PatchSpec& s) { archetype (s, A::Sequence, 0); });
        L.add ("Arp Lights", "Sequences", { "arp", "up-down", "digital" }, "Two-octave up-down arpeggio with tine pluck.", [] (PatchSpec& s) { archetype (s, A::Arp, 1); });
        L.add ("Bassline Engine", "Sequences", { "sequencer", "bass", "acid" }, "Acid bass driven by the step sequencer.", [] (PatchSpec& s)
        { archetype (s, A::Bass, 1); s.p ("arp_mode", 2.0f).p ("seq_rate", 4.0f); s.p ("voice_mode", 0.0f).p ("polyphony", 4.0f); });
        L.add ("Kalimba Arp", "Sequences", { "arp", "kalimba", "random" }, "Random-order kalimba arpeggio.", [] (PatchSpec& s)
        { archetype (s, A::Arp, 2); s.p ("arp_pattern", 5.0f); });
        L.add ("Gated Chord Seq", "Sequences", { "chord", "gated", "trance" }, "Gated chord pattern from the sequencer.", [] (PatchSpec& s)
        {
            archetype (s, A::Sequence, 0);
            auto& st = s.seq.steps;
            for (int i = 0; i < 16; ++i) { st[(size_t) i].on = i % 4 != 3; st[(size_t) i].notes = { 0, 3, 7, 12 }; st[(size_t) i].gate = 0.4f; }
        });

        // ---------------- Vintage 1980s -------------------------------------------------------
        L.add ("Eighties Poly Brass", "Vintage 1980s", { "80s", "brass", "ota" }, "Big 80s poly brass with JP OTA filter swell.", [] (PatchSpec& s) { archetype (s, A::Brass, 1); unison (s, 0, 3, 0.15f); });
        L.add ("Crystal EP 85", "Vintage 1980s", { "80s", "fm", "ep" }, "Mid-80s digital FM electric piano with chorus.", [] (PatchSpec& s) { archetype (s, A::Keys, 2); fx (s, FxChorus, 0.45f); });
        L.add ("Synthwave Lead", "Vintage 1980s", { "synthwave", "lead", "saw" }, "Saw lead with chorus, delay and gated hall.", [] (PatchSpec& s)
        { archetype (s, A::Lead, 0); fx (s, FxChorus, 0.3f); reverb (s, 0.3f, 0.75f, 2.0f); });
        L.add ("Pop Synth Pad 84", "Vintage 1980s", { "80s", "pad", "pop" }, "Bright detuned pad with dimension chorus typical of 80s pop.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 0); s.p ("flt1_cutoff", 4500.0f).p ("env1_attack", 0.15f); });
        L.add ("Gated Drum Synth", "Vintage 1980s", { "80s", "tom", "synth drum" }, "Pitch-swept analog tom with gated reverb.", [] (PatchSpec& s)
        { analog (s, 0, 0.0f, 0.9f); pitchEnv (s, 2, 1, 12.0f, 0.25f); noise (s, 0.2f, 1, 0.0f); env (s, 0, 0.001f, 0.4f, 0.0f, 0.3f); filterOff (s, 0); reverb (s, 0.4f, 0.4f, 0.6f); });

        // ---------------- Vintage 1990s -------------------------------------------------------
        L.add ("Nineties Digital Pad", "Vintage 1990s", { "90s", "digital", "jd" }, "Bright early-90s digital pad: harmonic wavetable, glass layer, chorus and big reverb.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 1); fx (s, FxChorus, 0.45f); reverb (s, 0.45f, 0.9f, 5.0f); });
        L.add ("Rave Hoover", "Vintage 1990s", { "hoover", "rave", "90s" }, "Detuned PWM 'hoover' with pitch scoop and chorus.", [] (PatchSpec& s)
        { analog (s, 0, 1.0f, 0.7f, 0.3f); unison (s, 0, 6, 0.45f, 0.9f); analog (s, 1, 0.67f, 0.5f); pitch (s, 1, -1); unison (s, 1, 4, 0.4f, 0.9f); pitchEnv (s, 2, 3, -5.0f, 0.25f); filter (s, 0, JN, LP24, 4000.0f, 0.1f); voice (s, 2, 0.12f, 1); fx (s, FxChorus, 0.5f); });
        L.add ("House Organ Bass", "Vintage 1990s", { "house", "organ", "90s" }, "Classic house organ stab with short decay.", [] (PatchSpec& s) { archetype (s, A::Organ, 1); });
        L.add ("Trance Supersaw", "Vintage 1990s", { "trance", "supersaw", "anthem" }, "Seven-voice detuned saw stack with stereo delay.", [] (PatchSpec& s)
        { wavetable (s, 0, "Basic Shapes", 0.66f, 0.7f); unison (s, 0, 7, 0.35f, 1.0f, 0.8f); wavetable (s, 1, "Basic Shapes", 0.66f, 0.4f); pitch (s, 1, 1); unison (s, 1, 5, 0.3f, 1.0f); filter (s, 0, SVF, LP12, 9000.0f, 0.1f); env (s, 0, 0.005f, 0.5f, 0.8f, 0.4f); fx (s, FxStereoDelay, 0.25f); reverb (s, 0.3f, 0.8f, 3.0f); });
        L.add ("Pop Bell Pad 92", "Vintage 1990s", { "90s", "bell", "pad" }, "Bell attack over a digital pad, a classic early-90s layer.", [] (PatchSpec& s)
        { archetype (s, A::Pad, 1); wavetable (s, 2, "FM Ratios", 0.7f, 0.35f); env (s, 0, 0.01f, 2.0f, 0.7f, 1.8f); });

        // expansion: curated voicing x treatment combinations and original FM voices (FactoryExpansion.cpp)
        std::set<juce::String> taken;
        for (const auto& p : L.v) taken.insert (p.meta.name);
        for (auto p : FactoryExpansion::build())
        {
            if (taken.count (p.meta.name) != 0) p.meta.name << " II";
            taken.insert (p.meta.name);
            p.set ("master_volume", levelTrim (p.meta.name));
            L.v.push_back (std::move (p));
        }
        return L.v;
    }
}

const std::vector<PatchSpec>& all()
{
    static const std::vector<PatchSpec> presets = build();
    return presets;
}

} // namespace vsx::FactoryPresets
