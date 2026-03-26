#include "ProgressionChartComponent.h"
#include "ChordyTheme.h"
#include <cmath>

ProgressionChartComponent::ProgressionChartComponent()
{
    setInterceptsMouseClicks (true, false);
}

void ProgressionChartComponent::setProgression (Progression* prog)
{
    progression = prog;
    progressionReadOnly = nullptr;
    selectedChordIndex = -1;
    repaint();
}

void ProgressionChartComponent::setProgressionReadOnly (const Progression* prog)
{
    progressionReadOnly = prog;
    progression = nullptr;
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

double ProgressionChartComponent::xToBeat (float x, int row) const
{
    float bw = getBeatWidth();
    double rowStartBeat = row * beatsPerRow;
    return rowStartBeat + (x - leftPad) / bw;
}

double ProgressionChartComponent::snapBeat (double beat) const
{
    if (quantizeGrid <= 0.0)
        return beat;
    return std::round (beat / quantizeGrid) * quantizeGrid;
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
    auto* prog = getProgression();
    if (prog == nullptr)
        return -1;

    for (int i = 0; i < static_cast<int> (prog->chords.size()); ++i)
    {
        const auto& chord = prog->chords[static_cast<size_t> (i)];
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

ProgressionChartComponent::DragEdge ProgressionChartComponent::hitTestEdge (
    juce::Point<float> pos, int& outChordIndex) const
{
    auto* prog = getProgression();
    if (prog == nullptr || ! editMode)
        return DragEdge::None;

    for (int i = 0; i < static_cast<int> (prog->chords.size()); ++i)
    {
        const auto& chord = prog->chords[static_cast<size_t> (i)];
        int startRow = getRowForBeat (chord.startBeat);
        int endRow = getRowForBeat (chord.startBeat + chord.durationBeats - 0.01);

        for (int row = startRow; row <= endRow; ++row)
        {
            auto rect = getChordRect (chord, row);
            if (rect.isEmpty())
                continue;

            // Check right edge
            if (std::abs (pos.x - rect.getRight()) < edgeHitZone
                && pos.y >= rect.getY() && pos.y <= rect.getBottom())
            {
                outChordIndex = i;
                return DragEdge::Right;
            }

            // Check left edge
            if (std::abs (pos.x - rect.getX()) < edgeHitZone
                && pos.y >= rect.getY() && pos.y <= rect.getBottom())
            {
                outChordIndex = i;
                return DragEdge::Left;
            }
        }
    }

    // Check end marker
    if (prog != nullptr)
    {
        double totalBeats = prog->totalBeats;
        int endRow = getRowForBeat (totalBeats);
        double rowStartBeat = endRow * beatsPerRow;
        float endX = leftPad + static_cast<float> (totalBeats - rowStartBeat) * getBeatWidth();
        float endY = static_cast<float> (endRow * (rowHeight + rowGap));

        if (std::abs (pos.x - endX) < edgeHitZone + 2.0f
            && pos.y >= endY && pos.y <= endY + rowHeight)
        {
            outChordIndex = -1;
            return DragEdge::EndMarker;
        }
    }

    outChordIndex = -1;
    return DragEdge::None;
}

void ProgressionChartComponent::mouseMove (const juce::MouseEvent& e)
{
    if (! editMode || progression == nullptr)
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    int idx = -1;
    auto edge = hitTestEdge (e.position, idx);
    if (edge != DragEdge::None)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void ProgressionChartComponent::mouseDown (const juce::MouseEvent& e)
{
    auto* prog = getProgression();
    if (prog == nullptr)
        return;

    // Check for edge drag first (edit mode only)
    if (editMode && progression != nullptr)
    {
        int idx = -1;
        auto edge = hitTestEdge (e.position, idx);
        if (edge == DragEdge::EndMarker)
        {
            dragEdge = DragEdge::EndMarker;
            dragChordIndex = -1;
            return;
        }
        if (edge != DragEdge::None && idx >= 0)
        {
            dragEdge = edge;
            dragChordIndex = idx;
            dragOrigStartBeat = progression->chords[static_cast<size_t> (idx)].startBeat;
            dragOrigDuration = progression->chords[static_cast<size_t> (idx)].durationBeats;
            return;
        }
    }

    // Normal click — select chord
    int hit = hitTestChord (e.position);

    if (hit != selectedChordIndex)
    {
        selectedChordIndex = hit;
        repaint();

        if (onChordSelected)
            onChordSelected (selectedChordIndex);
    }
}

void ProgressionChartComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragEdge == DragEdge::None || progression == nullptr)
        return;

    // End marker drag — independent of chord durations
    if (dragEdge == DragEdge::EndMarker)
    {
        int row = getRowForBeat (progression->totalBeats);
        double newEnd = snapBeat (xToBeat (e.position.x, row));

        // Must be at least past the end of the last chord
        if (! progression->chords.empty())
        {
            auto& last = progression->chords.back();
            double minEnd = last.startBeat + last.durationBeats;
            if (newEnd < minEnd)
                newEnd = minEnd;
        }
        if (newEnd < quantizeGrid)
            newEnd = quantizeGrid;

        progression->totalBeats = newEnd;
        repaint();
        return;
    }

    if (dragChordIndex < 0)
        return;

    auto& chord = progression->chords[static_cast<size_t> (dragChordIndex)];
    int row = getRowForBeat (chord.startBeat);
    double newBeat = snapBeat (xToBeat (e.position.x, row));

    if (dragEdge == DragEdge::Right)
    {
        // Resize right edge: change duration
        double newDuration = newBeat - chord.startBeat;
        if (newDuration < quantizeGrid)
            newDuration = quantizeGrid;

        // Don't overlap next chord
        int nextIdx = dragChordIndex + 1;
        if (nextIdx < static_cast<int> (progression->chords.size()))
        {
            double maxEnd = progression->chords[static_cast<size_t> (nextIdx)].startBeat;
            if (chord.startBeat + newDuration > maxEnd)
                newDuration = maxEnd - chord.startBeat;
        }

        chord.durationBeats = newDuration;
    }
    else if (dragEdge == DragEdge::Left)
    {
        // Resize left edge: change start and duration together
        double newStart = newBeat;
        if (newStart < 0.0)
            newStart = 0.0;

        // Don't overlap previous chord
        int prevIdx = dragChordIndex - 1;
        if (prevIdx >= 0)
        {
            auto& prev = progression->chords[static_cast<size_t> (prevIdx)];
            double minStart = prev.startBeat + prev.durationBeats;
            if (newStart < minStart)
                newStart = minStart;
        }

        double endBeat = chord.startBeat + chord.durationBeats;
        double newDuration = endBeat - newStart;
        if (newDuration < quantizeGrid)
        {
            newStart = endBeat - quantizeGrid;
            newDuration = quantizeGrid;
        }

        chord.startBeat = newStart;
        chord.durationBeats = newDuration;
    }

    repaint();
}

void ProgressionChartComponent::mouseUp (const juce::MouseEvent&)
{
    if (dragEdge != DragEdge::None)
    {
        dragEdge = DragEdge::None;
        dragChordIndex = -1;

        // Update total beats
        if (progression != nullptr)
        {
            double maxEnd = 0.0;
            for (const auto& c : progression->chords)
                maxEnd = juce::jmax (maxEnd, c.startBeat + c.durationBeats);
            progression->totalBeats = maxEnd;
        }

        if (onChordResized)
            onChordResized();
    }
}

void ProgressionChartComponent::paint (juce::Graphics& g)
{
    auto* prog = getProgression();

    g.fillAll (juce::Colour (ChordyTheme::bgDeepest));

    if (prog == nullptr || prog->chords.empty())
    {
        g.setColour (juce::Colour (ChordyTheme::textTertiary));
        g.setFont (13.0f);
        g.drawText ("No progression", getLocalBounds(), juce::Justification::centred);
        return;
    }

    float bw = getBeatWidth();
    int beatsPerBar = prog->timeSignatureNum;
    if (beatsPerBar <= 0) beatsPerBar = 4;
    double totalBeats = prog->totalBeats;
    if (totalBeats <= 0.0)
    {
        for (const auto& c : prog->chords)
            totalBeats = juce::jmax (totalBeats, c.startBeat + c.durationBeats);
    }
    int numRows = juce::jmax (1, static_cast<int> (std::ceil (totalBeats / beatsPerRow)));

    // Draw rows
    for (int row = 0; row < numRows; ++row)
    {
        float y = static_cast<float> (row * (rowHeight + rowGap));

        g.setColour (juce::Colour (ChordyTheme::chartGrid));
        g.fillRoundedRectangle (static_cast<float> (leftPad), y,
                                static_cast<float> (getWidth() - leftPad - rightPad),
                                static_cast<float> (rowHeight), 4.0f);

        g.setColour (juce::Colour (ChordyTheme::chartBarLine));
        for (int beat = 0; beat <= beatsPerRow; beat += beatsPerBar)
        {
            float x = leftPad + beat * bw;
            g.drawVerticalLine (static_cast<int> (x), y, y + rowHeight);
        }
    }

    // Draw chords
    for (int i = 0; i < static_cast<int> (prog->chords.size()); ++i)
    {
        const auto& chord = prog->chords[static_cast<size_t> (i)];
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

            if (isSelected)
                g.setColour (juce::Colour (ChordyTheme::chartChordSelected));
            else if (isPassing)
                g.setColour (juce::Colour (ChordyTheme::chartPassingChord));
            else
                g.setColour (juce::Colour (ChordyTheme::chartChordBg));

            g.fillRoundedRectangle (inset, 4.0f);

            if (isSelected)
            {
                g.setColour (juce::Colour (ChordyTheme::accent));
                g.drawRoundedRectangle (inset, 4.0f, 1.5f);
            }

            g.setColour (juce::Colour (isSelected ? ChordyTheme::textPrimary : ChordyTheme::textSecondary));
            g.setFont (isPassing ? 10.0f : 12.0f);
            g.drawText (chord.getDisplayName(), inset.reduced (2.0f, 0),
                        juce::Justification::centred, true);
        }
    }

    // Draw end marker (edit mode only)
    if (editMode && totalBeats > 0.0)
    {
        int endRow = getRowForBeat (totalBeats);
        double endRowStart = endRow * beatsPerRow;
        float endX = leftPad + static_cast<float> (totalBeats - endRowStart) * bw;
        float endY = static_cast<float> (endRow * (rowHeight + rowGap));

        // Thick amber line with a small triangle handle
        g.setColour (juce::Colour (ChordyTheme::accent));
        g.fillRect (endX - 1.5f, endY, 3.0f, static_cast<float> (rowHeight));

        // Small triangle at bottom of the line
        juce::Path triangle;
        triangle.addTriangle (endX - 5.0f, endY + rowHeight,
                              endX + 5.0f, endY + rowHeight,
                              endX, endY + rowHeight - 6.0f);
        g.fillPath (triangle);
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
