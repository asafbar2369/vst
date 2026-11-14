#pragma once
#include <array>
#include <cstddef>

// In the combo processor the same engine pitch can be addressed by more than
// one physical MIDI key (different source channels and/or transpose changes).
// Keep the synthesizer note held until its last source key is released.
namespace dc::midi
{
constexpr std::size_t kSourceKeys = 16u * 128u;
using NoteMap = std::array<int, kSourceKeys>;

inline bool hasOwner (const NoteMap& mapping, int pitch, int channel = 0) noexcept
{
    if (pitch < 0 || pitch > 127 || channel < 0 || channel > 16) return false;
    const std::size_t start = channel ? static_cast<std::size_t> (channel - 1) * 128u : 0u;
    const std::size_t end = channel ? start + 128u : mapping.size();
    for (std::size_t i = start; i < end; ++i)
        if (mapping[i] == pitch) return true;
    return false;
}
}
