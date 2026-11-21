#include "../Source/Synth/FmVoice.h"
#include "../Source/ThirdParty/msfa/synth.h"
#include "../Source/ThirdParty/msfa/dx7note.h"
#include "../Source/ThirdParty/msfa/fm_core.h"
#include "../Source/ThirdParty/msfa/lfo.h"
#include "../Source/ThirdParty/msfa/controllers.h"
#include "../Source/ThirdParty/msfa/aligned_buf.h"
#include "../Source/ThirdParty/msfa/tuning.h"
#include <algorithm>
#include <cstring>
#include <new>

/*
    FM equivalence with Dexed's "Modern" engine (docs/FM_DEXED_COMPARISON.md).

    Reference: the MSFA files in Source/ThirdParty/msfa (byte-identical to Dexed 1ed5a4d source/msfa) driven the way
    Dexed's DexedAudioProcessor does it: 16 voices, global LFO reset on program load and triggered by the first key,
    oscillator key sync on note-on, pitch-bend centred, per-voice >> 4 / >> 9 conversion with Dexed's overflow
    handling, voices summed, 64-sample engine blocks, events applied at block starts. (Dexed's output DC blocker and
    its optional filter are output stages, not part of the engine, and are left out of both sides.)
    Device under test: VICE SYNTH X fm::FmRenderer at 48 kHz, one renderer per note (as the synth's voices do).
    Patches: every factory FM voice at two keys / velocities, 32 random voices (one per algorithm; random envelopes,
    scaling, detune, fixed frequencies, feedback, pitch EG, LFO except sample-and-hold), and a five-note chord.
    With VSX_DEXED_DIR set, the raw patches, events and both renders are also written for the external harness
    (tools/dexed_reference), which builds the reference from a separate Dexed checkout.
*/
namespace vsx
{
class DexedEquivalenceTests final : public juce::UnitTest
{
public:
    DexedEquivalenceTests() : juce::UnitTest ("FM Dexed equivalence", "VICE SYNTH X") {}

    static fm::Patch randomPatch (juce::Random& r, int algorithm)
    {
        auto p = fm::Patch::init();
        p.algorithm = (uint8_t) algorithm;
        p.feedback = (uint8_t) r.nextInt (8);
        p.oscSync = (uint8_t) r.nextInt (2);
        for (auto& o : p.op)
        {
            for (int k = 0; k < 4; ++k) { o.rate[(size_t) k] = (uint8_t) (40 + r.nextInt (60)); o.level[(size_t) k] = (uint8_t) r.nextInt (100); }
            o.level[0] = (uint8_t) (80 + r.nextInt (20));
            o.level[3] = 0;
            o.breakPoint = (uint8_t) r.nextInt (100);
            o.leftDepth = (uint8_t) r.nextInt (60); o.rightDepth = (uint8_t) r.nextInt (60);
            o.leftCurve = (uint8_t) r.nextInt (4); o.rightCurve = (uint8_t) r.nextInt (4);
            o.rateScale = (uint8_t) r.nextInt (8);
            o.ampModSens = (uint8_t) r.nextInt (4);
            o.velSens = (uint8_t) r.nextInt (8);
            o.outputLevel = (uint8_t) (55 + r.nextInt (45));
            o.fixedMode = (uint8_t) (r.nextInt (8) == 0 ? 1 : 0);
            o.coarse = (uint8_t) (o.fixedMode ? r.nextInt (4) : r.nextInt (8));
            o.fine = (uint8_t) r.nextInt (100);
            o.detune = (uint8_t) r.nextInt (15);
        }
        for (int k = 0; k < 4; ++k) { p.pitchRate[(size_t) k] = (uint8_t) (50 + r.nextInt (50)); p.pitchLevel[(size_t) k] = (uint8_t) (44 + r.nextInt (13)); }
        p.pitchLevel[3] = 50;
        p.lfoSpeed = (uint8_t) r.nextInt (100); p.lfoDelay = (uint8_t) r.nextInt (60);
        p.lfoPmd = (uint8_t) r.nextInt (40); p.lfoAmd = (uint8_t) r.nextInt (40);
        p.lfoSync = 1; p.lfoWave = (uint8_t) r.nextInt (5); // not S/H: its random sequence differs by design
        p.lfoPms = (uint8_t) r.nextInt (8);
        p.transpose = (uint8_t) (18 + r.nextInt (13));
        p.setName ("ALG " + juce::String (algorithm + 1));
        return p;
    }

    struct Event { int t, note, vel; };

    static std::vector<float> renderDexedReference (const std::array<uint8_t, 156>& rawIn, const std::vector<Event>& evs, int total)
    {
        uint8_t raw[156];
        std::copy (rawIn.begin(), rawIn.end(), raw);
        FmCore core;
        Controllers c;
        c.pitch_range_up = c.pitch_range_dn = 3; c.pitch_step = 0; c.masterTune = 0;
        c.pitch_cc = 0x2000; c.modwheel_cc = c.foot_cc = c.breath_cc = c.aftertouch_cc = 0;
        c.portamento_enable_cc = false; c.portamento_cc = 0;
        c.core = &core;
        c.refresh();
        std::shared_ptr<TuningState> ts = createStandardTuning();
        struct V { std::unique_ptr<Dx7Note> n; int midi = -1; bool keydown = false, live = false; };
        std::array<V, 16> voices;
        for (auto& v : voices)
        {
            // zero-filled storage: Dx7Note leaves its feedback history uninitialised (see FmRenderer::FmRenderer)
            void* mem = ::operator new (sizeof (Dx7Note));
            std::memset (mem, 0, sizeof (Dx7Note));
            v.n.reset (new (mem) Dx7Note (ts, nullptr));
        }
        Lfo lfo;
        lfo.reset (raw + 137);
        int current = 0;
        const int transposeShift = raw[144] - 24;
        std::vector<float> out ((size_t) total, 0.0f);
        size_t ei = 0;
        AlignedBuf<int32_t, N> buf;
        for (int i = 0; i < total; i += N)
        {
            while (ei < evs.size() && evs[ei].t <= i)
            {
                const auto& ev = evs[ei++];
                const int pitch = ev.note + transposeShift;
                if (ev.vel > 0)
                {
                    bool trig = true;
                    for (auto& v : voices) if (v.keydown) { trig = false; c.mpeEnabled = false; }
                    auto& v = voices[(size_t) current];
                    current = (current + 1) % 16;
                    v.n->init (raw, pitch, ev.vel, 1, &c);
                    if (raw[136] != 0) v.n->oscSync();
                    v.midi = pitch; v.keydown = true; v.live = true;
                    if (trig) lfo.keydown();
                }
                else
                    for (auto& v : voices)
                        if (v.keydown && v.midi == pitch) { v.n->keyup(); v.keydown = false; break; }
            }
            float sum[N] = {};
            const int32_t lv = lfo.getsample(), ld = lfo.getdelay();
            for (auto& v : voices)
                if (v.live)
                {
                    for (int j = 0; j < N; ++j) buf.get()[j] = 0;
                    v.n->compute (buf.get(), lv, ld, &c);
                    for (int j = 0; j < N; ++j)
                    {
                        const int32_t val = buf.get()[j] >> 4;
                        const int cv = val < -(1 << 24) ? 0x8000 : val >= (1 << 24) ? 0x7fff : val >> 9; // Dexed as written
                        sum[j] += std::clamp ((float) cv / (float) 0x8000, -1.0f, 1.0f);
                    }
                }
            for (int j = 0; j < N && i + j < total; ++j) out[(size_t) (i + j)] = sum[j];
        }
        return out;
    }

    void runTest() override
    {
        beginTest ("FmRenderer output equals Dexed's Modern engine (factory voices, all 32 algorithms, chord)");
        fm::FmRenderer warmUp; // initialises MSFA's shared tables at 48 kHz (the reference uses the same tables)
        const auto dir = juce::SystemStats::getEnvironmentVariable ("VSX_DEXED_DIR", {});
        const juce::File d (dir);
        if (dir.isNotEmpty()) d.createDirectory();

        struct Case { juce::String id; fm::Patch p; std::vector<int> notes; int vel; };
        std::vector<Case> cases;
        int k = 0;
        for (const auto& v : fm::factoryVoices())
        {
            cases.push_back ({ "factory" + juce::String (k).paddedLeft ('0', 2), v, { 60 }, 100 });
            cases.push_back ({ "factory" + juce::String (k).paddedLeft ('0', 2) + "_low", v, { 36 }, 64 });
            ++k;
        }
        juce::Random r (2026);
        for (int a = 0; a < fm::kAlgorithms; ++a)
        {
            const auto p = randomPatch (r, a);
            cases.push_back ({ "alg" + juce::String (a + 1).paddedLeft ('0', 2), p, { 48 + r.nextInt (36) }, 20 + r.nextInt (108) });
        }
        // a chord on one patch (separate voices summed; the LFO is per voice here and global in Dexed, so LFO is off)
        auto chord = fm::factoryVoices().front();
        chord.lfoPmd = chord.lfoAmd = 0;
        cases.push_back ({ "chord", chord, { 48, 55, 60, 64, 67 }, 110 });

        const int total = 48000 * 3, offAt = 48000 * 2;
        std::vector<double> residuals;
        for (const auto& c : cases)
        {
            const auto raw = fm::FmRenderer::rawPatch (c.p, 1.0f);
            std::vector<Event> events;
            for (int n : c.notes) events.push_back ({ 0, n, c.vel });
            for (int n : c.notes) events.push_back ({ offAt, n, 0 });
            if (dir.isNotEmpty())
            {
                auto bin = d.getChildFile (c.id + ".bin"); bin.deleteFile(); bin.replaceWithData (raw.data(), raw.size());
                juce::String ev;
                for (const auto& e : events) ev << e.t << " " << e.note << " " << e.vel << "\n";
                d.getChildFile (c.id + ".txt").replaceWithText (ev);
            }

            std::vector<float> sum ((size_t) total, 0.0f);
            for (int n : c.notes)
            {
                fm::FmRenderer fr;
                fr.prepare (48000.0);
                fr.noteOn (c.p, n, c.vel, (float) n, 1);
                for (int i = 0; i < total; ++i)
                {
                    // the renderer computes MSFA in 64-sample blocks one output sample ahead (resampler priming), so a
                    // note-off issued one sample early lands on the same MSFA block boundary as Dexed's event at offAt
                    if (i == offAt - 1) fr.noteOff();
                    if (i % 64 == 0) fr.setPitch ((float) n, 1.0f);
                    sum[(size_t) i] += fr.render();
                }
            }
            if (dir.isNotEmpty())
            {
                auto out = d.getChildFile (c.id + ".vsx.f32"); out.deleteFile();
                out.replaceWithData (sum.data(), sum.size() * sizeof (float));
            }

            // compare: the renderer has one sample of latency (resampler priming)
            const auto ref = renderDexedReference (raw, events, total);
            double e2 = 0.0, r2 = 0.0, maxDiff = 0.0;
            for (int i = 0; i + 1 < total; ++i)
            {
                const double diff = (double) sum[(size_t) i + 1] - (double) ref[(size_t) i];
                e2 += diff * diff;
                r2 += (double) ref[(size_t) i] * ref[(size_t) i];
                maxDiff = std::max (maxDiff, std::abs (diff));
            }
            expect (r2 > 1.0e-6, c.id + ": the reference renders sound");
            const double res = 10.0 * std::log10 ((e2 + 1.0e-30) / (r2 + 1.0e-30));
            residuals.push_back (res);
            logMessage ("MEASURE|dexed|" + c.id + " residual vs Dexed Modern|" + juce::String (res, 1) + "|dB");
            logMessage ("MEASURE|dexed|" + c.id + " largest sample difference|" + juce::String (maxDiff * 32768.0, 2) + "|16-bit LSB");
            expect (res < -50.0, c.id + ": residual " + juce::String (res, 1) + " dB");
            expect (maxDiff * 32768.0 <= 2.01, c.id + ": largest difference " + juce::String (maxDiff * 32768.0, 2) + " LSB");
        }
        std::sort (residuals.begin(), residuals.end());
        const double median = residuals[residuals.size() / 2];
        logMessage ("MEASURE|dexed|median residual over " + juce::String ((int) residuals.size()) + " cases|" + juce::String (median, 1) + "|dB");
        expect (median < -100.0, "median residual " + juce::String (median, 1) + " dB");
        if (dir.isNotEmpty()) logMessage ("exported " + juce::String ((int) cases.size()) + " cases to " + d.getFullPathName());
    }
};

static DexedEquivalenceTests dexedEquivalenceTests;
} // namespace vsx
