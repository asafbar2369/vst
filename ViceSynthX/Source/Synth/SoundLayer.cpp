#include "SoundLayer.h"

namespace vsx
{
void LayerRouting::route (const juce::MidiBuffer& in, juce::MidiBuffer& out) noexcept
{
    // raw bytes only (juce::MidiMessage can allocate for long messages such as SysEx: not on the audio thread)
    out.clear();
    const int lo = keyLo.load(), hi = keyHi.load(), vlo = velLo.load(), vhi = velHi.load(), tr = transpose.load();
    for (const auto meta : in)
    {
        const uint8_t status = meta.numBytes > 0 ? meta.data[0] : 0;
        const int type = status & 0xF0, ch = status & 0x0F;
        const bool isOn = type == 0x90 && meta.numBytes >= 3 && meta.data[2] > 0;
        const bool isOff = (type == 0x80 || (type == 0x90 && meta.numBytes >= 3 && meta.data[2] == 0)) && meta.numBytes >= 3;
        if (! isOn && ! isOff) { out.addEvent (meta.data, meta.numBytes, meta.samplePosition); continue; }
        const int key = meta.data[1] & 0x7F, vel = meta.data[2] & 0x7F;
        auto& slot = started[(size_t) (ch * 128 + key)];
        if (isOn)
        {
            if (key < lo || key > hi || vel < vlo || vel > vhi) continue; // outside this layer's split / velocity range
            const int note = key + tr;
            if (note < 0 || note > 127) continue;
            slot = note;
            const uint8_t msg[3] = { (uint8_t) (0x90 | ch), (uint8_t) note, (uint8_t) vel };
            out.addEvent (msg, 3, meta.samplePosition);
        }
        else
        {
            if (slot < 0) continue; // this layer did not start the note
            const uint8_t msg[3] = { (uint8_t) (0x80 | ch), (uint8_t) slot, (uint8_t) vel };
            out.addEvent (msg, 3, meta.samplePosition);
            slot = -1;
        }
    }
}

SoundLayer::SoundLayer()
{
    const auto& reg = ParamRegistry::get();
    for (int i = 0; i < pid::kNumParams; ++i) values[(size_t) i].store (reg[i].defaultValue);
}

void SoundLayer::prepare (double sampleRate, int)
{
    engine.prepare (sampleRate, SynthEngine::kChunk);
    rack.prepare (sampleRate, SynthEngine::kChunk);
    buf.setSize (2, SynthEngine::kChunk);
    routed.ensureSize (2048);
}

void SoundLayer::setSound (const PatchSpec& spec)
{
    silence.store (true);
    for (int i = 0; i < pid::kNumParams; ++i) values[(size_t) i].store (spec.values[(size_t) i]);
    state.fromVar (spec.stateToVar(), true);
    sound = spec;
    userIr = spec.userIr;
    loaded = true;
}

void SoundLayer::clearSound()
{
    silence.store (true);
    enabled.store (false);
    sound = PatchSpec();
    loaded = false;
}

void SoundLayer::processChunk (const juce::MidiBuffer& midi, int n, const TransportInfo& t, float* outL, float* outR) noexcept
{
    if (silence.exchange (false))
    {
        engine.reset();
        rack.reset();
        routing.reset();
    }
    if (! enabled.load() || ! loaded) { routing.reset(); return; }
    n = std::min (n, buf.getNumSamples());
    for (int i = 0; i < pid::kNumParams; ++i) snap.value[(size_t) i] = values[(size_t) i].load (std::memory_order_relaxed);
    routing.route (midi, routed);
    juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, 0, n);
    view.clear();
    engine.process (view, routed, snap, t);
    FxContext fc { &snap, &engine.getGlobalMods(), t.bpm };
    rack.process (view.getWritePointer (0), view.getWritePointer (1), n, fc, state);
    // the layer's own Preset Volume, then its level and pan (equal-power)
    const float g = dbToGain (snap.get (pid::G (pid::global::MasterVolume)) + levelDb.load());
    const float p = juce::jlimit (-1.0f, 1.0f, pan.load());
    const float gl = g * std::cos ((p + 1.0f) * 0.25f * kPi) * 1.41421356f, gr = g * std::sin ((p + 1.0f) * 0.25f * kPi) * 1.41421356f;
    const float* l = view.getReadPointer (0);
    const float* r = view.getReadPointer (1);
    for (int i = 0; i < n; ++i) { outL[i] += l[i] * gl; outR[i] += r[i] * gr; }
}
} // namespace vsx
