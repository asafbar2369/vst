#pragma once
// A SYNTHETIC wave ROM in the JD-800 chip layout, generated here from sine / saw maths for the tests.
// It contains no Roland data: it only exercises the importer with the same structure the real chips
// have (header, sample table, multisample table, delta coded data with per-16-byte shifts, scrambled
// dump). Plain C++ (no JUCE).

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "JdRomImport.h"

namespace jdfixture
{
struct SampleSpec
{
    int rootKey = 60, mode = 0;             // mode: 0 loop the tail, 1 long loop, 2 one shot
    std::vector<int> pcm;                   // target values, full scale = +-65536
    int loopLength = 0;
    uint32_t forceStart = 0;                // 0 = next 16-byte aligned address; otherwise this (may be unaligned)
};
struct GroupSpec
{
    std::string name;
    std::vector<int> samples;               // indices into the chip's sample list
    std::vector<int> upper;                 // key limits, one fewer than samples
};

inline void put16 (std::vector<uint8_t>& b, size_t o, uint32_t v) { b[o] = uint8_t (v >> 8); b[o + 1] = uint8_t (v); }
inline void put24 (std::vector<uint8_t>& b, size_t o, uint32_t v) { b[o] = uint8_t (v >> 16); put16 (b, o + 1, v); }
inline void put32 (std::vector<uint8_t>& b, size_t o, uint32_t v) { put16 (b, o, v >> 16); put16 (b, o + 2, v); }

// Delta-encodes every sample into the readable 1 MiB image.
inline std::vector<uint8_t> buildImage (const std::vector<SampleSpec>& samples, const std::vector<GroupSpec>& groups)
{
    constexpr size_t N = jdrom::kChipBytes;
    std::vector<uint8_t> img (N, 0);
    std::vector<int> target (N, 0);
    std::vector<uint8_t> used (N, 0), reset (N, 0);
    std::vector<uint32_t> starts;
    uint32_t next = 0x8000;
    for (const auto& s : samples)
    {
        const uint32_t start = s.forceStart != 0 ? s.forceStart : next;
        starts.push_back (start);
        reset[start] = 1;
        for (size_t j = 0; j < s.pcm.size(); ++j) { target[start + j] = s.pcm[j]; used[start + j] = 1; }
        next = std::max (next, (uint32_t) ((start + s.pcm.size() + 15u) & ~15u));
    }
    // encode block by block (16 bytes share one 4-bit shift)
    int64_t acc = 0;
    for (size_t block = 0; block < 0xF0000; block += 16)
    {
        bool any = false;
        for (size_t k = 0; k < 16; ++k) any = any || used[block + k];
        if (! any) continue;
        int chosen = 15;
        int64_t accStart = acc;
        for (int shift = 0; shift <= 15; ++shift)
        {
            int64_t a = accStart;
            bool ok = true;
            for (size_t k = 0; k < 16 && ok; ++k)
            {
                const size_t addr = block + k;
                if (! used[addr]) continue;
                if (reset[addr]) a = 0;
                const int64_t q = std::llround (double (target[addr] - a) / double ((int64_t) 1 << shift));
                if (q < -127 || q > 127) ok = false; else a += q * ((int64_t) 1 << shift);
            }
            if (ok) { chosen = shift; break; }
        }
        for (size_t k = 0; k < 16; ++k)
        {
            const size_t addr = block + k;
            if (! used[addr]) continue;
            if (reset[addr]) acc = 0;
            int64_t q = std::llround (double (target[addr] - acc) / double ((int64_t) 1 << chosen));
            q = std::max<int64_t> (-127, std::min<int64_t> (127, q));
            acc += q * ((int64_t) 1 << chosen);
            img[addr] = (uint8_t) (int8_t) q;
        }
        uint8_t& packed = img[block >> 5];
        packed = (block & 0x10) ? uint8_t ((packed & 0x0F) | (chosen << 4)) : uint8_t ((packed & 0xF0) | chosen);
    }
    // tables near the end of the chip, like the real ones
    const uint32_t p80 = 0xFE000, p84 = 0xFF000;
    put16 (img, 0x60, (uint32_t) samples.size());
    put16 (img, 0x62, (uint32_t) groups.size());
    put32 (img, 0x80, p80);
    put32 (img, 0x84, p84);
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const auto& s = samples[i];
        const size_t r = p80 + 16 * i;
        img[r] = 0x7F;
        put24 (img, r + 1, starts[i]);
        put32 (img, r + 4, (uint32_t) s.pcm.size());
        put16 (img, r + 8, (uint32_t) (s.mode == 2 ? 0 : s.loopLength));
        img[r + 10] = (uint8_t) s.mode;
        img[r + 11] = (uint8_t) s.rootKey;
        put16 (img, r + 12, 1066);   // tuning word fields: 1066 + 1024 = 2090 means "exactly at the root key" (jdrom::kTuneZero)
        put16 (img, r + 14, 1024);
    }
    for (size_t i = 0; i < groups.size(); ++i)
    {
        const size_t r = p84 + 60 * i;
        std::memset (&img[r], ' ', 12);
        std::memcpy (&img[r], groups[i].name.data(), std::min<size_t> (12, groups[i].name.size()));
        for (size_t k = 0; k < 16; ++k) img[r + 12 + k] = k < groups[i].upper.size() ? (uint8_t) groups[i].upper[k] : 0x7F;
        for (size_t k = 0; k < 16; ++k) put16 (img, r + 28 + 2 * k, k < groups[i].samples.size() ? (uint32_t) groups[i].samples[k] : 0xFFFFu);
    }
    return img;
}

inline std::vector<uint8_t> scrambled (const std::vector<uint8_t>& image)
{
    std::vector<uint8_t> raw (image.size());
    jdrom::scramble (image.data(), raw.data(), image.size());
    return raw;
}

// `cycles` periods of a sine of amplitude `amp` (full scale 65536) in exactly `length` samples.
inline std::vector<int> sineCycles (int length, int cycles, double amp = 32768.0)
{
    std::vector<int> v ((size_t) length);
    for (int i = 0; i < length; ++i) v[(size_t) i] = (int) std::lround (amp * std::sin (2.0 * 3.14159265358979 * cycles * i / length));
    return v;
}

// A one-shot attack burst followed by `tailCycles` cycles of a sine: a typical "sample with a loop at the end".
inline SampleSpec loopedTone (int rootKey, int periodSamples, int totalPeriods, int loopPeriods)
{
    SampleSpec s;
    s.rootKey = rootKey;
    s.mode = 0;
    s.loopLength = periodSamples * loopPeriods;
    const int length = periodSamples * totalPeriods;
    s.pcm = sineCycles (length, totalPeriods);
    return s;
}

// Frequency of MIDI note n at A4 = 440.
inline double noteHz (int n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }

// A looped sine whose pitch equals root key `key` as closely as an integer loop length allows.
inline SampleSpec pitchedTone (int key, int loopPeriods = 24, double amp = 30000.0)
{
    const double period = jdrom::kDataRate / noteHz (key);
    const int loopLen = (int) std::lround (period * loopPeriods);
    const int lead = loopLen;                      // one loop of lead-in, then the sustained loop
    SampleSpec s;
    s.rootKey = key;
    s.mode = 0;
    s.loopLength = loopLen;
    // total length = lead + loopLen; the sine has exactly loopPeriods cycles per loopLen samples
    const int total = lead + loopLen;
    s.pcm.resize ((size_t) total);
    for (int i = 0; i < total; ++i)
        s.pcm[(size_t) i] = (int) std::lround (amp * std::sin (2.0 * 3.14159265358979 * loopPeriods * i / loopLen));
    return s;
}
} // namespace jdfixture
