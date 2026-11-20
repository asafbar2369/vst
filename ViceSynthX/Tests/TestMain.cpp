#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include "../Source/Analysis/SoundMatcher.h"

namespace
{
class ConsoleTestRunner final : public juce::UnitTestRunner
{
protected:
    void logMessage (const juce::String& message) override
    {
        // Flush every test stage in Release too; the last message survives a crash.
        std::cerr << message << std::endl;
    }
};
}

int main()
{
    std::cerr << "Starting VICE SYNTH X tests; sizeof(SynthEngine)=" << sizeof(vsx::SynthEngine)
              << ", sizeof(OfflineRenderer)=" << sizeof(vsx::OfflineRenderer) << std::endl;
    juce::ScopedJuceInitialiser_GUI initialise;
    ConsoleTestRunner runner;
    runner.setAssertOnFailure (false);
    // VSX_TEST_ONLY=<text> runs only the test groups whose name contains <text> (case-insensitive)
    const auto only = juce::SystemStats::getEnvironmentVariable ("VSX_TEST_ONLY", {});
    if (only.isEmpty())
        runner.runTestsInCategory ("VICE SYNTH X", 0x565358);
    else
    {
        juce::Array<juce::UnitTest*> selected;
        for (auto* t : juce::UnitTest::getAllTests())
            if (t->getName().containsIgnoreCase (only)) selected.add (t);
        runner.runTests (selected, 0x565358);
    }
    int passes = 0, failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (const auto* result = runner.getResult (i))
        {
            passes += result->passes;
            failures += result->failures;
        }
    }
    std::cout << "VICE SYNTH X: " << passes << " passed, "
              << failures << " failed." << std::endl;
    return passes > 0 && failures == 0 ? 0 : 1;
}
