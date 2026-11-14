#define _CRT_SECURE_NO_WARNINGS   // std::getenv below (MSVC otherwise warns)
// Portable (no JUCE) tests of the JD-800 wave ROM importer. They use a SYNTHETIC ROM generated in
// JdRomFixture.h. If the environment variable DIGITALCOMBO_JD800_ROM_DIR points at a folder holding
// the user's own dumps, the same code is also run against them (never in CI: the dumps are not in
// the repository); otherwise that part is reported as SKIPPED, not as passed.

#include "JD800SysEx.h"
#include "JdRomFixture.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <random>

static int passed = 0, failed = 0;
static void check (bool ok, const std::string& what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    ok ? ++passed : ++failed;
}

using namespace jdfixture;

static std::vector<uint8_t> syntheticChipA()
{
    std::vector<SampleSpec> s;
    s.push_back (pitchedTone (48));                      // 0 \ "Syn Saw 1": three zones
    s.push_back (pitchedTone (60));                      // 1 |
    s.push_back (pitchedTone (72));                      // 2 /
    SampleSpec shot;                                     // 3 one shot: a decaying burst
    shot.rootKey = 64; shot.mode = 2;
    for (int i = 0; i < 700; ++i) shot.pcm.push_back ((int) std::lround (40000.0 * std::exp (-i / 120.0) * std::sin (i * 0.31)));
    s.push_back (shot);
    SampleSpec unaligned = pitchedTone (55, 6);          // 4 starts inside a 16-byte block (as many real samples do)
    unaligned.forceStart = 0x20000 + 5;
    s.push_back (unaligned);
    std::vector<GroupSpec> g;
    g.push_back ({ "Syn Saw 1", { 0, 1, 2 }, { 54, 66 } });
    g.push_back ({ "Triangle", { 3 }, {} });
    g.push_back ({ "Syn Sine", { 4 }, {} });
    return buildImage (s, g);
}
static std::vector<uint8_t> syntheticChipB()
{
    std::vector<SampleSpec> s;
    s.push_back (pitchedTone (67));
    s.push_back (pitchedTone (79));
    std::vector<GroupSpec> g;
    g.push_back ({ "Pluck Harp", { 0, 1 }, { 72 } });
    g.push_back ({ "Not A Wave", { 0 }, {} });           // name that is not a JD-800 waveform
    g.push_back ({ "Syn Saw 1", { 1 }, {} });            // duplicate of a wave chip A provides
    return buildImage (s, g);
}

static void testPrimitives()
{
    const char* v = "123456789";
    check (jdrom::crc32 (reinterpret_cast<const uint8_t*> (v), 9) == 0xCBF43926u, "CRC-32 of '123456789' is cbf43926");
    std::mt19937 rng (7);
    std::vector<uint8_t> a (jdrom::kChipBytes), b (a.size()), c (a.size());
    for (auto& x : a) x = (uint8_t) rng();
    jdrom::descramble (a.data(), b.data(), a.size());
    jdrom::scramble (b.data(), c.data(), a.size());
    check (a == c, "scramble() is the exact inverse of descramble() over a 1 MiB random image");
    check (a != b, "descramble() really permutes the data");
    // one bit through the permutation: image address bit 0 -> dump address bit 2, image data bit 0 <- dump data bit 2
    std::vector<uint8_t> raw (jdrom::kChipBytes, 0), img (jdrom::kChipBytes);
    raw[4] = 0x04;
    jdrom::descramble (raw.data(), img.data(), raw.size());
    check (img[1] == 0x01 && std::count_if (img.begin(), img.end(), [] (uint8_t x) { return x != 0; }) == 1,
           "address bit 0 of the readable image comes from bit 2 of the dump address, data bit 0 from bit 2 of the dump byte");
    check (jdrom::identifyKnownChip (a.data(), a.size()) == -1 && jdrom::identifyKnownChip (a.data(), 1000) == -1,
           "random data / wrong size is not identified as a known chip");
}

static void testParse()
{
    const auto img = syntheticChipA();
    jdrom::ChipTables t;
    std::string why;
    check (jdrom::parseTables (img.data(), img.size(), t, why), "synthetic chip parses: " + why);
    check (t.samples.size() == 5 && t.groups.size() == 3, "5 samples and 3 groups read back");
    check (t.groups[0].name == "Syn Saw 1" && t.groups[0].samples.size() == 3 && t.groups[0].upperKey.size() == 2 && t.groups[0].upperKey[1] == 66,
           "group name, sample list and key limits read back");
    check (t.samples[3].loopMode == 2 && t.samples[3].loopLength == 0 && t.samples[1].rootKey == 60 && t.samples[1].loopMode == 0,
           "loop mode, loop length and root key read back");

    auto bad = [&] (const char* what, std::vector<uint8_t> m, void (*mut) (std::vector<uint8_t>&))
    {
        mut (m);
        jdrom::ChipTables x; std::string w;
        check (! jdrom::parseTables (m.data(), m.size(), x, w) && ! w.empty(), std::string ("rejected: ") + what + " (" + w + ")");
    };
    bad ("zero sample count", img, [] (std::vector<uint8_t>& m) { put16 (m, 0x60, 0); });
    bad ("table pointer beyond the chip", img, [] (std::vector<uint8_t>& m) { put32 (m, 0x80, 0xFFFF8); });
    bad ("sample table pointer inside the header", img, [] (std::vector<uint8_t>& m) { put32 (m, 0x80, 0x40); });
    bad ("sample running past the end of the chip", img, [] (std::vector<uint8_t>& m) { put32 (m, 0xFE000 + 4, 0xFFFFF); });
    bad ("sample start below the wave area", img, [] (std::vector<uint8_t>& m) { put24 (m, 0xFE000 + 1, 0x100); });
    bad ("invalid loop mode", img, [] (std::vector<uint8_t>& m) { m[0xFE000 + 10] = 9; });
    bad ("loop longer than the sample", img, [] (std::vector<uint8_t>& m) { put16 (m, 0xFE000 + 8, 0xFFFF); });
    bad ("non-printable group name", img, [] (std::vector<uint8_t>& m) { m[0xFF000] = 3; });
    bad ("group pointing at a missing sample", img, [] (std::vector<uint8_t>& m) { put16 (m, 0xFF000 + 28, 99); });
    bad ("descending key limits", img, [] (std::vector<uint8_t>& m) { m[0xFF000 + 12] = 90; m[0xFF000 + 13] = 40; });
    std::vector<uint8_t> shortImg (img.begin(), img.begin() + 1000);
    jdrom::ChipTables x; std::string w;
    check (! jdrom::parseTables (shortImg.data(), shortImg.size(), x, w), "rejected: a file that is not 1 MiB");
}

static void testDecode()
{
    const auto img = syntheticChipA();
    jdrom::ChipTables t; std::string why;
    jdrom::parseTables (img.data(), img.size(), t, why);
    // sample 1: pitchedTone(60) = 24-period sine, amplitude 30000/65536
    std::vector<float> d;
    jdrom::decodeSample (img.data(), t.samples[1], d);
    const auto src = pitchedTone (60);
    check (d.size() == src.pcm.size(), "decoded length equals the table length");
    double err = 0, peak = 0;
    for (size_t i = 16; i < d.size(); ++i) { err = std::max (err, (double) std::abs (d[i] - src.pcm[i] * jdrom::kGain)); peak = std::max (peak, (double) std::abs (d[i])); }
    check (err < 2.0e-3, "delta-coded data decodes back to the original waveform (max error " + std::to_string (err) + " of full scale)");
    check (peak > 0.44 && peak < 0.47, "full scale is +-65536 (peak " + std::to_string (peak) + ")");
    // unaligned start (shares a 16-byte shift block with the end of the previous data)
    jdrom::decodeSample (img.data(), t.samples[4], d);
    const auto un = pitchedTone (55, 6);
    double e2 = 0;
    for (size_t i = 16; i < d.size(); ++i) e2 = std::max (e2, (double) std::abs (d[i] - un.pcm[i] * jdrom::kGain));
    check (e2 < 2.0e-3, "a sample that starts inside a 16-byte block decodes correctly (max error " + std::to_string (e2) + ")");
    // the loop region has no DC after decoding
    jdrom::decodeSample (img.data(), t.samples[1], d);
    double m = 0;
    const size_t ll = t.samples[1].loopLength;
    for (size_t i = d.size() - ll; i < d.size(); ++i) m += d[i];
    check (std::abs (m / double (ll)) < 1.0e-4, "loop region is DC free");
    // garbage in the data area must not overflow or produce NaN
    auto noisy = img;
    std::mt19937 rng (3);
    for (size_t i = 0x8000; i < 0xF0000; ++i) noisy[i] = (uint8_t) rng();
    jdrom::decodeSample (noisy.data(), t.samples[0], d);
    bool finite = true; float mx = 0;
    for (float v : d) { finite = finite && std::isfinite (v); mx = std::max (mx, std::abs (v)); }
    check (finite && mx <= 2.5f, "random data decodes to bounded, finite values (peak " + std::to_string (mx) + ")");
}

static void testTuning()
{
    jdrom::SampleRecord r;
    r.field12 = 1066; r.field14 = 1024;
    check (std::abs (jdrom::sampleTuneCents (r)) < 1.0e-9, "tuning word 1066 + 1024 = no deviation");
    r.field12 = 566;
    check (std::abs (jdrom::sampleTuneCents (r) - 51.2) < 0.01, "500 units below = +51.2 cents (sample sounds above its label)");
    r.field12 = 1066; r.field14 = 1124;
    check (std::abs (jdrom::sampleTuneCents (r) + 10.24) < 0.01, "field 14 counts the same way (+100 units = -10.24 cents)");
    r.field12 = 60000;
    check (jdrom::sampleTuneCents (r) == 0.0, "a value outside +-150 cents is not trusted (0)");
    // the correction reaches the sample's root pitch
    std::vector<SampleSpec> s { pitchedTone (60) };
    auto img = buildImage (s, { { "Syn Saw 1", { 0 }, {} } });
    put16 (img, 0xFE000 + 12, 566);                        // sample sounds 51.2 cents above its label
    dc::UserWaveSet set; jdrom::Report rep;
    jdrom::addImage (img.data(), img.size(), "t", set, rep);
    const double rootHz = set.jdWave (0, 60)->rootHz;
    check (std::abs (1200.0 * std::log2 (rootHz / 261.6256) - 51.2) < 0.1, "root pitch of the imported sample includes the tuning word (+51.2 cents)");
}

static void testNames()
{
    check (jdrom::waveIndexForName ("Pluck Harp") == 56 && jdrom::waveIndexForName ("Syn Saw 1") == 0, "ROM names map to JD-800 waveform numbers");
    check (jdrom::waveIndexForName ("Pole lp") == jdrom::waveIndexForName ("Pole Ip") && jdrom::waveIndexForName ("Pole lp") >= 0, "'Pole lp' / 'Pole Ip' spellings are the same wave");
    check (jdrom::waveIndexForName ("Tabla") >= 0 && jdrom::waveIndexForName ("Table") == jdrom::waveIndexForName ("Tabla"), "'Tabla' / 'Table' are the same wave");
    check (jdrom::waveIndexForName ("ZZZ Vox") >= 0 && jdrom::waveIndexForName ("ZZZ Voz") == jdrom::waveIndexForName ("ZZZ Vox"), "'ZZZ Vox' / 'ZZZ Voz' are the same wave");
    check (jdrom::waveIndexForName ("no such wave") == -1, "an unknown name maps to -1");
    // all 108 names are distinct after normalisation (so no two waves can be confused)
    bool distinct = true;
    for (int i = 0; i < 108 && distinct; ++i)
        distinct = jdrom::waveIndexForName (jd800::kWaveNames[i]) == i;
    check (distinct, "each of the 108 waveform names maps back to its own number");
}

static void testBuild()
{
    dc::UserWaveSet set;
    jdrom::Report rep;
    const auto a = syntheticChipA(), b = syntheticChipB();
    check (jdrom::addImage (a.data(), a.size(), "A", set, rep), "chip A contributes waves");
    check (jdrom::addImage (b.data(), b.size(), "B", set, rep), "chip B contributes waves");
    check (rep.chipsUsed == 2 && rep.wavesFilled == 4, "4 waves filled from 2 chips (the duplicate and the unknown name are not counted): " + std::to_string (rep.wavesFilled));
    bool reported = false;
    for (auto& m : rep.messages) reported = reported || m.find ("Not A Wave") != std::string::npos;
    check (reported, "a multisample whose name is not a JD-800 waveform is reported");
    const int saw = 0, tri = jdrom::waveIndexForName ("Triangle"), harp = jdrom::waveIndexForName ("Pluck Harp");
    check (set.hasRomJd (saw) && set.countRomJd() == 4, "waves are available through the set");
    // zone selection: limits 54 and 66, then the rest
    const dc::SampleWave* z48 = set.jdWave (saw, 48);   // zone 0
    const dc::SampleWave* z54 = set.jdWave (saw, 54);   // key == limit -> still zone 0
    const dc::SampleWave* z55 = set.jdWave (saw, 55);   // zone 1
    const dc::SampleWave* z66 = set.jdWave (saw, 66);
    const dc::SampleWave* z67 = set.jdWave (saw, 67);   // zone 2
    const dc::SampleWave* z127 = set.jdWave (saw, 127);
    check (z48 != nullptr && z48 == z54 && z55 != z54 && z55 == z66 && z67 != z66 && z67 == z127, "key zones switch above each limit and the last sample covers the top");
    check (std::abs (z55->rootHz - 261.6256f) < 0.01f && std::abs (z48->rootHz - 130.8128f) < 0.01f, "root key becomes the sample's root pitch");
    check (z55->loop && z55->loopEnd == (int) z55->data.size() && z55->loopStart > 0, "a looped sample keeps its loop at the tail");
    check (std::abs (z55->rate - 44100.0f) < 0.5f && ! z55->mips.empty(), "data rate is 44.1 kHz and the band-limiting pyramid is built");
    check (! set.jdWave (tri, 60)->loop, "a one-shot sample does not loop");
    check (set.jdWave (harp, 60) != nullptr && set.jdWave (harp, 100) != set.jdWave (harp, 60), "chip B's waves are present with their own zones");
    check (set.jdWave (saw, 60) != set.jdWave (harp, 60), "waves differ");
    // a wave file the user loaded wins; ROM does not overwrite it
    dc::UserWaveSet set2;
    set2.jd[0].data = { 0.f, 0.1f, 0.2f, 0.1f };
    jdrom::Report r2;
    jdrom::addImage (a.data(), a.size(), "A", set2, r2);
    check (! set2.hasRomJd (0) && set2.jdWave (0, 60) == &set2.jd[0] && r2.wavesFilled == 2, "a user wave file for a number is kept (ROM fills only the other waves)");
}

static void testAddChip()
{
    dc::UserWaveSet set;
    jdrom::Report rep;
    const auto raw = scrambled (syntheticChipA());
    check (jdrom::addChip (raw.data(), raw.size(), "dumpA", set, rep) && rep.wavesFilled == 3 && rep.chipsVerified == 0,
           "a scrambled dump with an unpublished CRC is accepted by structure (not counted as verified)");
    dc::UserWaveSet set2; jdrom::Report r2;
    const auto readable = syntheticChipA();
    check (jdrom::addChip (readable.data(), readable.size(), "imgA", set2, r2) && r2.wavesFilled == 3, "an already readable image is accepted too");
    dc::UserWaveSet set3; jdrom::Report r3;
    std::mt19937 rng (11);
    std::vector<uint8_t> garbage (jdrom::kChipBytes);
    for (auto& x : garbage) x = (uint8_t) rng();
    check (! jdrom::addChip (garbage.data(), garbage.size(), "junk", set3, r3) && r3.chipsRejected == 1 && set3.countRomJd() == 0, "random data is rejected");
    std::vector<uint8_t> small (4096, 0);
    check (! jdrom::addChip (small.data(), small.size(), "small", set3, r3) && set3.countRomJd() == 0, "a file of the wrong size is rejected");
}

// ---- optional: the user's own dumps ------------------------------------------------------------
static std::vector<uint8_t> readFile (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    return std::vector<uint8_t> ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
}

static void testRealDumps()
{
    const char* dir = std::getenv ("DIGITALCOMBO_JD800_ROM_DIR");
    if (dir == nullptr) { std::printf ("SKIP  real JD-800 dumps (DIGITALCOMBO_JD800_ROM_DIR is not set; not available in CI)\n"); return; }
    dc::UserWaveSet set;
    jdrom::Report rep;
    const char* files[] = { "roland_r15209290_mb838000-20_2a6-aa_9114-r11.ic25", "roland_r15209291_mb838000-20_2a7-aa_9119-r15.ic26", "roland_r15209305_mb838000-20_2a8-aa_9102-r01.ic1" };
    int found = 0;
    for (auto* f : files)
    {
        auto data = readFile (std::string (dir) + "/" + f);
        if (data.empty()) data = readFile (std::string (dir) + "/jd800/" + f);
        if (data.empty()) continue;
        ++found;
        jdrom::addChip (data.data(), data.size(), f, set, rep);
    }
    if (found != 3) { std::printf ("SKIP  real JD-800 dumps: only %d of 3 chip files found in %s\n", found, dir); return; }
    check (rep.chipsVerified == 3 && rep.chipsUsed == 3, "the three real chips are recognised by CRC-32 and used");
    check (set.countRomJd() == 108, "all 108 JD-800 waveforms are filled from the real ROM (" + std::to_string (set.countRomJd()) + ")");
    check (rep.samplesDecoded > 600, "samples decoded: " + std::to_string (rep.samplesDecoded));
    int bad = 0, looped = 0, oneShot = 0, nonFinite = 0;
    double peakAll = 0;
    for (int w = 0; w < 108; ++w)
        for (const auto& z : set.jdZones[w])
        {
            const auto& s = *z.wave;
            for (float v : s.data) { if (! std::isfinite (v)) ++nonFinite; peakAll = std::max (peakAll, (double) std::abs (v)); }
            if (s.loop) { ++looped; if (s.loopStart < 0 || s.loopEnd > (int) s.data.size() || s.loopEnd - s.loopStart < 2) ++bad; }
            else ++oneShot;
        }
    check (bad == 0 && nonFinite == 0, "every decoded sample is finite and every loop lies inside its sample");
    check (peakAll < 1.6, "decoded amplitude stays within the +-65536 accumulator range (peak " + std::to_string (peakAll) + ")");
    // waveforms the manual lists as one-shot ('O') are one-shots in the ROM, and loops are loops
    int loopTypeMismatch = 0;
    for (int w = 0; w < 108; ++w)
    {
        const bool wantShot = jd800::kWaveLoopType[w] == 'O';
        for (const auto& z : set.jdZones[w])
            if (z.wave->loop == wantShot) ++loopTypeMismatch;
    }
    check (loopTypeMismatch == 0, "loop / one-shot flags of every sample agree with the manual's waveform list (O = one shot): " + std::to_string (loopTypeMismatch) + " disagreements");
    // seam continuity of the loops: the step across the loop point is no larger than the typical second difference
    std::vector<double> seam;
    for (int w = 0; w < 108; ++w)
        for (const auto& z : set.jdZones[w])
        {
            const auto& s = *z.wave;
            if (! s.loop || s.loopEnd - s.loopStart < 8 || s.data.size() < 3000) continue;
            double sd = 0;
            const size_t n = s.data.size();
            for (size_t i = n - 2000; i + 1 < n; ++i) sd += std::abs (s.data[i + 1] - 2 * s.data[i] + s.data[i - 1]);
            sd /= 2000.0;
            if (sd <= 0) continue;
            const double pred = s.data[(size_t) s.loopEnd - 1] + (s.data[(size_t) s.loopEnd - 1] - s.data[(size_t) s.loopEnd - 2]);
            seam.push_back (std::abs (s.data[(size_t) s.loopStart] - pred) / sd);
        }
    // tuning: pitch implied by the loop length (an integer number of periods) against the root pitch the importer
    // assigned, for loops of 150+ samples where the integer rounding is below 6 cents
    {
        std::vector<double> dev;
        for (int w = 0; w < 108; ++w)
            for (const auto& z : set.jdZones[w])
            {
                const auto& s = *z.wave;
                if (! s.loop) continue;
                const double ll = s.loopEnd - s.loopStart;
                if (ll < 150 || ll > 8000) continue;
                double best = 1e9; int bestN = 1;
                for (int n = 1; n <= 3; ++n)
                {
                    const double c = 1200.0 * std::log2 (n * jdrom::kDataRate / ll / s.rootHz);
                    if (std::abs (c) < std::abs (best)) { best = c; bestN = n; }
                }
                if (std::abs (best) < 300.0 && 1731.0 * 0.5 / ll * bestN <= 6.0) dev.push_back (best);
            }
        double mean = 0, sq = 0, worst = 0;
        for (double d : dev) { mean += d; worst = std::max (worst, std::abs (d)); }
        mean /= std::max<size_t> (1, dev.size());
        for (double d : dev) sq += (d - mean) * (d - mean);
        const double sd = std::sqrt (sq / double (std::max<size_t> (1, dev.size())));
        check (dev.size() > 120 && sd < 6.0 && worst < 25.0,
               "with the tuning word applied, " + std::to_string (dev.size()) + " loop-length pitches sit on their root keys: mean " + std::to_string (mean)
               + " cents, spread " + std::to_string (sd) + ", worst " + std::to_string (worst));
    }
    std::sort (seam.begin(), seam.end());
    const double med = seam.empty() ? 1e9 : seam[seam.size() / 2];
    check (seam.size() > 100 && med < 3.0, "loop seams are continuous: median seam step " + std::to_string (med) + " typical second differences over " + std::to_string (seam.size()) + " samples");
}

int main()
{
    testPrimitives();
    testParse();
    testDecode();
    testTuning();
    testNames();
    testBuild();
    testAddChip();
    testRealDumps();
    std::printf ("\nRESULT: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
