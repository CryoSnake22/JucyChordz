#include "VoicingStatsChart.h"
#include "ChordDetector.h"
#include "ChordyTheme.h"

VoicingStatsChart::VoicingStatsChart()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

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
    playingBar = -1;
    repaint();
}

void VoicingStatsChart::setPlayingKey (int keyIndex)
{
    playingBar = keyIndex;
    repaint();
}

int VoicingStatsChart::getBarIndexAt (float x) const
{
    auto area = getLocalBounds().toFloat();
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
        float barX = area.getX() + static_cast<float> (i) * (barWidth + barSpacing);
        if (x >= barX && x < barX + barWidth)
            return i;
    }

    return -1;
}

void VoicingStatsChart::mouseDown (const juce::MouseEvent& event)
{
    if (! hasData || ! onKeyClicked)
        return;

    int idx = getBarIndexAt (event.position.x);
    if (idx >= 0)
        onKeyClicked (idx);
}

void VoicingStatsChart::mouseMove (const juce::MouseEvent& event)
{
    int idx = hasData ? getBarIndexAt (event.position.x) : -1;
    if (idx != hoveredBar)
    {
        hoveredBar = idx;
        repaint();
    }
}

void VoicingStatsChart::mouseEnter (const juce::MouseEvent& event)
{
    mouseMove (event);
}

void VoicingStatsChart::mouseExit (const juce::MouseEvent&)
{
    if (hoveredBar != -1)
    {
        hoveredBar = -1;
        repaint();
    }
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
            g.setColour (juce::Colour (ChordyTheme::border));
            g.drawRect (barArea.reduced (0.5f), 1.0f);
        }
        else
        {
            // Background outline
            g.setColour (juce::Colour (ChordyTheme::bgElevated));
            g.fillRect (barArea);

            // Filled bar proportional to accuracy
            float fillHeight = static_cast<float> (acc) * chartHeight;
            auto fillArea = juce::Rectangle<float> (
                x, chartBottom - fillHeight, barWidth, fillHeight);

            // Color: red -> yellow -> green based on accuracy
            juce::Colour barColour;
            if (acc >= 0.8)
                barColour = juce::Colour (ChordyTheme::success);
            else if (acc >= 0.5)
                barColour = juce::Colour (ChordyTheme::warning);
            else
                barColour = juce::Colour (ChordyTheme::danger);

            g.setColour (barColour);
            g.fillRect (fillArea);
        }

        // Hover highlight
        if (i == hoveredBar && hasData)
        {
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.fillRect (barArea);
        }

        // Playing indicator — amber outline
        if (i == playingBar)
        {
            g.setColour (juce::Colour (ChordyTheme::accent));
            g.drawRect (barArea, 2.0f);
        }

        // Root label
        g.setColour (juce::Colour (ChordyTheme::textTertiary));
        g.setFont (juce::FontOptions (ChordyTheme::fontMeta));
        g.drawText (ChordDetector::noteNameFromPitchClass (i),
                    static_cast<int> (x), static_cast<int> (chartBottom + 1),
                    static_cast<int> (barWidth), static_cast<int> (labelHeight),
                    juce::Justification::centred);
    }
}
