#include "../Source/Analysis/SoundMatcher.h"
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

/*
    Sample workflow tests. All audio fixtures are generated here (pure tones); no user or third-party
    audio is used, and nothing is uploaded anywhere.
*/
namespace vsx
{
namespace
{
    juce::File writeTone (const juce::File& f, double hz, double seconds, double sr = 48000.0, int channels = 1, float amp = 0.5f)
    {
        f.deleteFile();
        juce::AudioBuffer<float> b (channels, (int) (seconds * sr));
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (ch == 1 ? 1.5 : 1.0) * i / sr));
        std::unique_ptr<juce::AudioFormat> fmt;
        if (f.hasFileExtension ("aif;aiff")) fmt = std::make_unique<juce::AiffAudioFormat>();
        else if (f.hasFileExtension ("flac")) fmt = std::make_unique<juce::FlacAudioFormat>();
        else fmt = std::make_unique<juce::WavAudioFormat>();
        auto os = std::make_unique<juce::FileOutputStream> (f);
        std::unique_ptr<juce::AudioFormatWriter> w (fmt->createWriterFor (os.get(), sr, (unsigned int) channels, 24, {}, 0));
        if (w != nullptr)
        {
            os.release();
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        return f;
    }

    float pitchOf (const juce::AudioBuffer<float>& b, double sr)
    {
        const auto f = AudioFeatures::analyse (b, sr);
        return f.valid ? f.f0 : 0.0f;
    }

    double cents (double measured, double expected) { return 1200.0 * std::log2 (std::max (1.0e-6, measured) / expected); }

    float rmsRange (const juce::AudioBuffer<float>& b, double sr, double t0, double t1)
    {
        const int a = (int) (t0 * sr), z = std::min (b.getNumSamples(), (int) (t1 * sr));
        double s = 0;
        for (int i = a; i < z; ++i) s += (double) b.getSample (0, i) * b.getSample (0, i);
        return (float) std::sqrt (s / std::max (1, z - a));
    }

    bool waitImports (ViceSynthProcessor& p, int timeoutMs = 20000)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (juce::Time::getMillisecondCounter() < end)
        {
            if (p.deliverPendingImports() == 0 && ! p.getImportStatus (0).loading && ! p.getImportStatus (1).loading
                && ! p.getImportStatus (2).loading)
                return true;
            juce::Thread::sleep (5);
        }
        return false;
    }
}

class SampleWorkflowTests final : public juce::UnitTest
{
public:
    SampleWorkflowTests() : juce::UnitTest ("Sample workflow", "VICE SYNTH X") {}

    void runTest() override
    {
        using namespace pid;
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vsx-sample-fixtures");
        dir.deleteRecursively();
        dir.createDirectory();
        const double c4 = 261.6255653;
        const auto toneC4 = writeTone (dir.getChildFile ("tone.wav"), c4, 2.0);
        const auto toneA3 = writeTone (dir.getChildFile ("named_A3.wav"), 220.0, 2.0);
        const auto sharp = writeTone (dir.getChildFile ("sharp.wav"), 440.0 * std::pow (2.0, 30.0 / 1200.0), 2.0); // A4 + 30 cents
        const auto stereo = writeTone (dir.getChildFile ("stereo.aif"), c4, 1.0, 44100.0, 2);
        const auto flac = writeTone (dir.getChildFile ("tone.flac"), c4, 1.0, 44100.0, 1);
        const auto garbage = dir.getChildFile ("garbage.wav");
        garbage.replaceWithText ("this is not audio data at all, just text pretending to be a wav file");
        auto renderer = std::make_unique<OfflineRenderer> (48000.0);

        auto proc = std::make_unique<ViceSynthProcessor>();
        proc->prepareToPlay (48000.0, 256);

        beginTest ("Parameter registry layout matches the declared indices");
        expectEquals (ParamRegistry::layoutErrors(), 0);
        expect (ParamRegistry::get()[O (0, osc::RootAuto)].id == "osc_a_root_auto" && ParamRegistry::get()[O (2, osc::SampleEnd)].id == "osc_c_sample_end");

        beginTest ("LOAD SAMPLE: one file plays chromatically across the keyboard (root C4 by default)");
        {
            bool done = false, ok = false;
            proc->importToOscillator (0, toneC4, ViceSynthProcessor::ImportKind::Auto, [&] (bool k, const juce::String&) { done = true; ok = k; });
            expect (waitImports (*proc) && done && ok, proc->getImportStatus (0).text);
            const auto spec = proc->getCurrentPatch();
            expectEquals ((int) spec.values[(size_t) O (0, osc::Mode)], 2, "oscillator switched to Sample mode");
            expect (spec.values[(size_t) O (0, osc::On)] > 0.5f);
            expect (spec.sampleId[0].endsWith ("tone.wav"));
            for (const int note : { 48, 60, 72 })
            {
                const auto out = renderer->render (spec, note, 100, 0.8, 0.1, 1);
                const double expected = c4 * std::pow (2.0, (note - 60) / 12.0);
                const double err = cents (pitchOf (out, 48000.0), expected);
                logMessage ("MEASURE|sample|pitch error MIDI " + juce::String (note) + "|" + juce::String (err, 2) + "|cents");
                expect (std::abs (err) < 10.0, "MIDI " + juce::String (note) + ": " + juce::String (err, 1) + " cents");
            }
            for (const int note : { 0, 127 })
            {
                const auto out = renderer->render (spec, note, 100, 0.5, 0.1, 1);
                float pk = out.getMagnitude (0, out.getNumSamples());
                bool finite = true;
                for (int i = 0; i < out.getNumSamples(); ++i) finite = finite && std::isfinite (out.getSample (0, i));
                expect (finite && pk > 1.0e-4f, "MIDI " + juce::String (note) + " playable");
            }
            // undo restores the previous oscillator
            expect (proc->undo());
            expectEquals ((int) proc->getCurrentPatch().values[(size_t) O (0, osc::Mode)], (int) spec.values[(size_t) O (0, osc::Mode)] == 2 ? (int) PatchOps::initPatch().values[(size_t) O (0, osc::Mode)] : 0);
            expect (proc->redo());
        }

        beginTest ("Root key, Root Auto (file name / detected pitch) and the fine-tune sign");
        {
            proc->importToOscillator (0, toneA3);
            expect (waitImports (*proc));
            auto spec = proc->getCurrentPatch();
            // default: root C4, so MIDI 60 plays the recording unchanged (220 Hz)
            const float p60 = pitchOf (renderer->render (spec, 60, 100, 0.8, 0.1, 1), 48000.0);
            expect (std::abs (cents (p60, 220.0)) < 10.0, "default root: MIDI 60 measured " + juce::String (p60, 2) + " Hz, root " + juce::String (spec.values[(size_t) O (0, osc::RootNote)]) + " auto " + juce::String (spec.values[(size_t) O (0, osc::RootAuto)]) + " id " + spec.sampleId[0]);
            spec.values[(size_t) O (0, osc::RootAuto)] = 1.0f; // file name says A3
            expect (std::abs (cents (pitchOf (renderer->render (spec, 57, 100, 0.8, 0.1, 1), 48000.0), 220.0)) < 10.0, "Root Auto from file name");
            spec.values[(size_t) O (0, osc::RootAuto)] = 0.0f;
            spec.values[(size_t) O (0, osc::RootNote)] = 69.0f; // root A4: MIDI 69 plays the recording unchanged
            expect (std::abs (cents (pitchOf (renderer->render (spec, 69, 100, 0.8, 0.1, 1), 48000.0), 220.0)) < 10.0, "Root Key control");

            proc->importToOscillator (0, sharp);
            expect (waitImports (*proc));
            spec = proc->getCurrentPatch();
            spec.values[(size_t) O (0, osc::RootAuto)] = 1.0f; // detected: A4 + 30 cents -> corrected
            const double err = cents (pitchOf (renderer->render (spec, 69, 100, 0.8, 0.1, 1), 48000.0), 440.0);
            logMessage ("MEASURE|sample|Root Auto detected-tuning correction residual|" + juce::String (err, 2) + "|cents");
            expect (std::abs (err) < 8.0, "detected tuning must be corrected, residual " + juce::String (err, 1) + " cents");
        }

        beginTest ("Loops at extreme playback rates, crossfade at loop start 0, Sample End");
        {
            proc->importToOscillator (0, toneC4);
            expect (waitImports (*proc));
            auto spec = proc->getCurrentPatch();
            spec.values[(size_t) O (0, osc::LoopStart)] = 0.5f;
            spec.values[(size_t) O (0, osc::LoopEnd)] = 0.5005f; // ~48-sample loop
            for (const int mode : { 1, 2 })
            {
                spec.values[(size_t) O (0, osc::LoopMode)] = (float) mode;
                const auto out = renderer->render (spec, 127, 100, 1.5, 0.0, 1);
                expect (rmsRange (out, 48000.0, 1.0, 1.5) > 1.0e-3f, juce::String (mode == 1 ? "forward" : "ping-pong") + " loop keeps sounding at MIDI 127");
            }
            spec.values[(size_t) O (0, osc::LoopMode)] = 1.0f;
            spec.values[(size_t) O (0, osc::LoopStart)] = 0.0f;
            spec.values[(size_t) O (0, osc::LoopEnd)] = 0.25f;
            spec.values[(size_t) O (0, osc::LoopXfade)] = 0.5f;
            const auto x = renderer->render (spec, 60, 100, 1.5, 0.0, 1);
            bool finite = true;
            for (int i = 0; i < x.getNumSamples(); ++i) finite = finite && std::isfinite (x.getSample (0, i));
            expect (finite && rmsRange (x, 48000.0, 1.0, 1.5) > 1.0e-3f, "loop from 0 with crossfade plays");

            spec.values[(size_t) O (0, osc::LoopMode)] = 0.0f;
            spec.values[(size_t) O (0, osc::SampleEnd)] = 0.25f; // 0.5 s of a 2 s file at the root key
            const auto e = renderer->render (spec, 60, 100, 1.2, 0.0, 1);
            expect (rmsRange (e, 48000.0, 0.1, 0.4) > 1.0e-2f && rmsRange (e, 48000.0, 0.7, 1.1) < 1.0e-4f, "playback stops at Sample End: "
                    + juce::String (rmsRange (e, 48000.0, 0.1, 0.4), 5) + " / " + juce::String (rmsRange (e, 48000.0, 0.7, 1.1), 6));
        }

        beginTest ("Stereo AIFF and FLAC import; invalid files keep the current sound");
        {
            proc->importToOscillator (1, stereo);
            expect (waitImports (*proc) && ! proc->getImportStatus (1).error, proc->getImportStatus (1).text);
            expect (proc->getImportStatus (1).text.contains ("stereo"));
            proc->importToOscillator (2, flac);
            expect (waitImports (*proc) && ! proc->getImportStatus (2).error, proc->getImportStatus (2).text);

            const auto before = proc->getCurrentPatch();
            bool done = false, ok = true;
            proc->importToOscillator (0, garbage, ViceSynthProcessor::ImportKind::Auto, [&] (bool k, const juce::String&) { done = true; ok = k; });
            expect (waitImports (*proc) && done && ! ok);
            expect (proc->getImportStatus (0).error);
            const auto after = proc->getCurrentPatch();
            expect (after.sampleId[0] == before.sampleId[0] && after.values == before.values, "failed import must not change the patch");
            proc->importToOscillator (0, dir.getChildFile ("readme.txt"));
            expect (proc->getImportStatus (0).error && proc->getImportStatus (0).text.contains ("Unsupported"));
        }

        beginTest ("Superseded and stale imports never replace newer state");
        {
            proc->importToOscillator (0, toneA3);
            proc->importToOscillator (0, toneC4); // newer request wins
            expect (waitImports (*proc));
            expect (proc->getCurrentPatch().sampleId[0].endsWith ("tone.wav"));

            proc->importToOscillator (0, toneA3);
            auto preset = PatchOps::initPatch();
            preset.meta.name = "Other";
            proc->applyPatch (preset, "Load preset while importing"); // user loads a preset before the import lands
            expect (waitImports (*proc));
            expect (! proc->getCurrentPatch().sampleId[0].endsWith ("named_A3.wav"), "stale import discarded");
            expect (proc->getImportStatus (0).error && proc->getImportStatus (0).text.contains ("discarded"));
        }

        beginTest ("Folder multisample import; project recall; missing file kept for relinking");
        {
            const auto folder = dir.getChildFile ("multi");
            folder.createDirectory();
            writeTone (folder.getChildFile ("ms_C3.wav"), c4 / 2.0, 0.6);
            writeTone (folder.getChildFile ("ms_C4.wav"), c4, 0.6);
            writeTone (folder.getChildFile ("ms_C5.wav"), c4 * 2.0, 0.6);
            proc->importToOscillator (1, folder);
            expect (waitImports (*proc) && ! proc->getImportStatus (1).error, proc->getImportStatus (1).text);
            expectEquals ((int) proc->getCurrentPatch().values[(size_t) O (1, osc::Mode)], 3);
            expect (proc->getImportStatus (1).text.contains ("3 zones"), proc->getImportStatus (1).text);

            const auto moved = writeTone (dir.getChildFile ("recall.wav"), c4, 1.0);
            proc->importToOscillator (0, moved);
            expect (waitImports (*proc));
            juce::MemoryBlock state;
            proc->getStateInformation (state);
            {
                auto restored = std::make_unique<ViceSynthProcessor>();
                restored->setStateInformation (state.getData(), (int) state.getSize());
                restored->getPatch().waitForLoads();
                const auto spec = restored->getCurrentPatch();
                expect (spec.sampleId[0].endsWith ("recall.wav"), "sample path recalled");
                expect (restored->getPatch().osc[0].sample.get() != nullptr, "sample reloaded on recall");
                expect (rmsRange (renderer->render (spec, 60, 100, 0.5, 0.1, 1), 48000.0, 0.1, 0.4) > 1.0e-2f);
            }
            moved.deleteFile();
            {
                auto restored = std::make_unique<ViceSynthProcessor>();
                restored->setStateInformation (state.getData(), (int) state.getSize());
                restored->getPatch().waitForLoads();
                expect (restored->getCurrentPatch().sampleId[0].endsWith ("recall.wav"), "missing file reference kept for relinking");
                expect (restored->getPatch().missingResources().joinIntoString (",").contains ("recall.wav"), "missing file reported");
            }
        }

        beginTest ("Destroying the processor while an import is running is safe");
        {
            auto p2 = std::make_unique<ViceSynthProcessor>();
            p2->importToOscillator (0, toneC4);
            p2.reset();
            expect (true);
        }
        dir.deleteRecursively();
    }
};

static SampleWorkflowTests sampleWorkflowTests;
} // namespace vsx
