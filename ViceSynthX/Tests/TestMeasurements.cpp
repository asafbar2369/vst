#include "../Source/Analysis/SoundMatcher.h"
#include "../Source/AI/PatchOps.h"
#include "../Source/PluginProcessor.h"
#include "../Source/Presets/PresetManager.h"
#include "../Source/Synth/Filters.h"
#include <juce_dsp/juce_dsp.h>
#include <random>

/*
    Objective DSP measurements and regression guards. Every measured value is printed as
        MEASURE|<area>|<item>|<value>|<unit>
    so CI logs can be turned into the audio-quality, CPU and factory-library reports. The
    assertions are regression guards (finite output, no clipping, expected filter slopes,
    pitch accuracy...), not claims of equivalence with any hardware.
*/
namespace vsx
{
namespace
{
    float rms (const float* x, int n) { double s = 0; for (int i = 0; i < n; ++i) s += (double) x[i] * x[i]; return (float) std::sqrt (s / std::max (1, n)); }
    float db (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-12f)); }

    bool allFinite (const juce::AudioBuffer<float>& b, float& peak)
    {
        peak = 0.0f;
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float v = b.getSample (c, i);
                if (! std::isfinite (v)) return false;
                peak = std::max (peak, std::abs (v));
            }
        return true;
    }

    /** RMS (dB) of the loudest 400 ms window of the mono sum (the factory-level calibration metric). */
    float loudestWindowDb (const juce::AudioBuffer<float>& b, double sr)
    {
        const int win = (int) (0.4 * sr), hop = win / 4;
        float best = 0.0f;
        for (int s = 0; s + win <= b.getNumSamples(); s += hop)
        {
            double acc = 0;
            for (int i = s; i < s + win; ++i)
            {
                const float m = 0.5f * (b.getSample (0, i) + b.getSample (b.getNumChannels() > 1 ? 1 : 0, i));
                acc += (double) m * m;
            }
            best = std::max (best, (float) std::sqrt (acc / win));
        }
        return db (best);
    }

    /** Blackman-Harris magnitude spectrum of a mono segment (power per bin). */
    std::vector<double> powerSpectrum (const float* x, int order)
    {
        const int n = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> buf ((size_t) n * 2, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / (n - 1), tp = juce::MathConstants<double>::twoPi * t;
            const double w = 0.35875 - 0.48829 * std::cos (tp) + 0.14128 * std::cos (2 * tp) - 0.01168 * std::cos (3 * tp);
            buf[(size_t) i] = (float) (x[i] * w);
        }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        std::vector<double> p ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i) p[(size_t) i] = (double) buf[(size_t) i] * buf[(size_t) i];
        return p;
    }

    /** Ratio (dB) of non-harmonic to harmonic energy below Nyquist: an aliasing / inharmonicity measure. */
    float inharmonicRatioDb (const float* x, int order, double sr, double f0)
    {
        const auto p = powerSpectrum (x, order);
        const double binHz = sr / (1 << order);
        double harm = 0, other = 0;
        for (size_t k = 3; k < p.size(); ++k) // skip DC
        {
            const double f = (double) k * binHz;
            const double h = f / f0, dist = std::abs (h - std::round (h)) * f0;
            if (std::round (h) >= 1.0 && dist <= 4.0 * binHz) harm += p[k]; else other += p[k];
        }
        return (float) (10.0 * std::log10 (std::max (other, 1.0e-30) / std::max (harm, 1.0e-30)));
    }

    PatchSpec cleanInit()
    {
        auto s = PatchOps::initPatch();
        s.set ("osc_drift", 0.0f);
        return s;
    }

    void measure (juce::UnitTest& t, const juce::String& area, const juce::String& item, double value, const juce::String& unit)
    {
        t.logMessage ("MEASURE|" + area + "|" + item + "|" + juce::String (value, 3) + "|" + unit);
    }
}

class MeasurementTests final : public juce::UnitTest
{
public:
    MeasurementTests() : juce::UnitTest ("DSP measurements", "VICE SYNTH X") {}

    void runTest() override
    {
        // VSX_MEASURE_SECTION=<name> runs one section (filters, oscillators, effects, library, processor, state, longrun)
        const auto only = juce::SystemStats::getEnvironmentVariable ("VSX_MEASURE_SECTION", {});
        auto want = [&only] (const char* n) { return only.isEmpty() || only.equalsIgnoreCase (n); };
        if (want ("filters")) filters();
        if (want ("oscillators")) oscillators();
        if (want ("effects")) effects();
        if (want ("library")) factoryLibrary();
        if (want ("processor")) processorMatrix();
        if (want ("state")) stateAndAutomation();
        if (want ("longrun")) longRun();
    }

    // ------------------------------------------------------------------------------------
    float filterGain (int model, int mode, float cutoff, float freq, double sr, float amp = 0.05f)
    {
        AnalogFilter f;
        f.prepare (sr);
        f.setModel (model, mode);
        f.update (cutoff, 0.0f, 0.0f, 0.0f, 0);
        const int n = (int) (0.3 * sr), tail = (int) (0.1 * sr);
        double in = 0, out = 0;
        for (int i = 0; i < n; ++i)
        {
            float x = amp * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / sr);
            float l = x, r = x;
            f.process (l, r);
            if (i >= n - tail) { in += (double) x * x; out += (double) l * l; }
        }
        return (float) std::sqrt (out / std::max (in, 1.0e-30));
    }

    void filters()
    {
        const double sr = 96000.0; // filters run at the oversampled rate
        for (int m = 0; m < choices::filterModels.size(); ++m)
        {
            const auto name = choices::filterModels[m];
            beginTest ("Filter " + name + ": slopes, resonance, stability");
            // low-pass 24 / 12 and high-pass slopes at fc = 1 kHz
            const float ref = filterGain (m, 0, 1000.0f, 100.0f, sr);
            const float at1k = filterGain (m, 0, 1000.0f, 1000.0f, sr);
            const float lp24 = filterGain (m, 0, 1000.0f, 8000.0f, sr);
            const float lp12 = filterGain (m, 1, 1000.0f, 8000.0f, sr);
            const float hpLow = filterGain (m, 3, 1000.0f, 125.0f, sr), hpHigh = filterGain (m, 3, 1000.0f, 8000.0f, sr);
            measure (*this, "filter", name + " LP24 gain@fc rel 100Hz", db (at1k / ref), "dB");
            measure (*this, "filter", name + " LP24 attenuation 3 oct above fc", db (lp24 / ref), "dB");
            measure (*this, "filter", name + " LP12 attenuation 3 oct above fc", db (lp12 / filterGain (m, 1, 1000.0f, 100.0f, sr)), "dB");
            measure (*this, "filter", name + " HP attenuation 3 oct below fc", db (hpLow / hpHigh), "dB");
            expect (db (lp24 / ref) < -30.0f, name + ": LP24 must attenuate > 30 dB three octaves above cutoff");
            expect (db (lp12 / filterGain (m, 1, 1000.0f, 100.0f, sr)) < -15.0f, name + ": LP12 must attenuate > 15 dB three octaves above cutoff");
            expect (db (hpLow / hpHigh) < -15.0f, name + ": HP must attenuate > 15 dB three octaves below cutoff");

            // self-oscillation: impulse, then silence at maximum resonance
            {
                AnalogFilter f;
                f.prepare (sr);
                f.setModel (m, 0);
                f.update (1000.0f, 1.0f, 0.0f, 0.0f, 0);
                std::vector<float> y ((size_t) sr);
                for (int i = 0; i < (int) sr; ++i)
                {
                    float l = i < 4 ? 0.5f : 0.0f, r = l;
                    f.process (l, r);
                    y[(size_t) i] = l;
                }
                const int tail = (int) (0.25 * sr);
                const float level = rms (y.data() + y.size() - (size_t) tail, tail);
                // dominant frequency from the spectrum (zero crossings are unreliable for distorted waveforms)
                const int order = 14;
                const auto spec = powerSpectrum (y.data() + y.size() - (size_t) (1 << order), order);
                size_t peakBin = 1;
                for (size_t k = 2; k < spec.size() - 1; ++k) if (spec[k] > spec[peakBin]) peakBin = k;
                const double a = std::log (spec[peakBin - 1] + 1e-30), b = std::log (spec[peakBin] + 1e-30), c = std::log (spec[peakBin + 1] + 1e-30);
                const double offset = 0.5 * (a - c) / (a - 2.0 * b + c);
                const double freq = ((double) peakBin + (std::isfinite (offset) ? offset : 0.0)) * sr / (1 << order);
                measure (*this, "filter", name + " self-oscillation level (res=1)", db (level), "dBFS");
                if (level > 1.0e-3f) measure (*this, "filter", name + " self-oscillation frequency (fc=1kHz)", freq, "Hz");
                float pk = 0;
                for (auto v : y) { expect (std::isfinite (v)); pk = std::max (pk, std::abs (v)); }
                expect (pk < 10.0f, name + ": self-oscillation must stay bounded");
            }

            // harmonic distortion with drive (100 Hz, fc = 10 kHz)
            {
                AnalogFilter f;
                f.prepare (sr);
                f.setModel (m, 0);
                f.update (10000.0f, 0.0f, 0.6f, 0.0f, 0);
                const int order = 15, n = 1 << order;
                std::vector<float> y ((size_t) n);
                for (int i = 0; i < n + 9600; ++i)
                {
                    float x = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 93.75 * i / sr), r = x;
                    f.process (x, r);
                    if (i >= 9600) y[(size_t) (i - 9600)] = x;
                }
                const auto p = powerSpectrum (y.data(), order);
                const double binHz = sr / n;
                auto bandPower = [&] (double hz) { double s = 0; const int c = (int) std::round (hz / binHz); for (int k = c - 3; k <= c + 3; ++k) if (k > 0 && k < (int) p.size()) s += p[(size_t) k]; return s; };
                const double fund = bandPower (93.75);
                double harm = 0;
                for (int h = 2; h <= 10; ++h) harm += bandPower (93.75 * h);
                measure (*this, "filter", name + " THD (drive 0.6, 0.5 FS, 94 Hz)", 100.0 * std::sqrt (harm / std::max (fund, 1.0e-30)), "%");
            }

            // stability under fast random modulation with noise input
            {
                AnalogFilter f;
                f.prepare (sr);
                f.setModel (m, 0);
                std::mt19937 rng ((unsigned) m + 7);
                std::uniform_real_distribution<float> u (0.0f, 1.0f);
                float pk = 0.0f;
                bool finite = true;
                for (int i = 0; i < (int) sr * 2; ++i)
                {
                    if (i % 16 == 0) f.update (20.0f * std::pow (1000.0f, u (rng)), u (rng), u (rng), u (rng), 16);
                    float l = u (rng) * 2.0f - 1.0f, r = u (rng) * 2.0f - 1.0f;
                    f.process (l, r);
                    finite = finite && std::isfinite (l) && std::isfinite (r);
                    pk = std::max ({ pk, std::abs (l), std::abs (r) });
                }
                measure (*this, "filter", name + " peak under random modulation (noise in)", db (pk), "dBFS");
                expect (finite, name + ": non-finite output under modulation");
                expect (pk < 20.0f, name + ": output exploded under modulation");
            }
        }
    }

    // ------------------------------------------------------------------------------------
    void oscillators()
    {
        beginTest ("Oscillators: pitch accuracy and aliasing per quality setting");
        auto renderer = std::make_unique<OfflineRenderer> (48000.0);
        {
            auto s = cleanInit();
            const auto out = renderer->render (s, 69, 100, 1.0, 0.0, 1);
            const auto f = AudioFeatures::analyse (out, 48000.0);
            const double cents = 1200.0 * std::log2 (std::max (1.0f, f.f0) / 440.0);
            measure (*this, "oscillator", "Analog saw A4 pitch error", cents, "cents");
            expect (std::abs (cents) < 5.0, "A4 must be within 5 cents, got " + juce::String (cents));
        }
        for (int q = 0; q < 3; ++q)
        {
            auto s = cleanInit();
            s.set ("flt1_cutoff", 20000.0f);
            const int note = 96; // C7, 2093 Hz: most harmonics above Nyquist
            const auto out = renderer->render (s, note, 100, 1.2, 0.0, q);
            const int order = 15;
            const float r = inharmonicRatioDb (out.getReadPointer (0) + 9600, order, 48000.0, 440.0 * std::pow (2.0, (note - 69) / 12.0));
            measure (*this, "oscillator", "Analog saw C7 inharmonic/aliasing energy, quality " + choices::qualities[q], r, "dB");
            expect (r < -25.0f, "aliasing guard (quality " + juce::String (q) + "): " + juce::String (r));
        }
        for (int mode = 0; mode < choices::oscModes.size(); ++mode)
        {
            auto s = cleanInit();
            s.set ("osc_a_mode", (float) mode);
            const auto out = renderer->render (s, 60, 100, 0.5, 0.3, 1);
            float pk = 0.0f;
            const bool finite = allFinite (out, pk);
            measure (*this, "oscillator", "mode " + choices::oscModes[mode] + " peak (init patch)", db (pk), "dBFS");
            expect (finite, "mode " + choices::oscModes[mode] + " non-finite");
        }
    }

    // ------------------------------------------------------------------------------------
    void effects()
    {
        beginTest ("Effects: every effect produces finite, bounded output");
        auto renderer = std::make_unique<OfflineRenderer> (48000.0);
        auto base = cleanInit();
        base.set ("env1_decay", 0.25f);
        base.set ("env1_sustain", 0.0f);
        base.set ("env1_release", 0.05f);
        const auto dry = renderer->render (base, 60, 110, 0.3, 2.0, 1);
        const float dryDb = loudestWindowDb (dry, 48000.0);
        for (int t = 0; t < choices::fxTypeIds.size(); ++t)
        {
            auto s = base;
            const auto id = choices::fxTypeIds[t];
            expect (s.set ("fx_" + id + "_on", 1.0f), "missing switch for " + id);
            const auto out = renderer->render (s, 60, 110, 0.3, 2.0, 1);
            float pk = 0.0f;
            const bool finite = allFinite (out, pk);
            const float level = loudestWindowDb (out, 48000.0);
            const float tail = db (rms (out.getReadPointer (0) + out.getNumSamples() - 24000, 24000));
            measure (*this, "fx", id + " level change vs dry (loudest 400 ms)", level - dryDb, "dB");
            measure (*this, "fx", id + " peak", db (pk), "dBFS");
            measure (*this, "fx", id + " tail level 1.8-2.3 s", tail, "dBFS");
            expect (finite, id + ": non-finite output");
            expect (pk < 4.0f, id + ": output peak " + juce::String (db (pk), 1) + " dBFS");
        }
    }

    // ------------------------------------------------------------------------------------
    /** Level as played: arp / sequencer active, bass at C2, others at C4, held >= attack + 1.5 s.
        Loudness = loudest 400 ms window of the mean channel power (the calibration metric). */
    static void presetLevel (OfflineRenderer& r, const PatchSpec& p, float& levelDb, float& peakDb, bool& finite, AudioFeatures* features)
    {
        const int note = p.meta.category == "Bass" ? 36 : 60;
        const double secs = std::max (2.0, (double) p.get ("env1_attack") + 1.5);
        const auto out = r.render (p, note, 100, secs, 1.0, 1);
        const double sr = r.getSampleRate();
        const int win = (int) (0.4 * sr), hop = win / 4;
        double best = 0;
        for (int st = 0; st + win <= out.getNumSamples(); st += hop)
        {
            double sq = 0;
            for (int c = 0; c < 2; ++c)
                for (int i = st; i < st + win; ++i) { const float v = out.getSample (c, i); sq += (double) v * v; }
            best = std::max (best, sq / (2.0 * win));
        }
        float pk = 0;
        finite = allFinite (out, pk);
        levelDb = (float) (10.0 * std::log10 (best + 1.0e-12));
        peakDb = db (pk);
        if (features != nullptr) *features = AudioFeatures::analyse (out, sr, secs);
    }

    void factoryLibrary()
    {
        beginTest ("Factory library: every preset renders as played, level window, no clipping, duplicates");
        auto renderer = std::make_unique<OfflineRenderer> (44100.0);
        const auto& presets = FactoryPresets::all();
        std::vector<AudioFeatures> feats (presets.size());
        float minDb = 0, maxDb = -200, maxPeak = -200;
        int silent = 0, clipped = 0, outside = 0;
        for (size_t i = 0; i < presets.size(); ++i)
        {
            const auto& p = presets[i];
            float level = 0, pk = 0;
            bool finite = true;
            presetLevel (*renderer, p, level, pk, finite, &feats[i]);
            logMessage ("LEVEL|" + p.meta.name + "|" + juce::String (level, 2) + "|" + juce::String (pk, 2) + "|" + juce::String (p.get ("master_volume"), 2));
            measure (*this, "preset", p.meta.name + " (" + p.meta.category + ") loudest-400ms RMS", level, "dBFS");
            measure (*this, "preset", p.meta.name + " peak", pk, "dBFS");
            expect (finite, p.meta.name + ": non-finite");
            if (level < -45.0f) ++silent;
            if (pk > -3.0f) { ++clipped; logMessage ("peak above -3 dBFS: " + p.meta.name); }
            // transient-heavy sounds are calibrated to the -4 dBFS peak ceiling instead (crest factor too high)
            const bool peakLimited = level < -16.0f && pk > -4.6f;
            if (peakLimited) measure (*this, "preset", p.meta.name + " peak-limited (crest factor)", pk - level, "dB");
            if ((level < -19.0f && ! peakLimited) || level > -13.0f) { ++outside; logMessage ("level outside -16 +/- 3 dB: " + p.meta.name); }
            minDb = std::min (minDb, level); maxDb = std::max (maxDb, level); maxPeak = std::max (maxPeak, pk);
        }
        measure (*this, "library", "presets rendered", (double) presets.size(), "count");
        measure (*this, "library", "level window min", minDb, "dBFS");
        measure (*this, "library", "level window max", maxDb, "dBFS");
        measure (*this, "library", "highest peak", maxPeak, "dBFS");
        expectEquals (silent, 0, "silent presets");
        expectEquals (clipped, 0, "presets peaking above -3 dBFS");
        expectEquals (outside, 0, "presets outside -16 +/- 3 dB");

        // near-duplicates: parameter distance + acoustic similarity
        const auto& reg = ParamRegistry::get();
        int nearDup = 0;
        for (size_t a = 0; a < presets.size(); ++a)
            for (size_t b = a + 1; b < presets.size(); ++b)
            {
                double d = 0;
                for (int i = 0; i < reg.size(); ++i)
                {
                    const auto& def = reg[i];
                    d += std::abs (presets[a].values[(size_t) i] - presets[b].values[(size_t) i]) / std::max (1.0e-6f, def.maxValue - def.minValue);
                }
                const float sim = AudioFeatures::similarity (feats[a], feats[b]);
                if (d < 0.5 || sim > 0.985f)
                {
                    ++nearDup;
                    measure (*this, "library", "near-duplicate candidate: " + presets[a].meta.name + " / " + presets[b].meta.name
                                                   + " (param distance " + juce::String (d, 2) + ")", sim, "similarity");
                }
            }
        measure (*this, "library", "near-duplicate candidates", nearDup, "pairs");
    }

    // ------------------------------------------------------------------------------------
    static void playChord (ViceSynthProcessor& p, double sr, int blockSize, double seconds, double& cpuSeconds, float& peak, bool& finite)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        const int blocks = (int) (seconds * sr / blockSize);
        cpuSeconds = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();
            if (b == 0) for (int n : { 48, 55, 60, 64, 67, 72 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            if (b == blocks * 3 / 4) for (int n : { 48, 55, 60, 64, 67, 72 }) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            buffer.clear();
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            p.processBlock (buffer, midi);
            cpuSeconds += (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
            float pk = 0.0f;
            finite = finite && allFinite (buffer, pk);
            peak = std::max (peak, pk);
        }
    }

    void processorMatrix()
    {
        beginTest ("Processor: sample rates x buffer sizes (6-note chord, factory pad)");
        for (const double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            for (const int bs : { 32, 64, 128, 256, 512, 1024 })
            {
                auto proc = std::make_unique<ViceSynthProcessor>();
                proc->setPlayConfigDetails (0, 2, sr, bs);
                proc->prepareToPlay (sr, bs);
                const int idx = proc->getPresetManager().indexOfName ("Ocean Drive Poly");
                if (idx >= 0) proc->loadPresetEntry (proc->getPresetManager().getEntries()[(size_t) idx]);
                double cpu = 0;
                float pk = 0;
                bool finite = true;
                playChord (*proc, sr, bs, 2.0, cpu, pk, finite);
                const auto cfg = juce::String ((int) sr) + " Hz / " + juce::String (bs);
                measure (*this, "cpu", cfg + " realtime factor (6 voices, quality High 2x, this machine)", 2.0 / std::max (cpu, 1.0e-9), "x realtime");
                measure (*this, "cpu", cfg + " peak", db (pk), "dBFS");
                expect (finite, cfg + ": non-finite output");
                expect (pk > 1.0e-4f, cfg + ": silent output");
                expect (pk < 2.0f, cfg + ": excessive peak");
                measure (*this, "latency", cfg + " reported latency", proc->getLatencySamples(), "samples");
                expect (proc->getLatencySamples() >= 0 && proc->getLatencySamples() < 64, "latency report");
                proc->releaseResources();
            }

        beginTest ("Processor: voice stealing with 64 simultaneous notes");
        auto proc = std::make_unique<ViceSynthProcessor>();
        proc->setPlayConfigDetails (0, 2, 48000.0, 256);
        proc->prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        for (int n = 20; n < 84; ++n) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 90), n % 256);
        float pk = 0;
        bool finite = true;
        for (int b = 0; b < 200; ++b)
        {
            buffer.clear();
            proc->processBlock (buffer, midi);
            midi.clear();
            float p2 = 0; finite = finite && allFinite (buffer, p2); pk = std::max (pk, p2);
        }
        measure (*this, "voices", "active voices after 64 note-ons", proc->getEngine().activeVoiceCount.load(), "voices");
        expect (finite);
        expect (proc->getEngine().activeVoiceCount.load() <= kMaxVoices + 8, "voice slots incl. stolen voices fading out");
        measure (*this, "voices", "peak with 64 held notes", db (pk), "dBFS");
    }

    // ------------------------------------------------------------------------------------
    void stateAndAutomation()
    {
        beginTest ("Processor: state save / restore (project recall)");
        auto a = std::make_unique<ViceSynthProcessor>();
        a->prepareToPlay (48000.0, 256);
        const auto& entries = a->getPresetManager().getEntries();
        if (! entries.empty()) a->loadPresetEntry (entries[entries.size() / 2]);
        a->setParam (pid::G (pid::global::MasterVolume), -7.5f);
        juce::MemoryBlock state;
        a->getStateInformation (state);
        auto b = std::make_unique<ViceSynthProcessor>();
        b->prepareToPlay (44100.0, 512);
        b->setStateInformation (state.getData(), (int) state.getSize());
        const auto pa = a->getCurrentPatch(), pb = b->getCurrentPatch();
        int diffs = 0;
        for (size_t i = 0; i < pa.values.size(); ++i)
            if (const auto& d = ParamRegistry::get()[(int) i]; std::abs (pa.values[i] - pb.values[i]) > 1.0e-5f * std::max (1.0f, d.maxValue - d.minValue))
            {
                ++diffs;
                logMessage ("recall mismatch: " + ParamRegistry::get()[(int) i].id + " saved " + juce::String (pa.values[i]) + " restored " + juce::String (pb.values[i]));
            }
        expectEquals (diffs, 0, "parameters differing after recall");
        expectEquals (pb.meta.name, pa.meta.name);
        measure (*this, "state", "state size", (double) state.getSize(), "bytes");

        beginTest ("Processor: automation of every automatable parameter while playing");
        juce::AudioBuffer<float> buffer (2, 128);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        std::mt19937 rng (42);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        auto& params = b->getParameters();
        bool finite = true;
        float pk = 0;
        for (int blk = 0; blk < 1500; ++blk)
        {
            for (int k = 0; k < 4; ++k)
                if (auto* prm = params[(int) (u (rng) * (float) (params.size() - 1))]; prm != nullptr && prm->isAutomatable())
                    prm->setValueNotifyingHost (u (rng));
            buffer.clear();
            b->processBlock (buffer, midi);
            midi.clear();
            if (blk % 300 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 48 + blk % 24, (juce::uint8) 100), 3);
            float p2 = 0; finite = finite && allFinite (buffer, p2); pk = std::max (pk, p2);
        }
        expect (finite, "non-finite output under random automation");
        measure (*this, "automation", "peak under random automation (6000 changes)", db (pk), "dBFS");
    }

    // ------------------------------------------------------------------------------------
    void longRun()
    {
        beginTest ("Processor: long-run stability (5 minutes of audio, random notes, macros, presets)");
        auto proc = std::make_unique<ViceSynthProcessor>();
        const double sr = 48000.0;
        const int bs = 256;
        proc->prepareToPlay (sr, bs);
        const auto& entries = proc->getPresetManager().getEntries();
        juce::AudioBuffer<float> buffer (2, bs);
        juce::MidiBuffer midi;
        std::mt19937 rng (7);
        std::uniform_int_distribution<int> note (36, 84), vel (30, 127);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        const int blocks = (int) (300.0 * sr / bs);
        bool finite = true;
        float pk = 0.0f;
        double cpu = 0;
        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();
            if (b % 40 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note (rng), (juce::uint8) vel (rng)), (int) (u (rng) * (bs - 1)));
            if (b % 40 == 25) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            if (b % 50 == 0) proc->setParam (pid::MACRO ((int) (u (rng) * 7.99f)), u (rng));
            if (b % 4000 == 0 && ! entries.empty()) proc->loadPresetEntry (entries[(size_t) (u (rng) * (float) (entries.size() - 1))]);
            buffer.clear();
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            proc->processBlock (buffer, midi);
            cpu += (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
            float p2 = 0; finite = finite && allFinite (buffer, p2); pk = std::max (pk, p2);
        }
        expect (finite, "non-finite output during the long run");
        expect (pk < 4.0f, "peak during long run " + juce::String (db (pk), 1) + " dBFS");
        measure (*this, "longrun", "audio rendered", 300.0, "s");
        measure (*this, "longrun", "peak", db (pk), "dBFS");
        measure (*this, "longrun", "realtime factor", 300.0 / std::max (cpu, 1.0e-9), "x realtime");
    }
};

static MeasurementTests measurementTests;
} // namespace vsx
