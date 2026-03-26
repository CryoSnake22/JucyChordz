#include "VoicingStatsChart.h"
#include "ChordDetector.h"

void VoicingStatsChart::setStats (
    const std::array<SpacedRepetitionEngine::KeyStats, 12>& stats)
{
    keyStats = stats;
    hasData = true;
    repaint();
}

void VoicingStatsChart::clearStats()
{
    keyStats = {};
    hasData = false;
    repaint();
}

void VoicingStatsChart::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Label area at the bottom
    float labelHeight = 14.0f;
    float chartTop = area.getY() + 2.0f;
    float chartBottom = area.getBottom() - labelHeight - 2.0f;
    float chartHeight = chartBottom - chartTop;

    if (chartHeight < 4.0f)
        return;

    float totalWidth = area.getWidth();
    float barSpacing = 3.0f;
    float barWidth = (totalWidth - barSpacing * 11.0f) / 12.0f;

    if (barWidth < 4.0f)
    {
        barSpacing = 1.0f;
        barWidth = (totalWidth - barSpacing * 11.0f) / 12.0f;
    }

    for (int i = 0; i < 12; ++i)
    {
        float x = area.getX() + static_cast<float> (i) * (barWidth + barSpacing);
        auto barArea = juce::Rectangle<float> (x, chartTop, barWidth, chartHeight);

        double acc = keyStats[static_cast<size_t> (i)].accuracy();

        if (acc < 0.0)
        {
            // No data — thin grey outline
            g.setColour (juce::Colour (0xFF445566));
            g.drawRect (barArea.reduced (0.5f), 1.0f);
        }
        else
        {
            // Background outline
            g.setColour (juce::Colour (0xFF333355));
            g.fillRect (barArea);

            // Filled bar proportional to accuracy
            float fillHeight = static_cast<float> (acc) * chartHeight;
            auto fillArea = juce::Rectangle<float> (
                x, chartBottom - fillHeight, barWidth, fillHeight);

            // Color: red → yellow → green based on accuracy
            juce::Colour barColour;
            if (acc >= 0.8)
                barColour = juce::Colour (0xFF00CC44); // green
            else if (acc >= 0.5)
                barColour = juce::Colour (0xFFCCCC00); // yellow
            else
                barColour = juce::Colour (0xFFCC4400); // red

            g.setColour (barColour);
            g.fillRect (fillArea);
        }

        // Root label
        g.setColour (juce::Colour (0xFF99AABB));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (ChordDetector::noteNameFromPitchClass (i),
                    static_cast<int> (x), static_cast<int> (chartBottom + 1),
                    static_cast<int> (barWidth), static_cast<int> (labelHeight),
                    juce::Justification::centred);
    }
}
