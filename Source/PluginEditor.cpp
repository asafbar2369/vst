#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
const juce::Colour transparent (0x00000000);
}

RMX16AudioProcessorEditor::RMX16AudioProcessorEditor (RMX16AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    faceplate = juce::ImageFileFormat::loadFrom (BinaryData::user_reference_faceplate_png,
                                                  BinaryData::user_reference_faceplate_pngSize);
    setResizable (true, true);
    setResizeLimits (846, 158, 1586, 297);
    // Keep the photographed hardware faceplate proportional at every size.
    // JUCE hosts (including FL Studio) can resize the editor, while this
    // constrainer prevents horizontal/vertical stretching of the artwork.
    if (auto* editorConstrainer = getConstrainer())
        editorConstrainer->setFixedAspectRatio (1057.0 / 198.0);

    auto configureKnob = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::rotarySliderFillColourId, transparent);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, transparent);
        s.setColour (juce::Slider::thumbColourId, transparent);
        s.setTooltip ("Drag vertically to adjust");
    };
    configureKnob (input); configureKnob (output); configureKnob (pot);
    input.setRange (-24.0, 12.0, 0.1);
    output.setRange (-24.0, 12.0, 0.1);
    pot.setRange (0.0, 9.9, 0.1);
    pot.setValue (processor.parameters.getRawParameterValue ("decay")->load(), juce::dontSendNotification);
    inputA = std::make_unique<SliderAttachment> (processor.parameters, "input", input);
    outputA = std::make_unique<SliderAttachment> (processor.parameters, "output", output);
    addAndMakeVisible (input); addAndMakeVisible (output); addAndMakeVisible (pot);
    pot.onValueChange = [this] { if (! updatingPot) setSelectedValue ((float) pot.getValue()); };

    auto configureHitButton = [this] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, transparent);
        b.setColour (juce::TextButton::buttonOnColourId, transparent);
        b.setColour (juce::TextButton::textColourOffId, transparent);
        b.setColour (juce::TextButton::textColourOnId, transparent);
        b.setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    };
    for (auto* b : { &programButton, &predelayButton, &decayButton, &lowButton, &highButton,
                      &storeButton, &recallButton, &nudgeDown, &nudgeUp })
        configureHitButton (*b);

    // PROGRAM arms numeric selection; pressing a digit then # commits it.
    // Direct digit selection remains available as a convenient one-click shortcut.
    programButton.onClick = [this]
    {
        programEntryArmed = true;
        parameterEntryArmed = false;
        entryBuffer.clear();
        repaint();
    };
    predelayButton.onClick = [this] { selectPotTarget (echoBank == 1 && processor.getCurrentProgram() == 6 ? "echoBPredelay" : "predelay"); };
    decayButton.onClick = [this] { selectPotTarget (echoBank == 1 && processor.getCurrentProgram() == 6 ? "echoBDecay" : "decay"); };
    lowButton.onClick = [this] { selectPotTarget (echoBank == 1 && processor.getCurrentProgram() == 6 ? "echoBLow" : "low"); };
    highButton.onClick = [this] { selectPotTarget (echoBank == 1 && processor.getCurrentProgram() == 6 ? "echoBHigh" : "high"); };
    storeButton.onClick = [this] { processor.storeUserPreset(); };
    recallButton.onClick = [this] { processor.recallUserPreset(); };
    nudgeDown.onClick = [this]
    {
        if (programEntryArmed) processor.setCurrentProgram ((processor.getCurrentProgram() + 8) % 9);
        else nudgeSelected (-1.0f);
    };
    nudgeUp.onClick = [this]
    {
        if (programEntryArmed) processor.setCurrentProgram ((processor.getCurrentProgram() + 1) % 9);
        else nudgeSelected (1.0f);
    };

    const juce::String keys[16] = { "1", "2", "3", "A", "4", "5", "6", "B",
                                    "7", "8", "9", "C", "*", "0", "#", "D" };
    for (int i = 0; i < 16; ++i)
    {
        keypad[(size_t) i] = std::make_unique<juce::TextButton> ("");
        configureHitButton (*keypad[(size_t) i]);
        keypad[(size_t) i]->setTooltip ("Keypad " + keys[i]);
        if (i < 3 || (i >= 4 && i < 7) || (i >= 8 && i < 11))
        {
            keypad[(size_t) i]->onClick = [this, key = keys[i]] { handleKeypadDigit (key); };
        }
        else if (i == 3)
            keypad[(size_t) i]->onClick = [this] { echoBank = 0; selectPotTarget ("predelay"); };
        else if (i == 7)
            keypad[(size_t) i]->onClick = [this] { echoBank = 1; selectPotTarget ("echoBPredelay"); };
        else if (i == 13)
            keypad[(size_t) i]->onClick = [this] { handleKeypadDigit ("0"); };
        else if (i == 12)
            keypad[(size_t) i]->onClick = [this]
            {
                if (programEntryArmed) return;
                if (potTarget == "high" || potTarget == "low" || potTarget == "echoBHigh" || potTarget == "echoBLow")
                {
                    if (! entryBuffer.startsWith ("-")) entryBuffer = "-" + entryBuffer;
                }
                else if (potTarget == "decay" || potTarget == "echoBDecay")
                {
                    if (! entryBuffer.contains (".")) entryBuffer += ".";
                }
                parameterEntryArmed = true;
                repaint();
            };
        else if (i == 14)
            keypad[(size_t) i]->onClick = [this] { commitKeypadEntry(); };
        else if (i == 15)
            keypad[(size_t) i]->onClick = [this] { selectPotTarget ("predelay"); };
        addAndMakeVisible (*keypad[(size_t) i]);
    }

    display.setJustificationType (juce::Justification::centred);
    display.setColour (juce::Label::textColourId, juce::Colour (0xffff3030));
    display.setColour (juce::Label::backgroundColourId, juce::Colour (0xff050607));
    display.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    display.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (display);

    // Build all child controls before the first size change. JUCE may call
    // resized() synchronously from setSize(), and resized() positions keypad[].
    setSize (1057, 198);
    startTimerHz (10);
}

void RMX16AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (faceplate.isValid())
        g.drawImage (faceplate, getLocalBounds().toFloat());
    else
    {
        g.setColour (juce::Colour (0xff33393a));
        g.fillAll();
        g.setColour (juce::Colours::white);
        g.drawText ("RMX16 RESEARCH BUILD", getLocalBounds(), juce::Justification::centred);
    }

    // All overlay geometry is authored in the 1057x198 faceplate coordinate
    // system. Apply the same transform as the background so LEDs and control
    // markers remain registered when the editor is resized by the host.
    const float sx = (float) getWidth() / 1057.0f;
    const float sy = (float) getHeight() / 198.0f;
    g.addTransform (juce::AffineTransform::scale (sx, sy));

    // Draw the live red LED text over the photographed seven-segment display.
    auto screen = juce::Rectangle<float> (315.0f, 25.0f, 450.0f, 46.0f);
    g.setColour (juce::Colour (0xff050607));
    g.fillRoundedRectangle (screen, 2.0f);
    g.setColour (juce::Colour (0xff471313));
    g.drawRoundedRectangle (screen, 2.0f, 1.0f);

    // Needle markers move over the original photographed rotary controls.
    auto drawNeedle = [&g] (juce::Point<float> c, float norm, float radius)
    {
        const float angle = juce::jmap (juce::jlimit (0.0f, 1.0f, norm), -2.35f, 2.35f);
        g.setColour (juce::Colour (0xff111414));
        g.drawLine (c.x, c.y, c.x + std::sin (angle) * radius, c.y - std::cos (angle) * radius, 2.0f);
        g.setColour (juce::Colour (0xffe6e1cf));
        g.drawLine (c.x, c.y, c.x + std::sin (angle) * radius, c.y - std::cos (angle) * radius, 1.0f);
    };
    drawNeedle ({ 103.0f, 132.0f }, (float) input.getValue() / 36.0f + 0.667f, 13.0f);
    drawNeedle ({ 180.0f, 132.0f }, (float) output.getValue() / 36.0f + 0.667f, 13.0f);
    drawNeedle ({ 257.0f, 40.0f }, (float) ((pot.getValue() - pot.getMinimum()) / juce::jmax (0.001, pot.getMaximum() - pot.getMinimum())), 12.0f);

}

void RMX16AudioProcessorEditor::resized()
{
    const float sx = (float) getWidth() / 1057.0f;
    const float sy = (float) getHeight() / 198.0f;
    auto place = [this, sx, sy] (juce::Component& c, float x, float y, float w, float h)
    {
        c.setBounds (juce::roundToInt (x * sx), juce::roundToInt (y * sy),
                     juce::roundToInt (w * sx), juce::roundToInt (h * sy));
    };
    place (input, 80, 108, 46, 47);
    place (output, 157, 108, 46, 47);
    place (pot, 235, 18, 45, 45);
    place (programButton, 316, 107, 43, 44);
    place (predelayButton, 372, 107, 43, 44);
    place (decayButton, 428, 107, 43, 44);
    place (lowButton, 487, 107, 39, 44);
    place (highButton, 528, 107, 39, 44);
    place (storeButton, 579, 107, 40, 44);
    place (recallButton, 620, 107, 40, 44);
    place (nudgeDown, 676, 107, 39, 44);
    place (nudgeUp, 715, 107, 39, 44);
    place (display, 315, 25, 450, 46);

    const float x0 = 807.0f, y0 = 32.0f, dx = 28.3f, dy = 26.2f;
    for (int i = 0; i < 16; ++i)
    {
        const int col = i % 4, row = i / 4;
        place (*keypad[(size_t) i], x0 + col * dx, y0 + row * dy, 24.0f, 22.0f);
    }
}

void RMX16AudioProcessorEditor::selectPotTarget (const juce::String& id)
{
    potTarget = id;
    parameterEntryArmed = true;
    programEntryArmed = false;
    entryBuffer.clear();
    updatingPot = true;
    pot.setRange (selectedMin(), selectedMax(), selectedStep());
    pot.setValue (selectedValue(), juce::dontSendNotification);
    updatingPot = false;
    repaint();
}

void RMX16AudioProcessorEditor::handleKeypadDigit (const juce::String& key)
{
    if (programEntryArmed || parameterEntryArmed)
    {
        if (entryBuffer.length() < 8)
            entryBuffer += key;
        repaint();
        return;
    }

    const int programNumber = key.getIntValue();
    if (programNumber >= 1 && programNumber <= 9)
        processor.setCurrentProgram (programNumber - 1);
}

void RMX16AudioProcessorEditor::commitKeypadEntry()
{
    if (programEntryArmed)
    {
        const int programNumber = entryBuffer.getIntValue();
        if (programNumber >= 1 && programNumber <= 9)
            processor.setCurrentProgram (programNumber - 1);
    }
    else if (parameterEntryArmed && entryBuffer.isNotEmpty())
    {
        const float entered = entryBuffer.getFloatValue();
        setSelectedValue (entered);
        pot.setValue (selectedValue(), juce::dontSendNotification);
    }
    programEntryArmed = false;
    parameterEntryArmed = false;
    entryBuffer.clear();
    repaint();
}

float RMX16AudioProcessorEditor::selectedValue() const
{
    return processor.parameters.getRawParameterValue (potTarget)->load();
}
float RMX16AudioProcessorEditor::selectedMin() const
{
    if (potTarget == "predelay" || potTarget == "echoBPredelay") return 0.0f;
    if (potTarget == "decay" || potTarget == "echoBDecay") return 0.0f;
    return -9.0f;
}
float RMX16AudioProcessorEditor::selectedMax() const
{
    if (potTarget == "predelay") return maxPredelayForCurrentProgram();
    if (potTarget == "echoBPredelay") return 400.0f;
    if (potTarget == "decay" || potTarget == "echoBDecay") return 9.9f;
    if (potTarget == "high" || potTarget == "echoBHigh") return 0.0f;
    return 9.0f;
}
float RMX16AudioProcessorEditor::selectedStep() const
{
    if (potTarget == "predelay") return processor.getCurrentProgram() == 5 ? 1.0f : 10.0f;
    if (potTarget == "echoBPredelay") return 10.0f;
    if (potTarget == "decay" || potTarget == "echoBDecay") return 0.1f;
    return 1.0f;
}
float RMX16AudioProcessorEditor::maxPredelayForCurrentProgram() const
{
    const int p = (int) processor.parameters.getRawParameterValue ("program")->load();
    if (p == 5) return 50.0f;   // Chorus
    if (p == 6) return 1200.0f; // Echo A; Echo B is capped at 400 ms.
    if (p == 8) return 100.0f; // Reverse 1
    if (p == 0 || p == 2 || p == 4) return 200.0f; // Ambience, Hall C1 and Hall B3.
    return 300.0f; // Room A1, Plate A1 and conservative Nonlin 2 cap.
}
void RMX16AudioProcessorEditor::setSelectedValue (float value)
{
    auto* param = processor.parameters.getParameter (potTarget);
    const float bounded = juce::jlimit (selectedMin(), selectedMax(), value);
    param->setValueNotifyingHost (param->convertTo0to1 (bounded));
}
void RMX16AudioProcessorEditor::nudgeSelected (float direction)
{
    setSelectedValue (selectedValue() + direction * selectedStep());
    pot.setValue (selectedValue(), juce::dontSendNotification);
}

void RMX16AudioProcessorEditor::timerCallback()
{
    const int p = (int) processor.parameters.getRawParameterValue ("program")->load() + 1;
    const bool echoB = (p == 7 && echoBank == 1);

    // Reflect the documented control availability of each original program.
    recallButton.setEnabled (processor.hasStoredUserPreset());
    const int programIndex = p - 1;
    const bool highAvailable = (programIndex != 8); // Reverse 1 has no documented decay-filter control.
    const bool lowAvailable = (programIndex != 1 && programIndex != 5
                               && programIndex != 7 && programIndex != 8);
    highButton.setEnabled (highAvailable || programIndex == 6); // Echo exposes both A/B filter banks.
    lowButton.setEnabled (lowAvailable || programIndex == 6);
    if (! highAvailable && (potTarget == "high" || potTarget == "echoBHigh"))
        selectPotTarget ("decay");
    if (! lowAvailable && (potTarget == "low" || potTarget == "echoBLow"))
        selectPotTarget ("decay");
    const float d = processor.parameters.getRawParameterValue (echoB ? "echoBDecay" : "decay")->load();
    const float rawPredelay = processor.parameters.getRawParameterValue (echoB ? "echoBPredelay" : "predelay")->load();
    const float predelayMaximum = echoB ? 400.0f : maxPredelayForCurrentProgram();
    const int pd = juce::roundToInt (juce::jlimit (0.0f, predelayMaximum, rawPredelay));
    const int h = (int) processor.parameters.getRawParameterValue (echoB ? "echoBHigh" : "high")->load();
    const int l = (int) processor.parameters.getRawParameterValue (echoB ? "echoBLow" : "low")->load();
    const juce::String programLabel = (p == 7) ? (echoB ? "ECHO B" : "ECHO A")
        : RMX16AudioProcessor::programNames[p - 1].fromFirstOccurrenceOf ("  ", false, false);
    juce::String displayText = juce::String (p).paddedLeft ('0', 2) + "  " + juce::String (d, 1) + "  " +
                               juce::String (pd).paddedLeft ('0', 3) + "  " + juce::String (h) + "  " + juce::String (l) +
                               "   " + programLabel.toUpperCase();
    if (entryBuffer.isNotEmpty()) displayText = "ENTRY " + entryBuffer;
    display.setText (displayText, juce::dontSendNotification);
    updatingPot = true;
    // Keep the hardware-style nudge resolution after every timer refresh:
    // 10 ms for ordinary delay programs, 1 ms for Chorus.
    if (potTarget == "predelay") pot.setRange (0.0, maxPredelayForCurrentProgram(), selectedStep());
    if (potTarget == "echoBPredelay") pot.setRange (0.0, 400.0, selectedStep());
    if (potTarget == "high" || potTarget == "echoBHigh") pot.setRange (-9.0, 0.0, 1.0);
    if (! pot.isMouseOverOrDragging()) pot.setValue (selectedValue(), juce::dontSendNotification);
    updatingPot = false;
    repaint();
}
