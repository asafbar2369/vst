#include "../Source/PluginProcessor.h"
#include "../Source/Presets/PresetManager.h"

namespace vsx
{
/**
    Opt-in benchmark / profiling harness (runs only when VSX_BENCH is set):
      VSX_BENCH=1 VSX_BENCH_PRESET="Ocean Drive Poly" VSX_BENCH_SECONDS=2 VSX_BENCH_RATE=48000
    Plays a 6-note chord through the full processor and prints the realtime factor.
*/
class BenchmarkTests final : public juce::UnitTest
{
public:
    BenchmarkTests() : juce::UnitTest ("Benchmark", "VICE SYNTH X") {}

    void runTest() override
    {
        beginTest ("Processor benchmark (opt-in)");
        if (juce::SystemStats::getEnvironmentVariable ("VSX_BENCH", {}).isEmpty()) { expect (true); return; }
        const auto preset = juce::SystemStats::getEnvironmentVariable ("VSX_BENCH_PRESET", "Ocean Drive Poly");
        const double seconds = juce::SystemStats::getEnvironmentVariable ("VSX_BENCH_SECONDS", "2").getDoubleValue();
        const double sr = juce::SystemStats::getEnvironmentVariable ("VSX_BENCH_RATE", "48000").getDoubleValue();
        const int bs = 256;
        auto proc = std::make_unique<ViceSynthProcessor>();
        proc->prepareToPlay (sr, bs);
        const int idx = proc->getPresetManager().indexOfName (preset);
        if (idx >= 0) proc->loadPresetEntry (proc->getPresetManager().getEntries()[(size_t) idx]);
        juce::AudioBuffer<float> buffer (2, bs);
        juce::MidiBuffer midi;
        for (int n : { 48, 55, 60, 64, 67, 72 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        const int blocks = (int) (seconds * sr / bs);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            proc->processBlock (buffer, midi);
            midi.clear();
        }
        const double cpu = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
        logMessage ("MEASURE|bench|" + preset + " " + juce::String ((int) sr) + " Hz realtime factor|" + juce::String (seconds / cpu, 3) + "|x realtime");
        expect (cpu > 0.0);
    }
};

static BenchmarkTests benchmarkTests;
} // namespace vsx
