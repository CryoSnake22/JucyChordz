#include "AccuracyTimeChart.h"
#include "ChordyTheme.h"

AccuracyTimeChart::AccuracyTimeChart()
{
    prevBpmButton.onClick = [this] { onPrevBpm(); };
    nextBpmButton.onClick = [this] { onNextBpm(); };
    addAndMakeVisible (prevBpmButton);
    addAndMakeVisible (nextBpmButton);

    bpmLabel.setFont (juce::FontOptions (ChordyTheme::fontBody));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    bpmLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (bpmLabel);

    updateBpmLabel();
}

void AccuracyTimeChart::setData (const std::vector<AttemptEntry>& attempts, int windowSize)
{
    points.clear();

    if (attempts.empty())
    {
        repaint();
        return;
    }

    // Compute rolling accuracy from attempts
    double runningSum = 0.0;
    int count = 0;
    for (size_t i = 0; i < attempts.size(); ++i)
    {
        runningSum += attempts[i].quality / 5.0;
        count++;

        // Remove oldest if beyond window
        if (count > windowSize)
        {
            runningSum -= attempts[i - static_cast<size_t> (windowSize)].quality / 5.0;
            count = windowSize;
        }

        DataPoint dp;
        dp.index = static_cast<int> (i);
        dp.accuracy = runningSum / count;
        points.push_back (dp);
    }

    repaint();
}

void AccuracyTimeChart::clearData()
{
    points.clear();
    availableBpms.clear();
    repaint();
}

void AccuracyTimeChart::setBpmOptions (const std::vector<float>& bpms)
{
    availableBpms = bpms;

    if (availableBpms.empty())
    {
        selectedBpmIndex = 0;
        updateBpmLabel();
        return;
    }

    // Find the closest match to current selection
    selectedBpmIndex = static_cast<int> (availableBpms.size()) - 1; // default to highest
    for (int i = 0; i < static_cast<int> (availableBpms.size()); ++i)
    {
        if (std::abs (availableBpms[static_cast<size_t> (i)] - selectedBpm) < 0.1f)
        {
            selectedBpmIndex = i;
            break;
        }
    }

    selectedBpm = availableBpms[static_cast<size_t> (selectedBpmIndex)];
    updateBpmLabel();
}

void AccuracyTimeChart::selectBpm (float bpm)
{
    selectedBpm = bpm;
    for (int i = 0; i < static_cast<int> (availableBpms.size()); ++i)
    {
        if (std::abs (availableBpms[static_cast<size_t> (i)] - bpm) < 0.1f)
        {
            selectedBpmIndex = i;
            break;
        }
    }
    updateBpmLabel();
}

void AccuracyTimeChart::resized()
{
    auto area = getLocalBounds();
    auto stepperRow = area.removeFromTop (stepperHeight);

    int buttonWidth = 30;
    prevBpmButton.setBounds (stepperRow.removeFromLeft (buttonWidth));
    nextBpmButton.setBounds (stepperRow.removeFromRight (buttonWidth));
    bpmLabel.setBounds (stepperRow);
}

void AccuracyTimeChart::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto chartArea = area.withTrimmedTop (static_cast<float> (stepperHeight) + 2.0f);

    // Background
    g.setColour (juce::Colour (ChordyTheme::bgSurface));
    g.fillRoundedRectangle (chartArea, 4.0f);

    if (points.empty())
    {
        g.setColour (juce::Colour (ChordyTheme::textTertiary));
        g.setFont (juce::FontOptions (ChordyTheme::fontSmall));
        g.drawText ("No data at this BPM", chartArea, juce::Justification::centred);
        return;
    }

    float chartLeft = chartArea.getX() + 30.0f;  // space for Y-axis labels
    float chartRight = chartArea.getRight() - 8.0f;
    float chartTop = chartArea.getY() + 8.0f;
    float chartBottom = chartArea.getBottom() - 16.0f;  // space for X-axis
    float chartWidth = chartRight - chartLeft;
    float chartHeight = chartBottom - chartTop;

    if (chartWidth < 20.0f || chartHeight < 20.0f)
        return;

    // Grid lines and Y-axis labels
    g.setFont (juce::FontOptions (ChordyTheme::fontMeta));
    for (int pct : { 0, 25, 50, 75, 100 })
    {
        float y = chartBottom - (static_cast<float> (pct) / 100.0f) * chartHeight;

        // Grid line
        g.setColour (juce::Colour (ChordyTheme::border).withAlpha (0.3f));
        g.drawHorizontalLine (static_cast<int> (y), chartLeft, chartRight);

        // Label
        g.setColour (juce::Colour (ChordyTheme::textTertiary));
        g.drawText (juce::String (pct) + "%",
                    static_cast<int> (chartArea.getX()), static_cast<int> (y - 7),
                    25, 14, juce::Justification::right);
    }

    // 90% threshold line (mastery)
    {
        float y90 = chartBottom - 0.9f * chartHeight;
        g.setColour (juce::Colour (ChordyTheme::success).withAlpha (0.3f));
        const float dashLengths[] = { 4.0f, 4.0f };
        g.drawDashedLine (juce::Line<float> (chartLeft, y90, chartRight, y90),
                          dashLengths, 2, 1.0f);
    }

    // Draw accuracy line
    int numPoints = static_cast<int> (points.size());
    float xStep = numPoints > 1 ? chartWidth / static_cast<float> (numPoints - 1) : 0.0f;

    juce::Path linePath;
    bool started = false;

    for (int i = 0; i < numPoints; ++i)
    {
        float x = chartLeft + static_cast<float> (i) * xStep;
        float y = chartBottom - static_cast<float> (points[static_cast<size_t> (i)].accuracy) * chartHeight;

        if (! started)
        {
            linePath.startNewSubPath (x, y);
            started = true;
        }
        else
        {
            linePath.lineTo (x, y);
        }
    }

    // Line stroke
    g.setColour (juce::Colour (ChordyTheme::accent));
    g.strokePath (linePath, juce::PathStrokeType (2.0f));

    // Dots at each data point (only if not too many)
    if (numPoints <= 50)
    {
        g.setColour (juce::Colour (ChordyTheme::accent));
        for (int i = 0; i < numPoints; ++i)
        {
            float x = chartLeft + static_cast<float> (i) * xStep;
            float y = chartBottom - static_cast<float> (points[static_cast<size_t> (i)].accuracy) * chartHeight;
            g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
        }
    }

    // Latest accuracy value label
    if (! points.empty())
    {
        double lastAcc = points.back().accuracy;
        float x = chartRight;
        float y = chartBottom - static_cast<float> (lastAcc) * chartHeight;

        juce::Colour valColour;
        if (lastAcc >= 0.9)
            valColour = juce::Colour (ChordyTheme::success);
        else if (lastAcc >= 0.5)
            valColour = juce::Colour (ChordyTheme::warning);
        else
            valColour = juce::Colour (ChordyTheme::danger);

        g.setColour (valColour);
        g.setFont (juce::FontOptions (ChordyTheme::fontSmall).withStyle ("Bold"));
        g.drawText (juce::String (static_cast<int> (lastAcc * 100)) + "%",
                    static_cast<int> (x - 30), static_cast<int> (y - 16), 30, 14,
                    juce::Justification::right);
    }
}

void AccuracyTimeChart::mouseDown (const juce::MouseEvent&)
{
    // Could add tooltip or detail on click later
}

void AccuracyTimeChart::updateBpmLabel()
{
    if (availableBpms.empty())
        bpmLabel.setText ("No data", juce::dontSendNotification);
    else
        bpmLabel.setText (juce::String (static_cast<int> (selectedBpm)) + " BPM",
                          juce::dontSendNotification);

    prevBpmButton.setEnabled (selectedBpmIndex > 0);
    nextBpmButton.setEnabled (selectedBpmIndex < static_cast<int> (availableBpms.size()) - 1);
}

void AccuracyTimeChart::onPrevBpm()
{
    if (selectedBpmIndex > 0)
    {
        selectedBpmIndex--;
        selectedBpm = availableBpms[static_cast<size_t> (selectedBpmIndex)];
        updateBpmLabel();
        if (onBpmChanged)
            onBpmChanged (selectedBpm);
    }
}

void AccuracyTimeChart::onNextBpm()
{
    if (selectedBpmIndex < static_cast<int> (availableBpms.size()) - 1)
    {
        selectedBpmIndex++;
        selectedBpm = availableBpms[static_cast<size_t> (selectedBpmIndex)];
        updateBpmLabel();
        if (onBpmChanged)
            onBpmChanged (selectedBpm);
    }
}
