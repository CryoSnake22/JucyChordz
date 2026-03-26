#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PlaceholderPanel : public juce::Component
{
public:
    PlaceholderPanel (const juce::String& text) : labelText (text) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF222244));
        g.setColour (juce::Colour (0xFF667788));
        g.setFont (juce::FontOptions (16.0f));
        g.drawText (labelText, getLocalBounds(), juce::Justification::centred);
    }

private:
    juce::String labelText;
};
