#include "AudioFeatures.h"
#include "Pitch.h"
#include "../Core/DspUtils.h"
#include <juce_dsp/juce_dsp.h>

namespace vsx
{
namespace
{
    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        std::nth_element (v.begin(), v.begin() + (long) (v.size() / 2), v.end());
        return v[v.size() / 2];
    }

    float bandEdge (int i) { return 40.0f * std::pow (18000.0f / 40.0f, (float) i / (float) AudioFeatures::kBands); }

    /** Dominant oscillation rate (Hz) and depth of a control signal sampled at rate fs (via autocorrelation). */
    void modulationRate (const std::vector<float>& x, float fs, float minHz, float maxHz, float& rate, float& depth)
    {
        rate = depth = 0.0f;
        if (x.size() < 16) return;
        float mean = 0.0f;
        for (float v : x) mean += v;
        mean /= (float) x.size();
        std::vector<float> d (x.size());
        float var = 0.0f;
        for (size_t i = 0; i < x.size(); ++i) { d[i] = x[i] - mean; var += d[i] * d[i]; }
        if (var <= 1.0e-12f) return;
        const int minLag = std::max (1, (int) (fs / maxHz)), maxLag = std::min ((int) x.size() / 2, (int) (fs / minHz));
        float best = 0.0f; int bestLag = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            float acc = 0.0f;
            for (size_t i = 0; i + (size_t) lag < d.size(); ++i) acc += d[i] * d[i + (size_t) lag];
            acc /= var;
            if (acc > best) { best = acc; bestLag = lag; }
        }
        if (bestLag > 0 && best > 0.3f)
        {
            rate = fs / (float) bestLag;
            depth = std::sqrt (var / (float) x.size()) * 1.414f;
        }
    }
}

AudioFeatures AudioFeatures::analyse (const juce::AudioBuffer<float>& audio, double sr, double noteOff)
{
    AudioFeatures f;
    if (! std::isfinite (sr) || sr < 1000.0 || sr > 768000.0)
        return f;
    f.sampleRate = sr;
    const int n = audio.getNumSamples();
    const int chans = audio.getNumChannels();
    f.duration = n / sr;
    if (n < 2048 || chans < 1) return f;
    if (! std::isfinite (noteOff) || noteOff <= 0.0) noteOff = -1.0;
    else noteOff = std::min (noteOff, f.duration);
    const float* L = audio.getReadPointer (0);
    const float* R = audio.getReadPointer (chans > 1 ? 1 : 0);
    std::vector<float> mono ((size_t) n);
    double sumSq = 0.0, sumMid = 0.0, sumSide = 0.0, sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) return f;
        const float m = 0.5f * L[i] + 0.5f * R[i], s = 0.5f * L[i] - 0.5f * R[i];
        mono[(size_t) i] = m;
        sumSq += (double) m * m;
        sumMid += (double) m * m; sumSide += (double) s * s;
        sumLR += (double) L[i] * R[i]; sumLL += (double) L[i] * L[i]; sumRR += (double) R[i] * R[i];
        peak = std::max ({ peak, std::abs (L[i]), std::abs (R[i]) });
    }
    const float rms = (float) std::sqrt (sumSq / n);
    f.rmsDb = gainToDb (rms);
    f.peakDb = gainToDb (peak);
    f.crestDb = f.peakDb - f.rmsDb;
    f.stereoWidth = sumMid > 1.0e-12 ? (float) std::sqrt (sumSide / sumMid) : 0.0f;
    f.correlation = (sumLL > 1.0e-12 && sumRR > 1.0e-12) ? (float) (sumLR / std::sqrt (sumLL * sumRR)) : 1.0f;
    if (peak < 1.0e-5f) return f;

    // ---- envelope (100 Hz RMS) --------------------------------------------------------
    const int hopEnv = (int) (sr / 100.0);
    std::vector<float> env;
    for (int i = 0; i + hopEnv <= n; i += hopEnv)
    {
        double s = 0.0;
        for (int k = 0; k < hopEnv; ++k) s += (double) mono[(size_t) (i + k)] * mono[(size_t) (i + k)];
        env.push_back ((float) std::sqrt (s / hopEnv));
    }
    if (env.empty()) return f;
    const float envPeak = *std::max_element (env.begin(), env.end());
    for (auto& e : env) e /= std::max (envPeak, 1.0e-9f);
    f.envelope = env;
    const auto peakIt = std::max_element (env.begin(), env.end());
    const int peakIdx = (int) (peakIt - env.begin());
    int i10 = 0, i90 = peakIdx;
    for (int i = 0; i <= peakIdx; ++i) if (env[(size_t) i] >= 0.1f) { i10 = i; break; }
    for (int i = i10; i <= peakIdx; ++i) if (env[(size_t) i] >= 0.9f) { i90 = i; break; }
    f.attack = std::max (0.001f, (float) (i90 - i10) / 100.0f);

    const int offIdx = noteOff > 0.0 ? std::min ((int) env.size() - 1, (int) (noteOff * 100.0)) : (int) env.size() - 1;
    const int sustainStart = std::min (peakIdx, offIdx);
    // sustain: median level over the last 40% before note-off
    std::vector<float> sus;
    for (int i = sustainStart + (offIdx - sustainStart) * 6 / 10; i < offIdx; ++i) sus.push_back (env[(size_t) i]);
    f.sustain = sus.empty() ? env[(size_t) offIdx] : median (sus);
    // decay: time from peak to within 10% (of the drop) of the sustain level
    const float target = f.sustain + 0.1f * (1.0f - f.sustain);
    int idec = peakIdx;
    while (idec < offIdx && env[(size_t) idec] > target) ++idec;
    f.decay = (float) std::max (0, idec - peakIdx) / 100.0f;
    // release: time from note-off to -40 dB of the note-off level
    if (noteOff > 0.0 && offIdx < (int) env.size() - 1)
    {
        const float ref = std::max (env[(size_t) offIdx], 1.0e-6f);
        int ir = offIdx;
        while (ir < (int) env.size() - 1 && env[(size_t) ir] > ref * 0.01f) ++ir;
        f.release = (float) (ir - offIdx) / 100.0f;
    }
    // onsets: rises of > 6 dB within 30 ms
    for (size_t i = 3; i < env.size(); ++i)
        if (env[i] > 0.05f && env[i] > env[i - 3] * 2.0f && (i < 6 || env[i - 3] <= env[i - 6] * 1.5f)) { ++f.onsets; i += 5; }
    // tremolo over the sustained part
    {
        std::vector<float> part;
        for (int i = peakIdx + 5; i < offIdx; ++i) part.push_back (env[(size_t) i]);
        float rate, depth;
        modulationRate (part, 100.0f, 1.0f, 15.0f, rate, depth);
        f.tremoloRate = rate;
        f.tremoloDepth = std::min (1.0f, depth / std::max (0.05f, f.sustain));
    }
    float minEnv = 1.0f;
    for (int i = peakIdx; i < offIdx; ++i) minEnv = std::min (minEnv, std::max (env[(size_t) i], 1.0e-4f));
    f.dynamicRangeDb = -gainToDb (minEnv);

    // ---- spectral frames ---------------------------------------------------------------
    const int order = 12, fftN = 1 << order, hop = fftN / 4;
    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) fftN), buf ((size_t) fftN * 2), avgMag ((size_t) fftN / 2, 0.0f);
    for (int i = 0; i < fftN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) (fftN - 1));
    const float binHz = (float) (sr / fftN);
    std::vector<float> centroids, pitches;
    float flatSum = 0.0f; int frames = 0;
    const int analysisEnd = noteOff > 0.0 ? std::min (n, (int) (noteOff * sr)) : n;
    for (int start = 0; start + fftN <= std::max (analysisEnd, fftN); start += hop)
    {
        if (start + fftN > n) break;
        std::fill (buf.begin(), buf.end(), 0.0f);
        float frameEnergy = 0.0f;
        for (int i = 0; i < fftN; ++i) { buf[(size_t) i] = mono[(size_t) (start + i)] * win[(size_t) i]; frameEnergy += buf[(size_t) i] * buf[(size_t) i]; }
        if (frameEnergy < 1.0e-8f) continue;
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        double num = 0.0, den = 0.0, logSum = 0.0, linSum = 0.0;
        int cnt = 0;
        for (int b = 1; b < fftN / 2; ++b)
        {
            const float m = buf[(size_t) b];
            avgMag[(size_t) b] += m;
            const float hz = (float) b * binHz;
            num += (double) hz * m; den += m;
            if (hz > 60.0f && hz < 16000.0f) { logSum += std::log (m + 1.0e-9); linSum += m; ++cnt; }
        }
        if (den > 0.0) centroids.push_back ((float) (num / den));
        if (cnt > 0 && linSum > 0.0) flatSum += (float) (std::exp (logSum / cnt) / (linSum / cnt));
        ++frames;

        float conf = 0.0f;
        const float period = yinPeriod (mono.data() + start, std::min (fftN, 3072), sr, 40.0f, 2000.0f, 0.15f, &conf);
        if (period > 0.0f && conf > 0.6f) pitches.push_back ((float) (sr / period));
    }
    if (frames == 0) return f;
    f.flatness = flatSum / (float) frames;
    if (! centroids.empty())
    {
        double s = 0.0;
        for (float c : centroids) s += c;
        f.centroid = (float) (s / centroids.size());
        const size_t k = std::max<size_t> (1, centroids.size() / 5);
        double a = 0.0, b = 0.0;
        for (size_t i = 0; i < k; ++i) { a += centroids[i]; b += centroids[centroids.size() - 1 - i]; }
        f.centroidStart = (float) (a / k);
        f.centroidEnd = (float) (b / k);
    }

    // rolloff 85 % and band energies
    double total = 0.0;
    for (int b = 1; b < fftN / 2; ++b) total += (double) avgMag[(size_t) b] * avgMag[(size_t) b];
    double acc = 0.0;
    for (int b = 1; b < fftN / 2; ++b)
    {
        acc += (double) avgMag[(size_t) b] * avgMag[(size_t) b];
        if (acc >= 0.85 * total) { f.rolloff = (float) b * binHz; break; }
    }
    float maxBand = -200.0f;
    for (int i = 0; i < kBands; ++i)
    {
        const int b0 = std::max (1, (int) (bandEdge (i) / binHz)), b1 = std::max (b0 + 1, (int) (bandEdge (i + 1) / binHz));
        double e = 0.0;
        for (int b = b0; b < b1 && b < fftN / 2; ++b) e += (double) avgMag[(size_t) b] * avgMag[(size_t) b];
        f.bandsDb[(size_t) i] = 10.0f * std::log10 ((float) (e / (b1 - b0)) + 1.0e-12f);
        maxBand = std::max (maxBand, f.bandsDb[(size_t) i]);
    }
    for (auto& b : f.bandsDb) b = std::max (-80.0f, b - maxBand);

    // pitch, vibrato and harmonics
    if (pitches.size() >= 3 && (float) pitches.size() > 0.3f * (float) frames)
    {
        f.f0 = median (pitches);
        f.pitchConfidence = (float) pitches.size() / (float) frames;
        f.pitchGlideSemis = 12.0f * std::log2 (pitches.back() / pitches.front());
        std::vector<float> cents;
        for (float p : pitches) cents.push_back (1200.0f * std::log2 (p / f.f0));
        float rate, depth;
        modulationRate (cents, (float) (sr / hop), 2.0f, 12.0f, rate, depth);
        f.vibratoRate = rate;
        f.vibratoDepthCents = depth;

        float maxH = -200.0f;
        for (int h = 1; h <= kHarmonics; ++h)
        {
            const float hz = f.f0 * (float) h;
            float m = 0.0f;
            if (hz < sr * 0.5 - binHz * 2)
            {
                const int c = (int) std::round (hz / binHz);
                for (int b = std::max (1, c - 2); b <= std::min (fftN / 2 - 1, c + 2); ++b) m = std::max (m, avgMag[(size_t) b]);
            }
            f.harmonicsDb[(size_t) h - 1] = gainToDb (m + 1.0e-9f);
            maxH = std::max (maxH, f.harmonicsDb[(size_t) h - 1]);
        }
        for (auto& h : f.harmonicsDb) h = std::max (-90.0f, h - maxH);
    }
    else
        f.harmonicsDb.fill (-90.0f);

    f.valid = true;
    return f;
}

juce::String AudioFeatures::describe() const
{
    if (! valid) return "No usable audio (silent or too short).";
    juce::String s;
    s << "duration " << juce::String (duration, 2) << " s, RMS " << juce::String (rmsDb, 1) << " dBFS, crest " << juce::String (crestDb, 1) << " dB\n";
    if (f0 > 0.0f)
        s << "pitch " << juce::String (f0, 1) << " Hz (MIDI " << juce::String (hzToMidi (f0), 1) << "), confidence " << juce::String (pitchConfidence, 2)
          << ", glide " << juce::String (pitchGlideSemis, 1) << " st, vibrato " << juce::String (vibratoRate, 1) << " Hz / "
          << juce::String (vibratoDepthCents, 0) << " cents\n";
    else
        s << "pitch: unpitched / noisy\n";
    s << "brightness: centroid " << juce::String ((int) centroid) << " Hz (start " << (int) centroidStart << " -> end " << (int) centroidEnd
      << "), rolloff85 " << (int) rolloff << " Hz, noisiness (spectral flatness) " << juce::String (flatness, 3) << "\n";
    if (f0 > 0.0f)
    {
        juce::StringArray h;
        for (int i = 0; i < 12; ++i) h.add (juce::String (harmonicsDb[(size_t) i], 0));
        s << "harmonics dB (1..12): " << h.joinIntoString (" ") << "\n";
    }
    juce::StringArray b;
    for (int i = 0; i < kBands; i += 2) b.add (juce::String ((int) bandEdge (i)) + "Hz:" + juce::String (bandsDb[(size_t) i], 0));
    s << "spectral envelope: " << b.joinIntoString (" ") << "\n";
    s << "envelope: attack " << juce::String (attack * 1000.0f, 0) << " ms, decay " << juce::String (decay, 2) << " s, sustain "
      << juce::String (sustain, 2) << ", release " << juce::String (release, 2) << " s, tremolo " << juce::String (tremoloRate, 1) << " Hz depth "
      << juce::String (tremoloDepth, 2) << ", onsets " << onsets << "\n";
    s << "stereo width " << juce::String (stereoWidth, 2) << ", L/R correlation " << juce::String (correlation, 2);
    return s;
}

float AudioFeatures::distance (const AudioFeatures& a, const AudioFeatures& b)
{
    if (! a.valid || ! b.valid) return 10.0f;
    float d = 0.0f;
    float band = 0.0f;
    for (int i = 0; i < kBands; ++i) band += std::abs (a.bandsDb[(size_t) i] - b.bandsDb[(size_t) i]);
    d += 1.0f * band / (float) kBands / 12.0f;

    if (a.f0 > 0.0f && b.f0 > 0.0f)
    {
        float h = 0.0f;
        for (int i = 0; i < 16; ++i) h += std::abs (std::max (-60.0f, a.harmonicsDb[(size_t) i]) - std::max (-60.0f, b.harmonicsDb[(size_t) i]));
        d += 0.6f * h / 16.0f / 15.0f;
        d += 0.2f * std::abs (a.vibratoDepthCents - b.vibratoDepthCents) / 50.0f;
    }
    else if ((a.f0 > 0.0f) != (b.f0 > 0.0f))
        d += 0.5f;

    d += 0.5f * std::abs (std::log2 (std::max (50.0f, a.centroid) / std::max (50.0f, b.centroid)));
    d += 0.4f * std::abs (a.flatness - b.flatness) * 4.0f;

    // envelope shape (resampled to a common length)
    if (! a.envelope.empty() && ! b.envelope.empty())
    {
        const int m = 64;
        float e = 0.0f;
        for (int i = 0; i < m; ++i)
        {
            const float ia = (float) i / (m - 1) * (float) (a.envelope.size() - 1);
            const float ib = (float) i / (m - 1) * (float) (b.envelope.size() - 1);
            e += std::abs (a.envelope[(size_t) ia] - b.envelope[(size_t) ib]);
        }
        d += 1.0f * e / (float) m;
    }
    d += 0.3f * std::abs (std::log2 (std::max (0.002f, a.attack) / std::max (0.002f, b.attack))) / 3.0f;
    d += 0.2f * std::abs (a.stereoWidth - b.stereoWidth);
    return d;
}

} // namespace vsx
