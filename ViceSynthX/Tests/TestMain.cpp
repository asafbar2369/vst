#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests (0x565358);
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
