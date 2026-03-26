#include "ProgressionChartComponent.h"
#include "ChordyTheme.h"
#include <cmath>

ProgressionChartComponent::ProgressionChartComponent()
{
    setInterceptsMouseClicks (true, false);
}

void ProgressionChartComponent::setProgression (const Progression* prog)
{
    progression = prog;
    selectedChordIndex = -1;
    repaint();
}

void ProgressionChartComponent::setCursorBeat (double beat)
{
    cursorBeat = beat;
    repaint();
}

void ProgressionChartComponent::setSelectedChord (int index)
{
    selectedChordIndex = index;
    repaint();
}

void ProgressionChartComponent::setEditMode (bool enabled)
{
    editMode = enabled;
}

float ProgressionChartComponent::getBeatWidth() const
{
    return static_cast<float> (getWidth() - leftPad - rightPad) / beatsPerRow;
}

int ProgressionChartComponent::getRowForBeat (double beat) const
{
    return static_cast<int> (beat) / beatsPerRow;
}

juce::Rectangle<float> ProgressionChartComponent::getChordRect (
    const ProgressionChord& chord, int row) const
{
    float bw = getBeatWidth();
    double rowStartBeat = row * beatsPerRow;
    double rowEndBeat = rowStartBeat + beatsPerRow;

    double clipStart = juce::jmax (chord.startBeat, rowStartBeat);
    double clipEnd = juce::jmin (chord.startBeat + chord.durationBeats, rowEndBeat);

    if (clipEnd <= clipStart)
        return {};

    float x = leftPad + static_cast<float> (clipStart - rowStartBeat) * bw;
    float w = static_cast<float> (clipEnd - clipStart) * bw;
    float y = static_cast<float> (row * (rowHeight + rowGap));

    return { x, y, w, static_cast<float> (rowHeight) };
}

int ProgressionChartComponent::hitTestChord (juce::Point<float> pos) const
{
    if (progression == nullptr)
        return -1;

    for (int i = 0; i < static_cast<int> (progression->chords.size()); ++i)
    {
        const auto& chord = progression->chords[static_cast<size_t> (i)];
        int startRow = getRowForBeat (chord.startBeat);
        int endRow = getRowForBeat (chord.startBeat + chord.durationBeats - 0.01);

        for (int row = startRow; row <= endRow; ++row)
        {
            auto rect = getChordRect (chord, row);
            if (rect.contains (pos))
                return i;
        }
    }

    return -1;
}

void ProgressionChartComponent::mouseDown (const juce::MouseEvent& e)
{
    if (progression == nullptr)
        return;

    int hit = hitTestChord (e.position);

    if (hit != selectedChordIndex)
    {
        selectedChordIndex = hit;
        repaint();

        if (onChordSelected)
            onChordSelected (selectedChordIndex);
    }
}

void ProgressionChartComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (ChordyTheme::bgDeepest));

    if (progression == nullptr || progression->chords.empty())
    {
        g.setColour (juce::Colour (ChordyTheme::textTertiary));
        g.setFont (13.0f);
        g.drawText ("No progression", getLocalBounds(), juce::Justification::centred);
        return;
    }

    float bw = getBeatWidth();
    int beatsPerBar = progression->timeSignatureNum;
    double totalBeats = progression->totalBeats;
    if (totalBeats <= 0.0)
    {
        // Compute from chords
        for (const auto& c : progression->chords)
            totalBeats = juce::jmax (totalBeats, c.startBeat + c.durationBeats);
    }
    int numRows = juce::jmax (1, static_cast<int> (std::ceil (totalBeats / beatsPerRow)));

    // Draw rows
    for (int row = 0; row < numRows; ++row)
    {
        float y = static_cast<float> (row * (rowHeight + rowGap));

        // Row background
        g.setColour (juce::Colour (ChordyTheme::chartGrid));
        g.fillRoundedRectangle (static_cast<float> (leftPad), y,
                                static_cast<float> (getWidth() - leftPad - rightPad),
                                static_cast<float> (rowHeight), 4.0f);

        // Bar lines
        g.setColour (juce::Colour (ChordyTheme::chartBarLine));
        for (int beat = 0; beat <= beatsPerRow; beat += beatsPerBar)
        {
            float x = leftPad + beat * bw;
            g.drawVerticalLine (static_cast<int> (x), y, y + rowHeight);
        }
    }

    // Draw chords
    for (int i = 0; i < static_cast<int> (progression->chords.size()); ++i)
    {
        const auto& chord = progression->chords[static_cast<size_t> (i)];
        int startRow = getRowForBeat (chord.startBeat);
        int endRow = getRowForBeat (chord.startBeat + chord.durationBeats - 0.01);

        for (int row = startRow; row <= endRow; ++row)
        {
            auto rect = getChordRect (chord, row);
            if (rect.isEmpty())
                continue;

            auto inset = rect.reduced (1.0f);
            bool isPassing = chord.durationBeats < 1.0;
            bool isSelected = (i == selectedChordIndex);

            // Chord background
            if (isSelected)
                g.setColour (juce::Colour (ChordyTheme::chartChordSelected));
            else if (isPassing)
                g.setColour (juce::Colour (ChordyTheme::chartPassingChord));
            else
                g.setColour (juce::Colour (ChordyTheme::chartChordBg));

            g.fillRoundedRectangle (inset, 4.0f);

            // Selection border
            if (isSelected)
            {
                g.setColour (juce::Colour (ChordyTheme::accent));
                g.drawRoundedRectangle (inset, 4.0f, 1.5f);
            }

            // Chord name
            g.setColour (juce::Colour (isSelected ? ChordyTheme::textPrimary : ChordyTheme::textSecondary));
            g.setFont (isPassing ? 10.0f : 12.0f);
            g.drawText (chord.getDisplayName(), inset.reduced (2.0f, 0),
                        juce::Justification::centred, true);
        }
    }

    // Draw cursor
    if (cursorBeat >= 0.0 && cursorBeat < totalBeats)
    {
        int row = getRowForBeat (cursorBeat);
        double rowStartBeat = row * beatsPerRow;
        float x = leftPad + static_cast<float> (cursorBeat - rowStartBeat) * bw;
        float y = static_cast<float> (row * (rowHeight + rowGap));

        g.setColour (juce::Colour (ChordyTheme::chartCursor));
        g.fillRect (x - 1.0f, y, 2.0f, static_cast<float> (rowHeight));
    }
}
