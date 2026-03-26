#include "ChordyKeyboardComponent.h"

ChordyKeyboardComponent::ChordyKeyboardComponent (
    juce::MidiKeyboardState& state, Orientation orientation)
    : juce::MidiKeyboardComponent (state, orientation)
{
}

void ChordyKeyboardComponent::setKeyColour (int noteNumber, KeyColour colour)
{
    keyColours[noteNumber] = colour;
}

void ChordyKeyboardComponent::clearAllColours()
{
    keyColours.clear();
}

juce::Colour ChordyKeyboardComponent::getOverlayColour (KeyColour kc) const
{
    switch (kc)
    {
        case KeyColour::Correct:  return juce::Colour (0x8800CC44); // green, semi-transparent
        case KeyColour::Wrong:    return juce::Colour (0x88CC2200); // red, semi-transparent
        case KeyColour::Target:   return juce::Colour (0xAA4488FF); // bright blue, clearly visible
        default:                  return juce::Colour();
    }
}

void ChordyKeyboardComponent::drawWhiteNote (
    int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
    bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour)
{
    // Draw the default white note first
    juce::MidiKeyboardComponent::drawWhiteNote (
        midiNoteNumber, g, area, isDown, isOver, lineColour, textColour);

    // Overlay color if assigned
    auto it = keyColours.find (midiNoteNumber);
    if (it != keyColours.end() && it->second != KeyColour::Normal)
    {
        g.setColour (getOverlayColour (it->second));
        g.fillRect (area.reduced (1.0f));
    }
}

void ChordyKeyboardComponent::drawBlackNote (
    int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
    bool isDown, bool isOver, juce::Colour noteFillColour)
{
    // Draw the default black note first
    juce::MidiKeyboardComponent::drawBlackNote (
        midiNoteNumber, g, area, isDown, isOver, noteFillColour);

    // Overlay color if assigned
    auto it = keyColours.find (midiNoteNumber);
    if (it != keyColours.end() && it->second != KeyColour::Normal)
    {
        g.setColour (getOverlayColour (it->second));
        g.fillRect (area.reduced (1.0f));
    }
}
