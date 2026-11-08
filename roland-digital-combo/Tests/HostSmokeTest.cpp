// Host-level test: loads the COMPILED "Digital Combo.vst3" through JUCE's VST3 hosting code
// (the same path a JUCE-based DAW uses), plays it, checks the results and writes WAV demos.
//
// Usage: DigitalComboHostTest <path to Digital Combo.vst3> <output folder>
// Exit code = number of failed checks.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

namespace
{
int passed = 0, failed = 0;

void check (bool ok, const juce::String& what)
{
    if (ok) ++passed; else ++failed;
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    std::fflush (stdout);
}

constexpr double kRate = 48000.0;
constexpr int kBlock = 512;

std::unique_ptr<juce::AudioPluginInstance> load (const juce::String& path, juce::String& error)
{
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, path);
    if (types.isEmpty())
    {
        error = "no VST3 class found in " + path;
        return nullptr;
    }
    auto inst = format.createInstanceFromDescription (*types[0], kRate, kBlock, error);
    if (inst != nullptr)
    {
        inst->setPlayConfigDetails (0, 2, kRate, kBlock);
        inst->prepareToPlay (kRate, kBlock);
    }
    return inst;
}

juce::AudioProcessorParameter* param (juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (128) == name)
            return prm;
    return nullptr;
}

// Sets a parameter from its plain value, given the plain range the plug-in declares.
bool setPlain (juce::AudioPluginInstance& p, const juce::String& name, float value, float minV, float maxV)
{
    if (auto* prm = param (p, name))
    {
        prm->setValueNotifyingHost ((value - minV) / (maxV - minV));
        return true;
    }
    std::printf ("  (parameter not found: %s)\n", name.toRawUTF8());
    return false;
}

struct Event { int sample; juce::MidiMessage msg; };

juce::AudioBuffer<float> render (juce::AudioPluginInstance& p, double seconds, const std::vector<Event>& events)
{
    const int total = int (seconds * kRate);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    juce::AudioBuffer<float> block (2, kBlock);
    for (int done = 0; done < total; done += kBlock)
    {
        const int n = std::min (kBlock, total - done);
        block.setSize (2, n, false, false, true);
        block.clear();
        juce::MidiBuffer midi;
        for (auto& e : events)
            if (e.sample >= done && e.sample < done + n)
                midi.addEvent (e.msg, e.sample - done);
        p.processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, done, block, ch, 0, n);
    }
    return out;
}

bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i))) return false;
    return true;
}

float rms (const juce::AudioBuffer<float>& b)
{
    double s = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i) s += double (b.getSample (ch, i)) * b.getSample (ch, i);
    return float (std::sqrt (s / double (2 * std::max (1, b.getNumSamples()))));
}

float zeroCrossingHz (const juce::AudioBuffer<float>& b, int from, int to = -1)
{
    int crossings = 0, first = 0, last = 0;
    if (to < 0) to = b.getNumSamples();
    for (int i = from + 1; i < to; ++i)
        if (b.getSample (0, i - 1) <= 0.0f && b.getSample (0, i) > 0.0f)
        {
            if (crossings == 0) first = i;
            last = i;
            ++crossings;
        }
    return crossings < 2 ? 0.0f : float ((crossings - 1) * kRate / double (last - first));
}

void writeWav (const juce::AudioBuffer<float>& b, const juce::File& file)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (kRate)
                                                                               .withNumChannels (2)
                                                                               .withBitsPerSample (24));
    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

juce::MidiMessage on (int n, int v = 100) { return juce::MidiMessage::noteOn (1, n, (juce::uint8) v); }
juce::MidiMessage off (int n) { return juce::MidiMessage::noteOff (1, n); }

std::vector<Event> phrase (int root)
{
    // A short original phrase: chord, then a melody on top.
    const int s = int (kRate);
    std::vector<Event> e;
    for (int n : { root, root + 7, root + 12, root + 16 }) { e.push_back ({ 0, on (n, 90) }); e.push_back ({ 2 * s, off (n) }); }
    const int melody[] = { root + 24, root + 26, root + 28, root + 31 };
    for (int i = 0; i < 4; ++i)
    {
        e.push_back ({ s / 2 + i * s / 3, on (melody[i], 110) });
        e.push_back ({ s / 2 + (i + 1) * s / 3 - 200, off (melody[i]) });
    }
    return e;
}

void makeCleanD50 (juce::AudioPluginInstance& p)
{
    setPlain (p, "Play Mode", 1, 0, 3);
    setPlain (p, "D-50 Upper Partial Mute", 2, 0, 3);
    setPlain (p, "D-50 Upper P1 TVF Cutoff Frequency", 100, 0, 100);
    setPlain (p, "D-50 Upper P1 TVF ENV Depth", 0, 0, 100);
    setPlain (p, "D-50 Upper Chorus Balance", 0, 0, 100);
    setPlain (p, "D-50 Reverb Balance", 0, 0, 100);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3)
    {
        std::printf ("usage: DigitalComboHostTest <plugin.vst3> <output dir>\n");
        return 2;
    }
    const juce::String path (argv[1]);
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    outDir.createDirectory();
    std::printf ("Host test of %s\n\n", path.toRawUTF8());

    juce::String err;
    auto p = load (path, err);
    check (p != nullptr, "VST3 binary loads through JUCE's VST3 host" + (err.isEmpty() ? juce::String() : " (" + err + ")"));
    if (p == nullptr)
    {
        std::printf ("\nRESULT: %d passed, %d failed\n", passed, failed);
        return 1;
    }
    check (p->getName() == "Digital Combo" && p->acceptsMidi() && p->getTotalNumOutputChannels() == 2,
           "reports name '" + p->getName() + "', accepts MIDI, 2 outputs");
    check (p->getParameters().size() >= 155, "exposes " + juce::String (p->getParameters().size()) + " parameters to the host");
    check (p->getNumPrograms() == 10, "exposes " + juce::String (p->getNumPrograms()) + " programs");

    // ---- each play mode, recorded as WAV demo ----
    const char* modes[] = { "JD-800", "D-50", "LAYER", "SPLIT" };
    for (int m = 0; m < 4; ++m)
    {
        auto inst = load (path, err);
        setPlain (*inst, "Play Mode", (float) m, 0, 3);
        render (*inst, 0.05, {});
        auto audio = render (*inst, 4.0, phrase (m == 3 ? 48 : 48));
        const float level = rms (audio);
        check (allFinite (audio) && level > 0.005f && audio.getMagnitude (0, audio.getNumSamples()) < 4.0f,
               juce::String ("mode ") + modes[m] + ": audio produced through the host (rms " + juce::String (level, 4) + ")");
        writeWav (audio, outDir.getChildFile (juce::String ("mode-") + modes[m] + ".wav"));
    }

    // ---- built-in presets via the host's program interface ----
    for (int i = 0; i < p->getNumPrograms(); ++i)
    {
        auto inst = load (path, err);
        inst->setCurrentProgram (i);
        render (*inst, 0.05, {});
        auto audio = render (*inst, 4.0, phrase (48));
        const float level = rms (audio);
        check (allFinite (audio) && level > 0.003f && inst->getCurrentProgram() == i,
               "program " + juce::String (i + 1) + " '" + inst->getProgramName (i) + "' plays (rms " + juce::String (level, 4) + ")");
        writeWav (audio, outDir.getChildFile ("preset-" + juce::String (i + 1) + "-"
                                              + juce::File::createLegalFileName (inst->getProgramName (i)).replaceCharacter (' ', '_') + ".wav"));
    }

    // ---- pitch and SysEx through the VST3 boundary ----
    {
        auto inst = load (path, err);
        makeCleanD50 (*inst);
        render (*inst, 0.05, {});
        auto a = render (*inst, 1.0, { { 0, on (69) }, { 40000, off (69) } });
        const float f1 = zeroCrossingHz (a, 24000, 38000); // measured while the note is held
        check (std::abs (f1 - 440.0f) < 4.0f, "D-50 A4 through the host: " + juce::String (f1, 2) + " Hz (expected 440)");
        // DT1: Upper Partial-1 WG Pitch Coarse = 48 (C5) -> one octave up
        const uint8_t dt1[] = { 0xF0, 0x41, 0x00, 0x14, 0x12, 0x00, 0x00, 0x00, 0x30, 0x50, 0xF7 };
        render (*inst, 1.0, { { 0, juce::MidiMessage (dt1, (int) sizeof (dt1)) } });
        auto b = render (*inst, 1.0, { { 0, on (69) }, { 40000, off (69) } });
        const float f2 = zeroCrossingHz (b, 24000, 38000);
        check (std::abs (f2 - 880.0f) < 8.0f, "D-50 DT1 SysEx sent through the VST3 host changes pitch: " + juce::String (f2, 2) + " Hz (expected 880)");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
        auto* coarse = param (*inst, "D-50 Upper P1 WG Pitch Coarse");
        check (coarse != nullptr && std::abs (coarse->getValue() * 72.0f - 48.0f) < 0.5f,
               "host sees the SysEx-driven parameter change (Coarse = " + (coarse != nullptr ? coarse->getCurrentValueAsText() : juce::String ("?")) + ")");
    }

    // ---- state: restored instance sounds exactly like a directly configured one ----
    {
        auto configure = [] (juce::AudioPluginInstance& inst)
        {
            setPlain (inst, "Play Mode", 3, 0, 3);
            setPlain (inst, "Split Point", 55, 0, 127);
            setPlain (inst, "D-50 Upper P1 TVF Cutoff Frequency", 37, 0, 100);
            setPlain (inst, "JD Tone A TVF Cutoff freq", 44, 0, 100);
            setPlain (inst, "JD Delay Center level", 40, 0, 100);
        };
        auto a = load (path, err);
        configure (*a);
        render (*a, 0.05, {});
        juce::MemoryBlock state;
        a->getStateInformation (state);

        auto direct = load (path, err);
        configure (*direct);
        auto restored = load (path, err);
        restored->setStateInformation (state.getData(), (int) state.getSize());
        render (*direct, 0.05, {});
        render (*restored, 0.05, {});

        int mismatched = 0;
        for (auto* prm : a->getParameters())
            if (auto* other = param (*restored, prm->getName (128)))
                if (std::abs (other->getValue() - prm->getValue()) > 1.0e-4f) ++mismatched;
        check (mismatched == 0, "all host-visible parameter values restored (" + juce::String (mismatched) + " mismatches)");

        auto x = render (*direct, 3.0, phrase (43));
        auto y = render (*restored, 3.0, phrase (43));
        float diff = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < x.getNumSamples(); ++i) diff = std::max (diff, std::abs (x.getSample (ch, i) - y.getSample (ch, i)));
        check (diff < 1.0e-6f && rms (x) > 0.005f, "restored instance renders identical audio (max difference " + juce::String (diff, 8) + ")");
    }

    // ---- sample-rate change and repeated load/unload ----
    {
        auto inst = load (path, err);
        bool ok = true;
        for (double sr : { 44100.0, 96000.0, 48000.0 })
        {
            inst->releaseResources();
            inst->setPlayConfigDetails (0, 2, sr, kBlock);
            inst->prepareToPlay (sr, kBlock);
            auto audio = render (*inst, 0.5, { { 0, on (60) } });
            ok = ok && allFinite (audio) && rms (audio) > 0.002f;
        }
        check (ok, "sample-rate changes 44.1k -> 96k -> 48k keep producing valid audio");
        int loaded = 0;
        for (int i = 0; i < 10; ++i)
            if (auto tmp = load (path, err)) { render (*tmp, 0.02, { { 0, on (60) } }); ++loaded; }
        check (loaded == 10, "10 consecutive load / play / unload cycles");
    }

    // ---- editor opened by the host ----
    {
        auto inst = load (path, err);
        std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorAndMakeActive());
        bool ok = ed != nullptr && ed->getWidth() > 600 && ed->getHeight() > 400;
        if (ed != nullptr)
        {
            // Same pattern as a host plug-in window: the editor lives inside a top-level window.
            auto window = std::make_unique<juce::DocumentWindow> ("Digital Combo", juce::Colours::black,
                                                                  juce::DocumentWindow::closeButton);
            window->setUsingNativeTitleBar (true);
            window->setContentNonOwned (ed.get(), true);
            window->setVisible (true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1000);
            ok = ok && window->isOnDesktop() && ed->isShowing();
            render (*inst, 0.2, { { 0, on (60) } });    // audio keeps running while the editor is open
            window->setVisible (false);
            window->clearContentComponent();
            window.reset();
        }
        ed.reset();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        check (ok, "host opens the plug-in editor window (" + juce::String (ok ? "shown and closed" : "failed") + ")");
    }

    std::printf ("\nWAV demos written to %s\n", outDir.getFullPathName().toRawUTF8());
    std::printf ("\nRESULT: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
