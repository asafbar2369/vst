#include "FmVoice.h"

namespace vsx::fm
{
// =================================================================================
// Algorithms. Edges "a>b": OP a modulates OP b. Written from the published algorithm chart.
namespace
{
    struct AlgoDef { const char* edges; const char* carriers; int fbFrom, fbTo; };
    const AlgoDef kAlgoDefs[kAlgorithms] = {
        { "6>5 5>4 4>3 2>1", "13", 6, 6 },          // 1
        { "6>5 5>4 4>3 2>1", "13", 2, 2 },          // 2
        { "6>5 5>4 3>2 2>1", "14", 6, 6 },          // 3
        { "6>5 5>4 3>2 2>1", "14", 4, 6 },          // 4
        { "6>5 4>3 2>1", "135", 6, 6 },             // 5
        { "6>5 4>3 2>1", "135", 5, 6 },             // 6
        { "6>5 5>3 4>3 2>1", "13", 6, 6 },          // 7
        { "6>5 5>3 4>3 2>1", "13", 4, 4 },          // 8
        { "6>5 5>3 4>3 2>1", "13", 2, 2 },          // 9
        { "3>2 2>1 6>4 5>4", "14", 3, 3 },          // 10
        { "3>2 2>1 6>4 5>4", "14", 6, 6 },          // 11
        { "2>1 6>3 5>3 4>3", "13", 2, 2 },          // 12
        { "2>1 6>3 5>3 4>3", "13", 6, 6 },          // 13
        { "2>1 6>4 5>4 4>3", "13", 6, 6 },          // 14
        { "2>1 6>4 5>4 4>3", "13", 2, 2 },          // 15
        { "2>1 4>3 3>1 6>5 5>1", "1", 6, 6 },       // 16
        { "2>1 4>3 3>1 6>5 5>1", "1", 2, 2 },       // 17
        { "2>1 3>1 6>5 5>4 4>1", "1", 3, 3 },       // 18
        { "3>2 2>1 6>4 6>5", "145", 6, 6 },         // 19
        { "3>1 3>2 5>4 6>4", "124", 3, 3 },         // 20
        { "3>1 3>2 6>4 6>5", "1245", 3, 3 },        // 21
        { "2>1 6>3 6>4 6>5", "1345", 6, 6 },        // 22
        { "3>2 6>4 6>5", "1245", 6, 6 },            // 23
        { "6>3 6>4 6>5", "12345", 6, 6 },           // 24
        { "6>4 6>5", "12345", 6, 6 },               // 25
        { "3>2 5>4 6>4", "124", 6, 6 },             // 26
        { "3>2 5>4 6>4", "124", 3, 3 },             // 27
        { "2>1 5>4 4>3", "136", 5, 5 },             // 28
        { "4>3 6>5", "1235", 6, 6 },                // 29
        { "5>4 4>3", "1236", 5, 5 },                // 30
        { "6>5", "12345", 6, 6 },                   // 31
        { "", "123456", 6, 6 },                     // 32
    };

    std::array<Algorithm, kAlgorithms> buildAlgorithms()
    {
        std::array<Algorithm, kAlgorithms> out {};
        for (int a = 0; a < kAlgorithms; ++a)
        {
            const auto& d = kAlgoDefs[a];
            auto& al = out[(size_t) a];
            for (const char* p = d.edges; *p != 0; ++p)
                if (p[1] == '>' && p[0] >= '1' && p[0] <= '6' && p[2] >= '1' && p[2] <= '6')
                    al.modulators[(size_t) (p[2] - '1')] |= (uint8_t) (1u << (p[0] - '1'));
            for (const char* p = d.carriers; *p != 0; ++p) al.carriers |= (uint8_t) (1u << (*p - '1'));
            al.feedbackSource = d.fbFrom - 1;
            al.feedbackTarget = d.fbTo - 1;
        }
        return out;
    }

    // 4096-point sine table with a guard point, read with linear interpolation from a 32-bit phase
    struct SineTable
    {
        std::array<float, 4097> t {};
        SineTable() { for (int i = 0; i <= 4096; ++i) t[(size_t) i] = (float) std::sin (2.0 * 3.14159265358979323846 * i / 4096.0); }
        float operator() (uint32_t phase) const noexcept
        {
            const uint32_t idx = phase >> 20;
            const float frac = (float) (phase & 0xFFFFFu) * (1.0f / 1048576.0f);
            return t[idx] + (t[idx + 1] - t[idx]) * frac;
        }
    };
    const SineTable& sine() { static const SineTable s; return s; }

    uint8_t lim (uint8_t v, int hi, int& changed) { if (v > hi) { ++changed; return (uint8_t) hi; } return v; }
    uint8_t c7 (uint8_t v, int hi) { return (uint8_t) std::min<int> (v & 0x7F, hi); }
}

int Algorithm::numCarriers() const
{
    int n = 0;
    for (int i = 0; i < kOps; ++i) n += (carriers >> i) & 1;
    return n;
}

const Algorithm& algorithm (int index)
{
    static const auto table = buildAlgorithms();
    return table[(size_t) juce::jlimit (0, kAlgorithms - 1, index)];
}

// =================================================================================
juce::String Patch::getName() const
{
    juce::String s;
    for (char c : name) s += (juce::juce_wchar) ((c >= 32 && c < 127) ? c : ' ');
    return s.trimEnd();
}

void Patch::setName (const juce::String& n)
{
    for (int i = 0; i < 10; ++i)
    {
        const auto c = i < n.length() ? n[i] : (juce::juce_wchar) ' ';
        name[(size_t) i] = (c >= 32 && c < 127) ? (char) c : ' ';
    }
}

int Patch::clamp()
{
    int ch = 0;
    for (auto& o : op)
    {
        for (auto& r : o.rate) r = lim (r, 99, ch);
        for (auto& l : o.level) l = lim (l, 99, ch);
        o.breakPoint = lim (o.breakPoint, 99, ch);
        o.leftDepth = lim (o.leftDepth, 99, ch);
        o.rightDepth = lim (o.rightDepth, 99, ch);
        o.leftCurve = lim (o.leftCurve, 3, ch);
        o.rightCurve = lim (o.rightCurve, 3, ch);
        o.rateScale = lim (o.rateScale, 7, ch);
        o.ampModSens = lim (o.ampModSens, 3, ch);
        o.velSens = lim (o.velSens, 7, ch);
        o.outputLevel = lim (o.outputLevel, 99, ch);
        o.fixedMode = lim (o.fixedMode, 1, ch);
        o.coarse = lim (o.coarse, 31, ch);
        o.fine = lim (o.fine, 99, ch);
        o.detune = lim (o.detune, 14, ch);
    }
    for (auto& r : pitchRate) r = lim (r, 99, ch);
    for (auto& l : pitchLevel) l = lim (l, 99, ch);
    algorithm = lim (algorithm, 31, ch);
    feedback = lim (feedback, 7, ch);
    oscSync = lim (oscSync, 1, ch);
    lfoSpeed = lim (lfoSpeed, 99, ch);
    lfoDelay = lim (lfoDelay, 99, ch);
    lfoPmd = lim (lfoPmd, 99, ch);
    lfoAmd = lim (lfoAmd, 99, ch);
    lfoSync = lim (lfoSync, 1, ch);
    lfoWave = lim (lfoWave, 5, ch);
    lfoPms = lim (lfoPms, 7, ch);
    transpose = lim (transpose, 48, ch);
    for (auto& c : name)
        if (c < 32 || c > 126) { c = ' '; ++ch; }
    opEnabled &= 0x3F;
    return ch;
}

std::array<uint8_t, kVcedSize> Patch::toVced() const
{
    std::array<uint8_t, kVcedSize> d {};
    int k = 0;
    for (int i = kOps - 1; i >= 0; --i) // OP6 first
    {
        const auto& o = op[(size_t) i];
        for (auto r : o.rate) d[(size_t) k++] = r;
        for (auto l : o.level) d[(size_t) k++] = l;
        d[(size_t) k++] = o.breakPoint; d[(size_t) k++] = o.leftDepth; d[(size_t) k++] = o.rightDepth;
        d[(size_t) k++] = o.leftCurve; d[(size_t) k++] = o.rightCurve; d[(size_t) k++] = o.rateScale;
        d[(size_t) k++] = o.ampModSens; d[(size_t) k++] = o.velSens; d[(size_t) k++] = o.outputLevel;
        d[(size_t) k++] = o.fixedMode; d[(size_t) k++] = o.coarse; d[(size_t) k++] = o.fine; d[(size_t) k++] = o.detune;
    }
    for (auto r : pitchRate) d[(size_t) k++] = r;
    for (auto l : pitchLevel) d[(size_t) k++] = l;
    d[(size_t) k++] = algorithm; d[(size_t) k++] = feedback; d[(size_t) k++] = oscSync;
    d[(size_t) k++] = lfoSpeed; d[(size_t) k++] = lfoDelay; d[(size_t) k++] = lfoPmd; d[(size_t) k++] = lfoAmd;
    d[(size_t) k++] = lfoSync; d[(size_t) k++] = lfoWave; d[(size_t) k++] = lfoPms; d[(size_t) k++] = transpose;
    for (char c : name) d[(size_t) k++] = (uint8_t) c;
    jassert (k == kVcedSize);
    return d;
}

Patch Patch::fromVced (const uint8_t* d)
{
    Patch p;
    int k = 0;
    for (int i = kOps - 1; i >= 0; --i)
    {
        auto& o = p.op[(size_t) i];
        for (auto& r : o.rate) r = d[k++] & 0x7F;
        for (auto& l : o.level) l = d[k++] & 0x7F;
        o.breakPoint = d[k++] & 0x7F; o.leftDepth = d[k++] & 0x7F; o.rightDepth = d[k++] & 0x7F;
        o.leftCurve = d[k++] & 0x7F; o.rightCurve = d[k++] & 0x7F; o.rateScale = d[k++] & 0x7F;
        o.ampModSens = d[k++] & 0x7F; o.velSens = d[k++] & 0x7F; o.outputLevel = d[k++] & 0x7F;
        o.fixedMode = d[k++] & 0x7F; o.coarse = d[k++] & 0x7F; o.fine = d[k++] & 0x7F; o.detune = d[k++] & 0x7F;
    }
    for (auto& r : p.pitchRate) r = d[k++] & 0x7F;
    for (auto& l : p.pitchLevel) l = d[k++] & 0x7F;
    p.algorithm = d[k++] & 0x7F; p.feedback = d[k++] & 0x7F; p.oscSync = d[k++] & 0x7F;
    p.lfoSpeed = d[k++] & 0x7F; p.lfoDelay = d[k++] & 0x7F; p.lfoPmd = d[k++] & 0x7F; p.lfoAmd = d[k++] & 0x7F;
    p.lfoSync = d[k++] & 0x7F; p.lfoWave = d[k++] & 0x7F; p.lfoPms = d[k++] & 0x7F; p.transpose = d[k++] & 0x7F;
    for (auto& c : p.name) c = (char) (d[k++] & 0x7F);
    p.clamp();
    return p;
}

std::array<uint8_t, kPackedSize> Patch::toPacked() const
{
    std::array<uint8_t, kPackedSize> d {};
    for (int i = kOps - 1, base = 0; i >= 0; --i, base += 17)
    {
        const auto& o = op[(size_t) i];
        for (int j = 0; j < 4; ++j) { d[(size_t) (base + j)] = o.rate[(size_t) j]; d[(size_t) (base + 4 + j)] = o.level[(size_t) j]; }
        d[(size_t) base + 8] = o.breakPoint;
        d[(size_t) base + 9] = o.leftDepth;
        d[(size_t) base + 10] = o.rightDepth;
        d[(size_t) base + 11] = (uint8_t) ((o.rightCurve << 2) | o.leftCurve);
        d[(size_t) base + 12] = (uint8_t) ((o.detune << 3) | o.rateScale);
        d[(size_t) base + 13] = (uint8_t) ((o.velSens << 2) | o.ampModSens);
        d[(size_t) base + 14] = o.outputLevel;
        d[(size_t) base + 15] = (uint8_t) ((o.coarse << 1) | o.fixedMode);
        d[(size_t) base + 16] = o.fine;
    }
    for (int j = 0; j < 4; ++j) { d[(size_t) (102 + j)] = pitchRate[(size_t) j]; d[(size_t) (106 + j)] = pitchLevel[(size_t) j]; }
    d[110] = algorithm;
    d[111] = (uint8_t) ((oscSync << 3) | feedback);
    d[112] = lfoSpeed; d[113] = lfoDelay; d[114] = lfoPmd; d[115] = lfoAmd;
    d[116] = (uint8_t) ((lfoPms << 4) | (lfoWave << 1) | lfoSync);
    d[117] = transpose;
    for (int j = 0; j < 10; ++j) d[(size_t) (118 + j)] = (uint8_t) name[(size_t) j];
    for (auto& b : d) b &= 0x7F;
    return d;
}

Patch Patch::fromPacked (const uint8_t* d)
{
    Patch p;
    for (int i = kOps - 1, base = 0; i >= 0; --i, base += 17)
    {
        auto& o = p.op[(size_t) i];
        for (int j = 0; j < 4; ++j) { o.rate[(size_t) j] = d[base + j] & 0x7F; o.level[(size_t) j] = d[base + 4 + j] & 0x7F; }
        o.breakPoint = d[base + 8] & 0x7F;
        o.leftDepth = d[base + 9] & 0x7F;
        o.rightDepth = d[base + 10] & 0x7F;
        o.leftCurve = d[base + 11] & 3;
        o.rightCurve = (d[base + 11] >> 2) & 3;
        o.rateScale = d[base + 12] & 7;
        o.detune = (d[base + 12] >> 3) & 15;
        o.ampModSens = d[base + 13] & 3;
        o.velSens = (d[base + 13] >> 2) & 7;
        o.outputLevel = d[base + 14] & 0x7F;
        o.fixedMode = d[base + 15] & 1;
        o.coarse = (d[base + 15] >> 1) & 31;
        o.fine = d[base + 16] & 0x7F;
    }
    for (int j = 0; j < 4; ++j) { p.pitchRate[(size_t) j] = d[102 + j] & 0x7F; p.pitchLevel[(size_t) j] = d[106 + j] & 0x7F; }
    p.algorithm = d[110] & 31;
    p.feedback = d[111] & 7;
    p.oscSync = (d[111] >> 3) & 1;
    p.lfoSpeed = d[112] & 0x7F; p.lfoDelay = d[113] & 0x7F; p.lfoPmd = d[114] & 0x7F; p.lfoAmd = d[115] & 0x7F;
    p.lfoSync = d[116] & 1;
    p.lfoWave = (d[116] >> 1) & 7;
    p.lfoPms = (d[116] >> 4) & 7;
    p.transpose = d[117] & 0x7F;
    for (int j = 0; j < 10; ++j) p.name[(size_t) j] = (char) (d[118 + j] & 0x7F);
    p.clamp();
    return p;
}

juce::String Patch::toHex() const
{
    const auto v = toVced();
    juce::String s;
    s.preallocateBytes ((kVcedSize + 1) * 2);
    for (auto b : v) s << juce::String::toHexString ((int) b).paddedLeft ('0', 2);
    s << juce::String::toHexString ((int) opEnabled).paddedLeft ('0', 2);
    return s;
}

bool Patch::fromHex (const juce::String& hex, Patch& out)
{
    if (hex.length() != (kVcedSize + 1) * 2 || ! hex.containsOnly ("0123456789abcdefABCDEF")) return false;
    std::array<uint8_t, kVcedSize + 1> d {};
    for (int i = 0; i <= kVcedSize; ++i) d[(size_t) i] = (uint8_t) hex.substring (i * 2, i * 2 + 2).getHexValue32();
    out = fromVced (d.data());
    out.opEnabled = d[kVcedSize] & 0x3F;
    return true;
}

Patch Patch::init()
{
    Patch p;
    for (auto& o : p.op) o.outputLevel = 0;
    p.op[0].outputLevel = 99;
    p.algorithm = 0;
    return p;
}

// =================================================================================
uint8_t checksum (const uint8_t* data, int size)
{
    int sum = 0;
    for (int i = 0; i < size; ++i) sum += data[i];
    return (uint8_t) ((128 - (sum & 0x7F)) & 0x7F);
}

SysexResult parseSysex (const juce::MemoryBlock& block)
{
    SysexResult r;
    const auto* b = static_cast<const uint8_t*> (block.getData());
    const int n = (int) block.getSize();
    if (n == 0) { r.error = "The file is empty."; return r; }
    if (n > 4 * 1024 * 1024) { r.error = "The file is too large for a SysEx cartridge."; return r; }

    juce::StringArray notes;
    bool any = false;
    for (int i = 0; i < n; ++i)
    {
        if (b[i] != 0xF0) continue;
        int end = i + 1;
        while (end < n && b[end] != 0xF7) ++end;
        if (end >= n) { notes.add ("an unterminated SysEx message was ignored"); break; }
        const uint8_t* m = b + i;
        const int len = end - i + 1;
        if (len >= 8 && m[1] == 0x43 && (m[2] & 0xF0) == 0x00)
        {
            const int format = m[3];
            const int count = (m[4] << 7) | m[5];
            if (format == 0x09 && count == 4096 && len == 4096 + 8)
            {
                if (checksum (m + 6, 4096) != m[6 + 4096]) { r.checksumOk = false; notes.add ("bank checksum mismatch"); }
                for (int v = 0; v < kBankVoices; ++v) r.voices.push_back (Patch::fromPacked (m + 6 + v * kPackedSize));
                r.isBank = true;
                any = true;
            }
            else if (format == 0x00 && count == kVcedSize && len == kVcedSize + 8)
            {
                if (checksum (m + 6, kVcedSize) != m[6 + kVcedSize]) { r.checksumOk = false; notes.add ("voice checksum mismatch"); }
                r.voices.push_back (Patch::fromVced (m + 6));
                any = true;
            }
            else
                notes.add ("an unsupported Yamaha-format message (format " + juce::String (format) + ", " + juce::String (count) + " bytes) was ignored");
        }
        i = end;
    }
    if (! any && n == 4096)
    {
        for (int v = 0; v < kBankVoices; ++v) r.voices.push_back (Patch::fromPacked (b + v * kPackedSize));
        r.isBank = true;
        notes.add ("raw 4096-byte bank without a SysEx header (no checksum to verify)");
        any = true;
    }
    if (! any)
    {
        r.error = "No 32-voice bank (format 9) or single voice (format 0) of the six-operator FM SysEx format was found.";
        return r;
    }
    r.ok = true;
    r.warning = notes.joinIntoString ("; ");
    return r;
}

SysexResult loadSysexFile (const juce::File& f)
{
    SysexResult r;
    if (! f.existsAsFile()) { r.error = "File not found: " + f.getFullPathName(); return r; }
    if (f.getSize() > 4 * 1024 * 1024) { r.error = "The file is too large for a SysEx cartridge."; return r; }
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) { r.error = "Could not read " + f.getFileName(); return r; }
    return parseSysex (mb);
}

juce::MemoryBlock makeBankSysex (const std::vector<Patch>& voices)
{
    std::vector<uint8_t> data (4096, 0);
    for (int v = 0; v < kBankVoices; ++v)
    {
        const auto p = v < (int) voices.size() ? voices[(size_t) v] : Patch::init();
        const auto packed = p.toPacked();
        std::copy (packed.begin(), packed.end(), data.begin() + v * kPackedSize);
    }
    juce::MemoryOutputStream out;
    const uint8_t head[] = { 0xF0, 0x43, 0x00, 0x09, 0x20, 0x00 };
    out.write (head, sizeof (head));
    out.write (data.data(), data.size());
    out.writeByte ((char) checksum (data.data(), 4096));
    out.writeByte ((char) 0xF7);
    return out.getMemoryBlock();
}

juce::MemoryBlock makeVoiceSysex (const Patch& p)
{
    const auto v = p.toVced();
    juce::MemoryOutputStream out;
    const uint8_t head[] = { 0xF0, 0x43, 0x00, 0x00, 0x01, 0x1B };
    out.write (head, sizeof (head));
    out.write (v.data(), v.size());
    out.writeByte ((char) checksum (v.data(), kVcedSize));
    out.writeByte ((char) 0xF7);
    return out.getMemoryBlock();
}

// =================================================================================
// Original factory voices (designed for this project).
namespace
{
    Operator opr (int coarse, int fine, int level, std::array<uint8_t, 4> r, std::array<uint8_t, 4> l, int vel = 2, int detune = 7, int rs = 2)
    {
        Operator o;
        o.coarse = (uint8_t) coarse; o.fine = (uint8_t) fine; o.outputLevel = (uint8_t) level;
        o.rate = r; o.level = l; o.velSens = (uint8_t) vel; o.detune = (uint8_t) detune; o.rateScale = (uint8_t) rs;
        return o;
    }

    Patch make (const char* nm, int algo, int fb, std::array<Operator, 6> ops)
    {
        Patch p;
        p.op = ops;
        p.algorithm = (uint8_t) (algo - 1);
        p.feedback = (uint8_t) fb;
        p.setName (nm);
        p.clamp();
        return p;
    }
}

const std::vector<Patch>& factoryVoices()
{
    static const std::vector<Patch> voices = []
    {
        std::vector<Patch> v;
        // tine-like electric piano: three carrier / modulator pairs (algorithm 5)
        v.push_back (make ("TINE PIANO", 5, 5, { opr (1, 0, 99, { 96, 28, 24, 62 }, { 99, 82, 0, 0 }, 2, 6),
                                                   opr (13, 0, 54, { 95, 52, 30, 70 }, { 99, 20, 0, 0 }, 7, 7),
                                                   opr (1, 0, 93, { 95, 30, 24, 62 }, { 99, 80, 0, 0 }, 2, 8),
                                                   opr (1, 0, 74, { 95, 40, 26, 65 }, { 99, 60, 0, 0 }, 6, 6),
                                                   opr (1, 0, 80, { 94, 32, 24, 60 }, { 99, 78, 0, 0 }, 2, 9),
                                                   opr (1, 0, 66, { 92, 45, 28, 64 }, { 99, 55, 0, 0 }, 5, 5) }));
        // glassy bell: inharmonic 3.5 modulators, long decays
        v.push_back (make ("GLASS BELL", 5, 3, { opr (1, 0, 99, { 98, 22, 20, 30 }, { 99, 70, 0, 0 }, 3, 7),
                                                    opr (3, 50, 70, { 98, 30, 22, 32 }, { 99, 50, 0, 0 }, 5, 8),
                                                    opr (2, 0, 88, { 98, 24, 20, 30 }, { 99, 66, 0, 0 }, 3, 6),
                                                    opr (7, 3, 62, { 98, 34, 24, 34 }, { 99, 40, 0, 0 }, 5, 7),
                                                    opr (4, 0, 80, { 98, 26, 20, 30 }, { 99, 60, 0, 0 }, 3, 9),
                                                    opr (9, 41, 58, { 98, 38, 25, 36 }, { 99, 30, 0, 0 }, 5, 5) }));
        // solid bass: one carrier, stacked modulators (algorithm 16)
        v.push_back (make ("SOLID BASS", 16, 6, { opr (1, 0, 99, { 99, 40, 30, 72 }, { 99, 86, 70, 0 }, 3, 7),
                                                     opr (1, 0, 78, { 99, 55, 35, 70 }, { 99, 60, 40, 0 }, 6, 7),
                                                     opr (1, 0, 70, { 99, 45, 30, 70 }, { 99, 70, 50, 0 }, 4, 8),
                                                     opr (2, 0, 60, { 99, 60, 40, 70 }, { 99, 50, 30, 0 }, 6, 7),
                                                     opr (1, 0, 66, { 99, 50, 30, 70 }, { 99, 60, 40, 0 }, 4, 6),
                                                     opr (3, 0, 50, { 99, 65, 40, 70 }, { 99, 40, 20, 0 }, 6, 7) }));
        // brass section: shared modulator, slow attack (algorithm 22)
        v.push_back (make ("BRASS SECT", 22, 6, { opr (1, 0, 97, { 62, 40, 30, 55 }, { 99, 92, 88, 0 }, 2, 7),
                                                     opr (1, 0, 76, { 60, 42, 30, 55 }, { 99, 86, 80, 0 }, 4, 7),
                                                     opr (1, 0, 95, { 64, 40, 30, 55 }, { 99, 92, 88, 0 }, 2, 6),
                                                     opr (1, 0, 94, { 63, 40, 30, 55 }, { 99, 92, 88, 0 }, 2, 8),
                                                     opr (1, 0, 93, { 61, 40, 30, 55 }, { 99, 92, 88, 0 }, 2, 9),
                                                     opr (1, 0, 78, { 55, 38, 30, 55 }, { 99, 88, 82, 0 }, 5, 7) }));
        // mallet / marimba (algorithm 7)
        v.push_back (make ("MALLETS", 7, 0, { opr (1, 0, 99, { 99, 42, 30, 50 }, { 99, 62, 0, 0 }, 3, 7),
                                                  opr (4, 0, 66, { 99, 70, 40, 60 }, { 99, 0, 0, 0 }, 6, 7),
                                                  opr (1, 0, 95, { 99, 45, 30, 50 }, { 99, 58, 0, 0 }, 3, 7),
                                                  opr (10, 0, 54, { 99, 80, 40, 60 }, { 99, 0, 0, 0 }, 7, 7),
                                                  opr (3, 0, 58, { 99, 75, 40, 60 }, { 99, 0, 0, 0 }, 6, 7),
                                                  opr (1, 0, 40, { 99, 70, 40, 60 }, { 99, 0, 0, 0 }, 4, 7) }));
        // organ: all carriers at drawbar-like ratios (algorithm 32)
        v.push_back (make ("DRAWBARS", 32, 2, { opr (0, 0, 90, { 99, 99, 99, 80 }, { 99, 99, 99, 0 }, 0, 7),
                                                    opr (1, 0, 95, { 99, 99, 99, 80 }, { 99, 99, 99, 0 }, 0, 7),
                                                    opr (2, 0, 88, { 99, 99, 99, 80 }, { 99, 99, 99, 0 }, 0, 8),
                                                    opr (3, 0, 82, { 99, 99, 99, 80 }, { 99, 99, 99, 0 }, 0, 6),
                                                    opr (4, 0, 80, { 99, 99, 99, 80 }, { 99, 99, 99, 0 }, 0, 7),
                                                    opr (6, 0, 74, { 99, 99, 99, 80 }, { 99, 99, 99, 0 }, 0, 7) }));
        // harp pluck (algorithm 1)
        v.push_back (make ("HARP PLUCK", 1, 4, { opr (1, 0, 99, { 99, 35, 25, 45 }, { 99, 60, 0, 0 }, 3, 7),
                                                    opr (2, 0, 72, { 99, 50, 30, 50 }, { 99, 30, 0, 0 }, 6, 7),
                                                    opr (1, 0, 96, { 99, 38, 25, 45 }, { 99, 58, 0, 0 }, 3, 8),
                                                    opr (1, 0, 70, { 99, 55, 30, 50 }, { 99, 30, 0, 0 }, 5, 6),
                                                    opr (3, 0, 60, { 99, 60, 30, 50 }, { 99, 20, 0, 0 }, 5, 7),
                                                    opr (1, 0, 55, { 99, 62, 30, 50 }, { 99, 10, 0, 0 }, 4, 7) }));
        // evolving metallic pad (algorithm 2, slow)
        v.push_back (make ("METAL PAD", 2, 5, { opr (1, 0, 96, { 40, 30, 20, 35 }, { 99, 90, 85, 0 }, 1, 7),
                                                    opr (1, 1, 74, { 35, 25, 20, 35 }, { 99, 80, 70, 0 }, 2, 9),
                                                    opr (1, 0, 94, { 42, 30, 20, 35 }, { 99, 90, 85, 0 }, 1, 5),
                                                    opr (3, 0, 64, { 30, 22, 18, 35 }, { 99, 70, 60, 0 }, 2, 7),
                                                    opr (1, 50, 58, { 28, 20, 18, 35 }, { 99, 65, 55, 0 }, 2, 8),
                                                    opr (5, 0, 50, { 25, 20, 18, 35 }, { 99, 60, 50, 0 }, 2, 7) }));
        // bright lead with heavy feedback (algorithm 1)
        v.push_back (make ("FB LEAD", 1, 7, { opr (1, 0, 99, { 92, 40, 30, 60 }, { 99, 90, 85, 0 }, 2, 7),
                                                   opr (1, 0, 80, { 92, 40, 30, 60 }, { 99, 85, 80, 0 }, 3, 8),
                                                   opr (1, 0, 96, { 92, 40, 30, 60 }, { 99, 90, 85, 0 }, 2, 6),
                                                   opr (2, 0, 70, { 92, 45, 30, 60 }, { 99, 80, 70, 0 }, 3, 7),
                                                   opr (1, 0, 72, { 92, 45, 30, 60 }, { 99, 80, 70, 0 }, 3, 7),
                                                   opr (1, 0, 76, { 92, 45, 30, 60 }, { 99, 80, 70, 0 }, 3, 7) }));
        // funky clav-like (algorithm 3)
        v.push_back (make ("FM CLAV", 3, 6, { opr (1, 0, 99, { 99, 52, 40, 70 }, { 99, 74, 0, 0 }, 4, 7),
                                                   opr (3, 0, 80, { 99, 70, 40, 70 }, { 99, 40, 0, 0 }, 6, 7),
                                                   opr (5, 0, 66, { 99, 75, 40, 70 }, { 99, 20, 0, 0 }, 6, 7),
                                                   opr (1, 0, 95, { 99, 52, 40, 70 }, { 99, 74, 0, 0 }, 4, 8),
                                                   opr (2, 0, 70, { 99, 70, 40, 70 }, { 99, 30, 0, 0 }, 6, 6),
                                                   opr (7, 0, 52, { 99, 80, 40, 70 }, { 99, 10, 0, 0 }, 6, 7) }));
        // digital strings (algorithm 2, slow attack, light modulation)
        v.push_back (make ("DIG STRING", 2, 3, { opr (1, 0, 97, { 52, 30, 25, 40 }, { 99, 92, 90, 0 }, 1, 6),
                                                    opr (1, 0, 62, { 50, 30, 25, 40 }, { 99, 85, 80, 0 }, 2, 8),
                                                    opr (1, 0, 95, { 54, 30, 25, 40 }, { 99, 92, 90, 0 }, 1, 8),
                                                    opr (1, 0, 64, { 48, 30, 25, 40 }, { 99, 85, 80, 0 }, 2, 6),
                                                    opr (2, 0, 52, { 46, 30, 25, 40 }, { 99, 80, 75, 0 }, 2, 7),
                                                    opr (1, 0, 50, { 46, 30, 25, 40 }, { 99, 80, 75, 0 }, 2, 7) }));
        // tubular chime (algorithm 5, inharmonic)
        v.push_back (make ("TUBULAR", 5, 2, { opr (1, 0, 99, { 98, 20, 18, 26 }, { 99, 60, 0, 0 }, 2, 7),
                                                   opr (3, 50, 72, { 98, 26, 20, 28 }, { 99, 45, 0, 0 }, 4, 8),
                                                   opr (2, 74, 90, { 98, 22, 18, 26 }, { 99, 58, 0, 0 }, 2, 6),
                                                   opr (5, 20, 64, { 98, 28, 20, 28 }, { 99, 40, 0, 0 }, 4, 7),
                                                   opr (4, 17, 82, { 98, 24, 18, 26 }, { 99, 55, 0, 0 }, 2, 9),
                                                   opr (7, 0, 56, { 98, 30, 20, 28 }, { 99, 30, 0, 0 }, 4, 5) }));
        return v;
    }();
    return voices;
}

float releaseSeconds (const Patch& v)
{
    float rel = 0.05f;
    const auto& al = algorithm (v.algorithm);
    for (int i = 0; i < kOps; ++i)
        if ((al.carriers >> i) & 1)
        {
            const auto& o = v.op[(size_t) i];
            const float span = std::max (0, (int) o.level[2] - (int) o.level[3]) / 99.0f;
            rel = std::max (rel, FmRenderer::rateToSeconds (o.rate[3], false) * span + 0.05f);
        }
    return juce::jlimit (0.05f, 12.0f, rel);
}

// =================================================================================
float FmRenderer::rateToSeconds (float rate, bool rising) noexcept
{
    // this project's own curve: rate 99 crosses the full range in about 9 ms (1.5 ms rising), rate 0 in about 38 s
    const float t = 38.0f * std::exp2 (-juce::jlimit (0.0f, 99.0f, rate) / 8.2f);
    return rising ? t / 6.0f : t;
}

void FmRenderer::prepare (double sampleRate)
{
    sr = sampleRate;
    local = Patch::init();
    if (patch == nullptr) patch = &local;
}

void FmRenderer::setPatch (const Patch* p) noexcept
{
    patch = p != nullptr ? p : &local;
}

void FmRenderer::noteOn (const Patch& p, int n, int vel, float pitchSemis, uint32_t seed)
{
    note = n;
    velocity = juce::jlimit (1, 127, vel);
    basePitch = pitchSemis;
    rng = seed | 1u;
    if (patch == nullptr) patch = &p;
    const auto& P = *patch;
    for (int i = 0; i < kOps; ++i)
    {
        const auto& o = P.op[(size_t) i];
        if (P.oscSync != 0) phase[(size_t) i] = 0;
        else { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; phase[(size_t) i] = rng; }
        out[(size_t) i] = 0.0f;
        eg[(size_t) i] = {};
        eg[(size_t) i].level = o.level[3];
        // keyboard level scaling
        const int bp = o.breakPoint + 21;
        const int dist = note - bp;
        const int depth = dist < 0 ? o.leftDepth : o.rightDepth;
        const int curve = dist < 0 ? o.leftCurve : o.rightCurve;
        const float oct = (float) std::abs (dist) / 12.0f;
        float ks = 0.0f;
        if (dist != 0 && depth > 0)
        {
            const float shape = (curve == 0 || curve == 3) ? oct / 3.0f : (std::exp2 (oct) - 1.0f) / 7.0f;
            ks = (float) depth / 99.0f * std::min (2.0f, shape) * 48.0f * ((curve == 0 || curve == 1) ? -1.0f : 1.0f);
        }
        const float vel01 = (float) velocity / 127.0f;
        const float velDb = -(float) o.velSens / 7.0f * (1.0f - vel01) * 28.0f;
        staticDb[(size_t) i] = levelToDb (o.outputLevel) + ks + velDb - (o.outputLevel == 0 ? 60.0f : 0.0f);
        rateBoost[(size_t) i] = (float) o.rateScale * (float) std::max (0, note - 21) / 22.0f;
        ampCur[(size_t) i] = 0.0f;
        ampStep[(size_t) i] = 0.0f;
    }
    pitchEg = {};
    pitchEg.level = P.pitchLevel[3];
    pitchEgLevel = pitchEg.level;
    fb1 = fb2 = 0.0f;
    if (P.lfoSync != 0) lfoPhase = 0.0;
    lfoAge = 0.0f;
    controlCounter = 0;
}

void FmRenderer::noteOff() noexcept
{
    for (auto& e : eg)
        if (e.stage < 4) { e.stage = 4; e.released = true; }
    if (pitchEg.stage < 4) pitchEg.stage = 4;
}

bool FmRenderer::finished() const noexcept
{
    const auto& al = algorithm (patch->algorithm);
    for (int i = 0; i < kOps; ++i)
        if (((al.carriers >> i) & 1) != 0 && ((patch->opEnabled >> i) & 1) != 0)
            if (eg[(size_t) i].stage < 5) return false;
    return true;
}

float FmRenderer::egDb (int op) const noexcept
{
    const float l = eg[(size_t) op].level;
    return l <= 0.5f ? -120.0f : levelToDb (l);
}

void FmRenderer::stepEg (Eg& e, const Operator& o, float boost, int n) noexcept
{
    if (e.stage == 3 || e.stage >= 5) return;
    const int seg = e.stage == 4 ? 3 : e.stage;
    const float target = o.level[(size_t) seg];
    const bool rising = target > e.level;
    const float seconds = rateToSeconds ((float) o.rate[(size_t) seg] + boost, rising);
    const float step = 99.0f / std::max (1.0e-4f, seconds) * (float) n / (float) sr;
    if (rising) e.level = std::min (target, e.level + step);
    else e.level = std::max (target, e.level - step);
    if (std::abs (e.level - target) < 1.0e-4f)
    {
        if (e.stage == 4) e.stage = 5;
        else ++e.stage; // 0 -> 1 -> 2 -> 3 (sustain)
    }
}

void FmRenderer::updateControl() noexcept
{
    const auto& P = *patch;
    const int n = kControl;
    for (int i = 0; i < kOps; ++i) stepEg (eg[(size_t) i], P.op[(size_t) i], rateBoost[(size_t) i], n);
    // pitch envelope (level 50 = no shift, +/-4 octaves at the extremes, finer around the centre)
    {
        Operator po;
        po.rate = P.pitchRate;
        po.level = P.pitchLevel;
        stepEg (pitchEg, po, 0.0f, n);
    }
    const float pd = (pitchEg.level - 50.0f) / 50.0f;
    const float pitchEgSemis = (pd < 0.0f ? -1.0f : 1.0f) * std::pow (std::abs (pd), 1.6f) * 48.0f;

    // LFO
    const float lfoHz = 0.06f * std::exp ((float) P.lfoSpeed * 0.0679f);
    lfoPhase += lfoHz * n / sr;
    bool wrapped = false;
    if (lfoPhase >= 1.0) { lfoPhase -= std::floor (lfoPhase); wrapped = true; }
    const float ph = (float) lfoPhase;
    switch (P.lfoWave)
    {
        case 0: lfoValue = 1.0f - 4.0f * std::abs (ph - 0.5f); break;      // triangle
        case 1: lfoValue = 1.0f - 2.0f * ph; break;                        // saw down
        case 2: lfoValue = 2.0f * ph - 1.0f; break;                        // saw up
        case 3: lfoValue = ph < 0.5f ? 1.0f : -1.0f; break;                // square
        case 4: lfoValue = std::sin (6.2831853f * ph); break;              // sine
        default:
            if (wrapped) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; lfoHold = (float) (rng & 0xFFFF) / 32767.5f - 1.0f; }
            lfoValue = lfoHold;
            break;
    }
    lfoAge += (float) n / (float) sr;
    const float delay = P.lfoDelay == 0 ? 0.0f : 0.05f * std::exp2 ((float) P.lfoDelay / 14.9f);
    const float fade = delay <= 0.0f ? 1.0f : juce::jlimit (0.0f, 1.0f, (lfoAge - delay) / (0.5f * delay + 0.01f));
    static constexpr float kPms[8] = { 0.0f, 0.05f, 0.1f, 0.17f, 0.3f, 0.55f, 1.2f, 4.0f };
    const float lfoSemis = kPms[P.lfoPms & 7] * ((float) P.lfoPmd / 99.0f) * lfoValue * fade;
    const float lfoUni = 0.5f * (lfoValue + 1.0f) * fade;

    const float pitch = basePitch + (float) ((int) P.transpose - 24) + pitchEgSemis + lfoSemis;
    const double f0 = 440.0 * std::exp2 ((pitch - 69.0) / 12.0);
    static constexpr float kAms[4] = { 0.0f, 0.25f, 0.55f, 1.0f };
    for (int i = 0; i < kOps; ++i)
    {
        const auto& o = P.op[(size_t) i];
        double f;
        if (o.fixedMode != 0) f = std::pow (10.0, (double) (o.coarse & 3)) * std::pow (10.0, o.fine / 100.0);
        else f = f0 * (o.coarse == 0 ? 0.5 : (double) o.coarse) * (1.0 + o.fine / 100.0);
        f *= std::exp2 (((int) o.detune - 7) * 1.5 / 1200.0);
        const double ratio = std::min (0.49, f / sr);
        inc[(size_t) i] = (uint32_t) (ratio * 4294967296.0);
        float db = staticDb[(size_t) i] + egDb (i) - kAms[o.ampModSens & 3] * ((float) P.lfoAmd / 99.0f) * lfoUni * 26.0f;
        db = std::min (0.0f, db);         // scaling can raise a level only up to the maximum, never beyond it
        if (f / sr >= 0.49) db = -120.0f; // above Nyquist: silent rather than aliased
        const float target = db <= -96.0f ? 0.0f : std::pow (10.0f, db / 20.0f);
        ampStep[(size_t) i] = (target - ampCur[(size_t) i]) / (float) n;
    }
}

float FmRenderer::render() noexcept
{
    if (--controlCounter <= 0)
    {
        updateControl();
        controlCounter = kControl;
    }
    const auto& P = *patch;
    const auto& al = algorithm (P.algorithm);
    const auto& sn = sine();
    constexpr float kModCycles = 2.0f; // full-level modulator = +/-2 cycles of phase deviation
    const float fbCycles = P.feedback == 0 ? 0.0f : 0.25f * std::exp2 ((float) P.feedback - 5.0f);
    float sum = 0.0f;
    for (int i = kOps - 1; i >= 0; --i)
    {
        ampCur[(size_t) i] += ampStep[(size_t) i];
        if (((P.opEnabled >> i) & 1) == 0) { out[(size_t) i] = 0.0f; phase[(size_t) i] += inc[(size_t) i]; continue; }
        float mod = 0.0f;
        const uint8_t mods = al.modulators[(size_t) i];
        for (int j = i + 1; j < kOps; ++j)
            if ((mods >> j) & 1) mod += out[(size_t) j];
        float cycles = mod * kModCycles * amount;
        if (i == al.feedbackTarget) cycles += 0.5f * (fb1 + fb2) * fbCycles;
        const auto offset = (uint32_t) (int64_t) (cycles * 4294967296.0f);
        const float y = sn (phase[(size_t) i] + offset) * std::max (0.0f, ampCur[(size_t) i]);
        out[(size_t) i] = y;
        phase[(size_t) i] += inc[(size_t) i];
        if (i == al.feedbackSource) { fb2 = fb1; fb1 = y; }
        if ((al.carriers >> i) & 1) sum += y;
    }
    // feedback from an operator computed after its target (algorithms 4 and 6) is picked up on the next sample
    return sum / std::sqrt ((float) std::max (1, al.numCarriers()));
}

} // namespace vsx::fm
