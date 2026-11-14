#pragma once
// Reads the JD-800's internal waveforms from PCM ROM dumps that the USER supplies from their own
// disk (three 1 MiB wave-ROM chips). Nothing from those ROMs is bundled, copied or uploaded: the
// plug-in only parses what the user points it at, keeps the decoded audio in memory, and never
// writes it anywhere.
//
// How the chips are read (all of it checked against the dumps' own structure; see
// docs/ROM-IMPORT.md for the measurements and what is still an assumption):
//  1. The dump is address- and data-line scrambled (the chip's pin order on the board). The inverse
//     permutation turns it into a readable image: a header, a sample table, a multisample table.
//  2. Header: +0x60 sample count, +0x62 multisample ("wave group") count, +0x80 sample table,
//     +0x84 group table (chip-local addresses).
//  3. Sample record, 16 bytes: byte 0 (unknown), 3-byte start, 4-byte length, 2-byte loop length,
//     loop mode (0 = loop the last `loop length` samples, 1 = same, long loop, 2 = one shot),
//     root key, two unknown 16-bit fields.
//  4. Group record, 60 bytes: 12-character name (= the waveform's name in the manual), 16 upper key
//     limits (0x7F = unused), 16 sample numbers (0xFFFF = unused). The last used sample plays above
//     the last limit.
//  5. Sample data is delta coded: signed bytes, shifted by a 4-bit value taken from a table in the
//     first 1/32 of the chip (one nibble per 16 bytes), accumulated.
// Plain C++ (no JUCE) so it can be unit-tested everywhere.

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "UserWaves.h"

namespace jdrom
{
constexpr size_t kChipBytes = 1u << 20;
constexpr double kDataRate = 44100.0;     // inferred (see ROM-IMPORT.md): loop lengths match 44.1 kHz pitches
constexpr float kGain = 1.0f / 65536.0f;  // accumulator range is +-65536

// The two 16-bit fields at +12 / +14 of a sample record behave as a tuning word: the sample's own pitch minus its
// root-key label is, in cents, 0.1024 * (2090 - field12 - field14) (inferred by fitting 181 loop-length pitches,
// residual 2.9 cents; checked against 89 autocorrelation pitches incl. one shots, residual 9.7 cents, versus 38 cents
// spread when ignored). Values outside +-150 cents are not trusted and give 0.
constexpr double kTuneCentsPerUnit = 0.1024, kTuneZero = 2090.0;

struct KnownChip { uint32_t crc32; const char* label; };
extern const KnownChip kKnownChips[3];    // CRC-32 of the scrambled dumps as the MAME project lists them

uint32_t crc32 (const uint8_t* data, size_t size) noexcept;
// 0..2 for one of the known JD-800 wave ROM dumps (size and CRC both match), otherwise -1.
int identifyKnownChip (const uint8_t* rawDump, size_t size) noexcept;

void descramble (const uint8_t* rawDump, uint8_t* out, size_t size) noexcept;   // size = kChipBytes
void scramble (const uint8_t* image, uint8_t* out, size_t size) noexcept;       // the inverse (used by the tests' synthetic ROM)

struct SampleRecord
{
    uint8_t byte0 = 0;
    uint32_t start = 0, length = 0, loopLength = 0;
    uint8_t loopMode = 0, rootKey = 60;
    uint16_t field12 = 0, field14 = 0;
};
struct GroupRecord
{
    std::string name;
    std::vector<uint8_t> upperKey;       // one per sample except the last
    std::vector<uint16_t> samples;       // indices into the sample table
};
struct ChipTables
{
    std::vector<SampleRecord> samples;
    std::vector<GroupRecord> groups;
};

// Cents by which the sample's real pitch lies above its root key label (see kTuneCentsPerUnit).
double sampleTuneCents (const SampleRecord&) noexcept;

// Parses and validates a descrambled image. On failure `why` says which check failed.
bool parseTables (const uint8_t* image, size_t size, ChipTables& out, std::string& why);
// Decodes one sample to floats (full scale = 1.0, DC removed, 16-sample fade-in).
void decodeSample (const uint8_t* image, const SampleRecord&, std::vector<float>& out);

// Index 0..107 of the JD-800 waveform with this ROM name (spelling-tolerant), or -1.
int waveIndexForName (const std::string& romName);

struct Report
{
    int chipsUsed = 0, chipsRejected = 0, chipsVerified = 0;
    int wavesFilled = 0, samplesDecoded = 0;
    std::vector<std::string> messages;
};

// Adds one raw 1 MiB file. Accepts the scrambled dump (descrambles it) or an already readable image.
// Returns true if it contributed waves. Waves a user WAV file already provides are left alone.
bool addChip (const uint8_t* data, size_t size, const std::string& label, dc::UserWaveSet& set, Report& report);
// For tests: a chip whose image is already readable (no CRC / descramble step).
bool addImage (const uint8_t* image, size_t size, const std::string& label, dc::UserWaveSet& set, Report& report);
} // namespace jdrom
