#pragma once
// On-screen performance controls: pitch wheel, mod wheel, MIDI activity LED and output meter.
// They show the real state of the instrument (incoming MIDI moves them) and drive it through the
// same path as a hardware controller (a MIDI message queued for the audio thread) plus the host
// parameter, so a drag is also recorded as automation.

#include "PluginProcessor.h"

class WheelControl : public juce::Component, private juce::Timer
{
public:
    enum class Kind { Pitch, Mod };

    WheelControl (DigitalComboProcessor&, Kind);
    ~WheelControl() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // Pitch: 0..16383 (8192 centre). Mod: 0..127.
    int getDisplayedValue() const noexcept { return shown; }
    int valueForY (float y) const noexcept;
    void dragToValue (int value);   // what a mouse drag does; also used by the tests
    void endDrag();                 // pitch springs back to centre
    bool isDragging() const noexcept { return dragging; }
    juce::String getParameterId() const;
    juce::String getReadout() const;
    void refreshNow() { timerCallback(); }   // what the 30 Hz timer does; the tests call it directly

private:
    void timerCallback() override;
    int liveValue() const noexcept;
    juce::Rectangle<float> slot() const noexcept;

    DigitalComboProcessor& proc;
    Kind kind;
    juce::RangedAudioParameter* param = nullptr;
    int shown = 0;
    bool dragging = false;
};

// Lights for 180 ms after any MIDI message arrives.
class MidiActivityLed : public juce::Component, private juce::Timer
{
public:
    explicit MidiActivityLed (DigitalComboProcessor&);
    ~MidiActivityLed() override;
    void paint (juce::Graphics&) override;
    bool isLit() const noexcept { return lit; }
    void refreshNow() { timerCallback(); }
private:
    void timerCallback() override;
    DigitalComboProcessor& proc;
    uint32_t lastCount = 0;
    int holdMs = 0;
    bool lit = false;
};

// Stereo peak meter with a short fall-back.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (DigitalComboProcessor&);
    ~LevelMeter() override;
    void paint (juce::Graphics&) override;
    float getLevelDb (int channel) const noexcept { return levelDb[channel & 1]; }
    void refreshNow() { timerCallback(); }
private:
    void timerCallback() override;
    DigitalComboProcessor& proc;
    float levelDb[2] { -90.0f, -90.0f };
};
