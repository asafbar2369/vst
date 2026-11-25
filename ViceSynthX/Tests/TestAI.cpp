#include "../Source/AI/PatchOps.h"
#include "../Source/AI/AIProvider.h"
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

        beginTest ("Oscillator names: A / B / C, OSC C, osc_c, 3 (the letter C was rejected before 2026-11-25)");
        for (const auto* name : { "C", "c", "OSC C", "osc_c", "Osc C", "3" })
        {
            const auto r = check (juce::String (R"({"reply":"t","action":"modify_patch","operations":[{"op":"wavetable","osc":")") + name
                                  + R"(","name":"Glass"}]})");
            expect (r.hasPatch && r.report.errors.isEmpty(), juce::String ("osc '") + name + "': " + r.report.errors.joinIntoString ("; "));
            if (r.hasPatch) expectEquals (r.patch.wavetableId[2], juce::String ("factory:Glass"));
        }
        for (const auto* name : { "A", "OSC B", "D", "OSC", "" })
        {
            const auto r = check (juce::String (R"({"reply":"t","action":"modify_patch","operations":[{"op":"wavetable","osc":")") + name
                                  + R"(","name":"Glass"}]})");
            const bool valid = juce::String (name) == "A" || juce::String (name) == "OSC B";
            expect (r.hasPatch == valid, juce::String ("osc '") + name + "'");
        }

        beginTest ("Repair round: rejected operations go back to the model once; the corrected answer is validated");
        {
            struct Scripted final : AIProvider
            {
                juce::StringArray replies; int calls = 0; juce::String lastUser;
                juce::String name() const override { return "scripted"; }
                juce::String selectedModel() const override { return "m"; }
                ModelListResult listModels (const juce::String&, const std::atomic<bool>&) override { return {}; }
                AIResult send (const AIRequest& req, const juce::String&, const std::atomic<bool>&) override
                {
                    AIResult r; r.ok = true; lastUser = req.userMessage;
                    r.raw = replies[juce::jmin (calls++, replies.size() - 1)];
                    r.json = juce::JSON::parse (r.raw);
                    return r;
                }
            };
            std::atomic<bool> cancel { false };
            AIRequest req; req.userMessage = "make it quieter";
            AIResult last; int repairs = -1;
            Scripted p1;
            p1.replies = { R"({"reply":"x","action":"modify_patch","operations":[{"op":"mod","source":"fx_reverb","dest":"master_volume","amount":0.2}]})",
                           R"({"reply":"fixed","action":"modify_patch","operations":[{"op":"set","param":"master_volume","value":-20}]})" };
            const auto d1 = PatchOps::designWithRepair (p1, req, {}, initial, "t", cancel, last, repairs);
            expectEquals (p1.calls, 2); expectEquals (repairs, 1);
            expect (p1.lastUser.contains ("fx_reverb") && p1.lastUser.contains ("rejected"), "the repair request carries the errors");
            expect (d1.hasPatch && d1.report.errors.isEmpty(), "corrected answer applied");
            expectWithinAbsoluteError (d1.patch.get ("master_volume"), -20.0f, 0.001f);

            Scripted p2; p2.replies = { R"({"reply":"ok","action":"modify_patch","operations":[{"op":"set","param":"master_volume","value":-18}]})" };
            const auto d2 = PatchOps::designWithRepair (p2, req, {}, initial, "t", cancel, last, repairs);
            expectEquals (p2.calls, 1, "a valid first answer is not sent back"); expectEquals (repairs, 0); expect (d2.hasPatch);

            Scripted p3; p3.replies = { R"({"reply":"x","action":"modify_patch","operations":[{"op":"set","param":"nope","value":1}]})" };
            const auto d3 = PatchOps::designWithRepair (p3, req, {}, initial, "t", cancel, last, repairs);
            expectEquals (p3.calls, 2, "exactly one repair round"); expect (! d3.hasPatch && d3.patch.values == initial.values, "still invalid: nothing applied");
        }

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
