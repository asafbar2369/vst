#pragma once
// A synth's "temporary area" (the bytes its SysEx addresses), a bank of received patch memory
// slots, and the host parameters bound to individual bytes. Used for both the D-50 (448 bytes)
// and the JD-800 (384 bytes).
//
// Threading: bytes are atomics. The audio thread copies changed parameter values into the area
// (applyParameters) and writes SysEx data; the message thread pushes SysEx-driven changes back
// into the host parameters (syncParameters) using per-byte dirty flags.

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class ParamArea
{
public:
    enum class Kind { Byte, Waveform, LayerBit }; // Waveform: 2 bytes (MSB, LSB); LayerBit: one bit of a byte

    struct Spec
    {
        int size = 0;                    // temporary area size in bytes
        int numSlots = 0;                // patch memory slots
        uint32_t bankBase = 0;           // linear address of slot 0
        int slotStride = 0;              // bytes per slot (== size)
        std::function<int (int)> minFor, maxFor;        // documented range per offset
        std::function<void (uint8_t*)> writeInit;     // our neutral init patch
    };

    explicit ParamArea (Spec s)
        : spec (std::move (s)),
          temp (std::make_unique<std::atomic<uint8_t>[]> ((size_t) spec.size)),
          bank (std::make_unique<std::atomic<uint8_t>[]> ((size_t) (spec.numSlots * spec.slotStride))),
          bankValid (std::make_unique<std::atomic<bool>[]> ((size_t) spec.numSlots)),
          dirty (std::make_unique<std::atomic<uint64_t>[]> ((size_t) numWords()))
    {
        std::vector<uint8_t> init ((size_t) spec.size);
        spec.writeInit (init.data());
        for (int slot = 0; slot < spec.numSlots; ++slot)
        {
            bankValid[(size_t) slot].store (false);
            for (int i = 0; i < spec.size; ++i)
                bank[(size_t) (slot * spec.slotStride + i)].store (init[(size_t) i]);
        }
        resetToInit();
    }

    int size() const noexcept { return spec.size; }
    int numSlots() const noexcept { return spec.numSlots; }
    uint8_t get (int offset) const noexcept { return temp[(size_t) offset].load (std::memory_order_relaxed); }
    bool isSlotValid (int slot) const noexcept { return bankValid[(size_t) slot].load(); }
    uint8_t getBank (int slot, int offset) const noexcept { return bank[(size_t) (slot * spec.slotStride + offset)].load(); }
    uint8_t clampFor (int offset, int v) const noexcept
    {
        return (uint8_t) juce::jlimit (spec.minFor (offset), spec.maxFor (offset), v & 0x7F);
    }

    void resetToInit() noexcept
    {
        std::vector<uint8_t> init ((size_t) spec.size);
        spec.writeInit (init.data());
        for (int i = 0; i < spec.size; ++i) temp[(size_t) i].store (init[(size_t) i]);
        for (int w = 0; w < numWords(); ++w) dirty[(size_t) w].store (0);
    }

    // Writes one byte addressed linearly (temporary area or patch memory). Returns false if ignored.
    bool write (uint32_t linear, int value) noexcept
    {
        if (linear < (uint32_t) spec.size)
        {
            temp[linear].store (clampFor ((int) linear, value));
            markDirty ((int) linear);
            return true;
        }
        if (linear >= spec.bankBase && linear < spec.bankBase + uint32_t (spec.numSlots * spec.slotStride))
        {
            const uint32_t rel = linear - spec.bankBase;
            const int slot = int (rel / (uint32_t) spec.slotStride), off = int (rel % (uint32_t) spec.slotStride);
            bank[rel].store (clampFor (off, value));
            bankValid[(size_t) slot].store (true);
            return true;
        }
        return false;
    }

    bool readForReply (uint32_t linear, uint8_t& out) const noexcept
    {
        if (linear < (uint32_t) spec.size) { out = get ((int) linear); return true; }
        if (linear >= spec.bankBase && linear < spec.bankBase + uint32_t (spec.numSlots * spec.slotStride))
        {
            out = bank[linear - spec.bankBase].load();
            return true;
        }
        return false;
    }
    bool isAddressRange (uint32_t address, uint32_t size) const noexcept
    {
        if (size == 0) return false;
        const uint32_t end = address + size;
        return end <= (uint32_t) spec.size
            || (address >= spec.bankBase && end <= spec.bankBase + uint32_t (spec.numSlots * spec.slotStride));
    }

    void loadSlotIntoTemp (int slot) noexcept
    {
        for (int i = 0; i < spec.size; ++i) write ((uint32_t) i, getBank (slot, i));
    }

    // ---- bound host parameters ----
    void bind (juce::RangedAudioParameter* p, std::atomic<float>* v, int offset, Kind kind, int bit = 0)
    {
        bound.push_back ({ p, v, offset, kind, bit, -1 });
    }

    // Audio thread: copy changed parameter values into the area.
    void applyParameters() noexcept
    {
        const bool force = forceParams.exchange (false);
        if (resync.exchange (false))
            for (auto& b : bound) b.lastSeen = (int) std::lround (b.value->load());
        for (auto& b : bound)
        {
            const int v = (int) std::lround (b.value->load (std::memory_order_relaxed));
            if (! force && v == b.lastSeen) continue;
            b.lastSeen = v;
            storeBound (b, v, temp.get());
        }
    }

    // Message thread: push SysEx/UI driven byte changes into the host parameters.
    void syncParameters()
    {
        std::vector<uint64_t> mask ((size_t) numWords());
        bool any = false;
        for (int w = 0; w < numWords(); ++w) { mask[(size_t) w] = dirty[(size_t) w].exchange (0); any = any || mask[(size_t) w] != 0; }
        if (! any) return;
        auto isDirty = [&mask] (int o) { return (mask[(size_t) (o / 64)] >> (o % 64)) & 1u; };
        for (auto& b : bound)
        {
            if (! isDirty (b.offset) && ! (b.kind == Kind::Waveform && isDirty (b.offset + 1))) continue;
            const int raw = boundValueFromArea (b);
            if ((int) std::lround (b.value->load()) != raw)
                b.param->setValueNotifyingHost (b.param->convertTo0to1 ((float) raw));
        }
    }

    // Copy of the area; parameter values not yet applied by the audio thread are overlaid.
    void snapshot (uint8_t* out) const noexcept
    {
        for (int i = 0; i < spec.size; ++i) out[i] = get (i);
        for (auto& b : bound)
        {
            const bool pending = ((dirty[(size_t) (b.offset / 64)].load() >> (b.offset % 64)) & 1u) != 0;
            if (! pending) storeBoundPlain (b, (int) std::lround (b.value->load()), out);
        }
    }

    void loadTemp (const uint8_t* data) noexcept // from saved state: the area is the source of truth
    {
        for (int i = 0; i < spec.size; ++i) temp[(size_t) i].store (clampFor (i, data[i]));
        resync = true;
        for (auto& b : bound) markDirty (b.offset);
    }
    void loadSlot (int slot, const uint8_t* data) noexcept
    {
        for (int i = 0; i < spec.size; ++i) bank[(size_t) (slot * spec.slotStride + i)].store (clampFor (i, data[i]));
        bankValid[(size_t) slot].store (true);
    }
    void forceParametersToArea() noexcept { forceParams = true; }

private:
    struct Bound { juce::RangedAudioParameter* param; std::atomic<float>* value; int offset; Kind kind; int bit; int lastSeen; };

    int numWords() const noexcept { return (spec.size + 63) / 64; }
    void markDirty (int o) noexcept { dirty[(size_t) (o / 64)].fetch_or (uint64_t (1) << (o % 64)); }

    int boundValueFromArea (const Bound& b) const noexcept
    {
        switch (b.kind)
        {
            case Kind::Waveform: return (get (b.offset) << 7) | get (b.offset + 1);
            case Kind::LayerBit: return (get (b.offset) >> b.bit) & 1;
            case Kind::Byte:
            default:             return get (b.offset);
        }
    }
    void storeBound (const Bound& b, int v, std::atomic<uint8_t>* area) noexcept
    {
        switch (b.kind)
        {
            case Kind::Waveform:
                area[(size_t) b.offset].store ((uint8_t) ((v >> 7) & 0x7F));
                area[(size_t) b.offset + 1].store ((uint8_t) (v & 0x7F));
                break;
            case Kind::LayerBit:
            {
                const uint8_t old = area[(size_t) b.offset].load();
                area[(size_t) b.offset].store ((uint8_t) (v ? (old | (1 << b.bit)) : (old & ~(1 << b.bit))));
                break;
            }
            case Kind::Byte:
            default:
                area[(size_t) b.offset].store (clampFor (b.offset, v));
                break;
        }
    }
    void storeBoundPlain (const Bound& b, int v, uint8_t* out) const noexcept
    {
        switch (b.kind)
        {
            case Kind::Waveform: out[b.offset] = (uint8_t) ((v >> 7) & 0x7F); out[b.offset + 1] = (uint8_t) (v & 0x7F); break;
            case Kind::LayerBit: out[b.offset] = (uint8_t) (v ? (out[b.offset] | (1 << b.bit)) : (out[b.offset] & ~(1 << b.bit))); break;
            case Kind::Byte:
            default:             out[b.offset] = clampFor (b.offset, v); break;
        }
    }

    Spec spec;
    std::unique_ptr<std::atomic<uint8_t>[]> temp, bank;
    std::unique_ptr<std::atomic<bool>[]> bankValid;
    std::unique_ptr<std::atomic<uint64_t>[]> dirty;
    std::vector<Bound> bound;
    std::atomic<bool> forceParams { true }, resync { false };
};
