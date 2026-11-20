#include "../Source/AI/PatchOps.h"
#include <juce_core/juce_core.h>

namespace vsx
{
class AIResponseTests final : public juce::UnitTest
{
public:
    AIResponseTests() : juce::UnitTest ("AI response validation", "VICE SYNTH X") {}

    void runTest() override
    {
        const auto initial = PatchOps::initPatch();
        auto check = [&] (const juce::String& json)
        {
            return PatchOps::fromResponse (juce::JSON::parse (json), initial, "test");
        };

        beginTest ("Malformed or unknown action never changes the current patch");
        for (const auto* json : {
            R"({"reply":"test","action":"new_patch"})",
            R"({"reply":"test","action":"unexpected","operations":[]})",
            R"({"reply":"test","action":"modify_patch","operations":{}})",
            R"({"reply":42,"action":"modify_patch","operations":[]})" })
        {
            const auto r = check (json);
            expect (! r.hasPatch);
            expect (! r.report.errors.isEmpty());
            expect (r.patch.values == initial.values);
        }

        beginTest ("Valid response changes real parameter");
        const auto valid = check (R"({"reply":"quieter","action":"modify_patch","operations":[{"op":"set","param":"master_volume","value":-18}]})");
        expect (valid.hasPatch);
        expectEquals (valid.report.applied, 1);
        expectWithinAbsoluteError (valid.patch.get ("master_volume"), -18.0f, 0.001f);
        expect (valid.report.errors.isEmpty());

        beginTest ("A mixed valid/invalid transaction is rejected as a whole");
        const auto mixed = check (R"({"reply":"test","action":"modify_patch","operations":[{"op":"set","param":"master_volume","value":-18},{"op":"set","param":"no_such_parameter","value":1}]})");
        expect (! mixed.hasPatch);
        expectEquals (mixed.report.applied, 0);
        expect (! mixed.report.errors.isEmpty());
        expect (mixed.patch.values == initial.values);

        beginTest ("Missing parameter value cannot erase a setting");
        const auto missing = check (R"({"reply":"test","action":"new_patch","operations":[{"op":"set","param":"master_volume"}]})");
        expect (! missing.hasPatch);
        expect (! missing.report.errors.isEmpty());
        expect (missing.patch.values == initial.values);

        beginTest ("Invalid variations are not exposed as playable presets");
        const auto variation = check (R"({"reply":"test","action":"modify_patch","operations":[{"op":"set","param":"master_volume","value":-18}],"variations":[{"name":"invalid","operations":[{"op":"not_real"}]},{"name":"valid","operations":[{"op":"set","param":"master_volume","value":-12}]}]})");
        expect (variation.hasPatch);
        expectEquals ((int) variation.variations.size(), 1);
        if (! variation.variations.empty())
            expectWithinAbsoluteError (variation.variations.front().get ("master_volume"), -12.0f, 0.001f);

        beginTest ("An answer does not mutate the patch");
        const auto answer = check (R"({"reply":"hello","action":"answer_only","operations":[]})");
        expect (! answer.hasPatch);
        expect (answer.patch.values == initial.values);
    }
};

static AIResponseTests aiResponseTests;
} // namespace vsx
