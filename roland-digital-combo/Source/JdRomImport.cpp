#include "JdRomImport.h"
#include "JD800SysEx.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace jdrom
{
const KnownChip kKnownChips[3] = {
    { 0xd743cec8u, "PCM A (2A6 / IC25)" },
    { 0x06098658u, "PCM B (2A7 / IC26)" },
    { 0x5c83e539u, "PCM C (2A8 / IC1)" },
};

uint32_t crc32 (const uint8_t* data, size_t size) noexcept
{
    static const std::array<uint32_t, 256> table = []
    {
        std::array<uint32_t, 256> t {};
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

int identifyKnownChip (const uint8_t* raw, size_t size) noexcept
{
    if (size != kChipBytes) return -1;
    const uint32_t c = crc32 (raw, size);
    for (int i = 0; i < 3; ++i)
        if (kKnownChips[i].crc32 == c) return i;
    return -1;
}

// ---- line scrambling ---------------------------------------------------------------------------
// Address bit j of the readable image comes from address bit kAddr[j] of the dump; data bit j of
// the readable byte comes from bit kData[j] of the dump byte. Bits above 19 are unchanged.
namespace
{
const int kAddr[20] = { 2, 0, 3, 4, 1, 9, 13, 10, 18, 17, 6, 15, 11, 16, 8, 5, 12, 7, 14, 19 };
const int kData[8]  = { 2, 0, 4, 5, 7, 6, 3, 1 };

struct Luts
{
    uint32_t addrLo[1024], addrHi[1024];
    uint8_t dataFwd[256], dataInv[256];
    Luts()
    {
        for (uint32_t v = 0; v < 1024; ++v)
        {
            uint32_t lo = 0, hi = 0;
            for (int j = 0; j < 10; ++j)
            {
                if (v & (1u << j)) lo |= 1u << kAddr[j];
                if (v & (1u << j)) hi |= 1u << kAddr[j + 10];
            }
            addrLo[v] = lo;
            addrHi[v] = hi;
        }
        for (int v = 0; v < 256; ++v)
        {
            int f = 0, inv = 0;
            for (int j = 0; j < 8; ++j)
            {
                if (v & (1 << kData[j])) f |= 1 << j;     // dump byte -> readable byte
                if (v & (1 << j)) inv |= 1 << kData[j];   // readable byte -> dump byte
            }
            dataFwd[v] = (uint8_t) f;
            dataInv[v] = (uint8_t) inv;
        }
    }
};
const Luts& luts() { static const Luts l; return l; }
inline uint32_t mapAddr (const Luts& l, uint32_t i) noexcept { return (i & ~0xFFFFFu) | l.addrLo[i & 1023u] | l.addrHi[(i >> 10) & 1023u]; }

inline uint32_t be16 (const uint8_t* p) noexcept { return (uint32_t) p[0] << 8 | p[1]; }
inline uint32_t be24 (const uint8_t* p) noexcept { return (uint32_t) p[0] << 16 | (uint32_t) p[1] << 8 | p[2]; }
inline uint32_t be32 (const uint8_t* p) noexcept { return (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 8 | p[3]; }
} // namespace

void descramble (const uint8_t* raw, uint8_t* out, size_t size) noexcept
{
    const auto& l = luts();
    for (size_t i = 0; i < size; ++i)
        out[i] = l.dataFwd[raw[mapAddr (l, (uint32_t) i)]];
}

void scramble (const uint8_t* image, uint8_t* out, size_t size) noexcept
{
    const auto& l = luts();
    for (size_t i = 0; i < size; ++i)
        out[mapAddr (l, (uint32_t) i)] = l.dataInv[image[i]];
}

// ---- tables ------------------------------------------------------------------------------------
bool parseTables (const uint8_t* img, size_t size, ChipTables& out, std::string& why)
{
    out = {};
    if (size != kChipBytes) { why = "not 1 MiB"; return false; }
    const uint32_t ns = be16 (img + 0x60), ng = be16 (img + 0x62), p80 = be32 (img + 0x80), p84 = be32 (img + 0x84);
    if (ns == 0 || ns > 1024 || ng == 0 || ng > 128) { why = "sample / group counts out of range"; return false; }
    if (p80 < 0x100 || p84 < 0x100 || p80 + 16u * ns > size || p84 + 60u * ng > size) { why = "table pointers outside the chip"; return false; }

    out.samples.reserve (ns);
    for (uint32_t i = 0; i < ns; ++i)
    {
        const uint8_t* r = img + p80 + 16u * i;
        SampleRecord s;
        s.byte0 = r[0];
        s.start = be24 (r + 1);
        s.length = be32 (r + 4);
        s.loopLength = be16 (r + 8);
        s.loopMode = r[10];
        s.rootKey = r[11];
        s.field12 = (uint16_t) be16 (r + 12);
        s.field14 = (uint16_t) be16 (r + 14);
        if (s.start < 0x8000u || s.length == 0 || (uint64_t) s.start + s.length > size) { why = "sample " + std::to_string (i) + " lies outside the wave area"; return false; }
        if (s.loopMode > 2 || s.loopLength > s.length || s.rootKey > 127) { why = "sample " + std::to_string (i) + " has invalid loop / key fields"; return false; }
        out.samples.push_back (s);
    }
    out.groups.reserve (ng);
    for (uint32_t i = 0; i < ng; ++i)
    {
        const uint8_t* r = img + p84 + 60u * i;
        GroupRecord g;
        for (int k = 0; k < 12; ++k)
        {
            const char ch = (char) r[k];
            if (ch < 32 || ch > 126) { why = "group " + std::to_string (i) + " name is not printable text"; return false; }
            g.name.push_back (ch);
        }
        while (! g.name.empty() && g.name.back() == ' ') g.name.pop_back();
        for (int k = 0; k < 16; ++k)
        {
            const uint16_t idx = (uint16_t) be16 (r + 28 + 2 * k);
            if (idx == 0xFFFFu) continue;
            if (idx >= ns) { why = "group '" + g.name + "' points at a missing sample"; return false; }
            g.samples.push_back (idx);
        }
        if (g.samples.empty()) { why = "group '" + g.name + "' has no samples"; return false; }
        for (size_t k = 0; k + 1 < g.samples.size(); ++k)
            g.upperKey.push_back (std::min<uint8_t> (r[12 + k], 127));
        for (size_t k = 1; k < g.upperKey.size(); ++k)
            if (g.upperKey[k] < g.upperKey[k - 1]) { why = "group '" + g.name + "' key limits are not ascending"; return false; }
        out.groups.push_back (std::move (g));
    }
    return true;
}

double sampleTuneCents (const SampleRecord& r) noexcept
{
    const double cents = kTuneCentsPerUnit * (kTuneZero - double (r.field12) - double (r.field14));
    return (cents > 150.0 || cents < -150.0) ? 0.0 : cents;
}

void decodeSample (const uint8_t* img, const SampleRecord& r, std::vector<float>& out)
{
    out.assign (r.length, 0.0f);
    int64_t acc = 0;
    for (uint32_t j = 0; j < r.length; ++j)
    {
        const uint32_t a = r.start + j;
        const int delta = (int8_t) img[a];
        const uint8_t packed = img[((a & 0xFFFFFu) >> 5) | (a & 0xF00000u)];   // one 4-bit shift per 16 bytes
        const int shift = (a & 0x10u) ? (packed >> 4) : (packed & 15);
        acc += (int64_t) delta * ((int64_t) 1 << shift);
        acc = std::max<int64_t> (-131072, std::min<int64_t> (131072, acc));
        out[j] = (float) acc * kGain;
    }
    // The chip's accumulator drifts: remove the mean of the part that sustains (the loop, or the tail of a
    // one shot) so loops do not thump, and ease in the first samples if that moved the start.
    const bool looped = r.loopMode != 2 && r.loopLength >= 2;
    const size_t len = out.size();
    const size_t from = looped ? len - r.loopLength : len - std::max<size_t> (16, len / 8);
    double mean = 0.0;
    for (size_t i = from; i < len; ++i) mean += out[i];
    mean /= double (len - from);
    for (auto& v : out) v -= (float) mean;
    if (std::abs (mean) > 0.004)
        for (size_t i = 0; i < std::min<size_t> (16, len); ++i)
            out[i] *= float (i + 1) / 16.0f;
}

// ---- names -------------------------------------------------------------------------------------
namespace
{
std::string normalise (const std::string& s)
{
    std::string n;
    for (char c : s)
    {
        if (! std::isalnum ((unsigned char) c)) continue;
        c = (char) std::tolower ((unsigned char) c);
        if (c == 'l') c = 'i';          // the ROM writes "Pole lp" where the manual's print reads "Pole Ip"
        n.push_back (c);
    }
    if (n == "tabia") n = "tabie";      // "Tabla" (ROM) / "Table" (an older transcription), after the l -> i fold
    if (n == "zzzvoz") n = "zzzvox";
    return n;
}
} // namespace

int waveIndexForName (const std::string& romName)
{
    static const std::vector<std::pair<std::string, int>> names = []
    {
        std::vector<std::pair<std::string, int>> v;
        for (int i = 0; i < jd800::kNumWaveforms; ++i) v.emplace_back (normalise (jd800::kWaveNames[i]), i);
        return v;
    }();
    const auto key = normalise (romName);
    for (const auto& n : names)
        if (n.first == key) return n.second;
    return -1;
}

// ---- building the wave set --------------------------------------------------------------------
bool addImage (const uint8_t* img, size_t size, const std::string& label, dc::UserWaveSet& set, Report& report)
{
    ChipTables t;
    std::string why;
    if (! parseTables (img, size, t, why))
    {
        report.messages.push_back (label + ": not a JD-800 wave ROM (" + why + ")");
        ++report.chipsRejected;
        return false;
    }
    std::vector<dc::SampleWave*> built (t.samples.size(), nullptr);
    int filled = 0;
    for (const auto& g : t.groups)
    {
        const int wave = waveIndexForName (g.name);
        if (wave < 0)
        {
            report.messages.push_back (label + ": multisample '" + g.name + "' is not one of the 108 JD-800 waveforms (skipped)");
            continue;
        }
        if (! set.jd[wave].empty() || set.hasRomJd (wave))
            continue;   // the user's own wave file (or an earlier chip) already provides this waveform
        std::vector<dc::UserWaveSet::Zone> zones;
        for (size_t k = 0; k < g.samples.size(); ++k)
        {
            const uint16_t si = g.samples[k];
            if (built[si] == nullptr)
            {
                const auto& rec = t.samples[si];
                auto w = std::make_unique<dc::SampleWave>();
                decodeSample (img, rec, w->data);
                w->rate = (float) kDataRate;
                w->rootHz = 440.0f * std::exp2 ((float (rec.rootKey) - 69.0f + float (sampleTuneCents (rec)) * 0.01f) / 12.0f);
                if (rec.loopMode != 2 && rec.loopLength >= 2)
                {
                    w->loop = true;
                    w->loopStart = (int) (rec.length - rec.loopLength);
                    w->loopEnd = (int) rec.length;
                }
                w->buildMips();
                built[si] = w.get();
                set.romSamples.push_back (std::move (w));
                ++report.samplesDecoded;
            }
            dc::UserWaveSet::Zone z;
            z.highKey = k < g.upperKey.size() ? (int) g.upperKey[k] : 127;
            z.wave = built[si];
            zones.push_back (z);
        }
        set.jdZones[wave] = std::move (zones);
        ++filled;
    }
    report.wavesFilled += filled;
    ++report.chipsUsed;
    return filled > 0;
}

bool addChip (const uint8_t* data, size_t size, const std::string& label, dc::UserWaveSet& set, Report& report)
{
    if (size != kChipBytes)
    {
        report.messages.push_back (label + ": skipped (a JD-800 wave ROM chip is exactly 1 MiB, this file is " + std::to_string (size) + " bytes)");
        return false;
    }
    const int known = identifyKnownChip (data, size);
    std::vector<uint8_t> image (size);
    ChipTables probe;
    std::string why;
    if (known >= 0)
    {
        descramble (data, image.data(), size);
        ++report.chipsVerified;
        report.messages.push_back (label + ": recognised as " + kKnownChips[known].label + " (CRC-32 matches the published dump)");
    }
    else
    {
        // not a known dump: accept it only if its structure is valid, readable as is or after descrambling
        if (parseTables (data, size, probe, why))
            std::memcpy (image.data(), data, size);
        else
            descramble (data, image.data(), size);
        report.messages.push_back (label + ": CRC-32 is not one of the published dumps, structure checked instead");
    }
    return addImage (image.data(), size, label, set, report);
}
} // namespace jdrom
