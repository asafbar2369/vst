#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>

class ConsoleTestRunner final : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    ConsoleTestRunner runner;
    runner.setAssertOnFailure (false);
    // VSX_TEST_ONLY=<text> runs only the test groups whose name contains <text> (case-insensitive)
    const auto only = juce::SystemStats::getEnvironmentVariable ("VSX_TEST_ONLY", {});
    if (only.isEmpty())
        runner.runAllTests (0x565358);
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
