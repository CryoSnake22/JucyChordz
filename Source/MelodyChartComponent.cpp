#include "MelodyChartComponent.h"
#include "ChordyTheme.h"
#include <cmath>

MelodyChartComponent::MelodyChartComponent()
{
    setInterceptsMouseClicks (true, false);
}

void MelodyChartComponent::setMelody (Melody* m)
{
    melody = m;
    melodyReadOnly = nullptr;
    highlightedNoteIndex = -1;
    selectedChordContextIndex = -1;
    noteStates.clear();
    repaint();
}

void MelodyChartComponent::setMelodyReadOnly (const Melody* m)
{
    melodyReadOnly = m;
    melody = nullptr;
    highlightedNoteIndex = -1;
    selectedChordContextIndex = -1;
    noteStates.clear();
    repaint();
}

void MelodyChartComponent::setCursorBeat (double beat)
{
    cursorBeat = beat;
    repaint();
}

void MelodyChartComponent::setHighlightedNoteIndex (int index)
{
    highlightedNoteIndex = index;
    repaint();
}

void MelodyChartComponent::setEditMode (bool enabled)
{
    editMode = enabled;
}

void MelodyChartComponent::setNoteState (int noteIndex, NoteState state)
{
    noteStates[noteIndex] = state;
    repaint();
}

void MelodyChartComponent::clearNoteStates()
{
    noteStates.clear();
    repaint();
}

void MelodyChartComponent::setSelectedChordContext (int index)
{
    selectedChordContextIndex = index;
    repaint();
}

//==============================================================================
// Layout helpers
//==============================================================================

float MelodyChartComponent::getBeatWidth() const
{
    return static_cast<float> (getWidth() - leftPad - rightPad) / beatsPerRow;
}

int MelodyChartComponent::getRowForBeat (double beat) const
{
    return static_cast<int> (beat) / beatsPerRow;
}

double MelodyChartComponent::xToBeat (float x, int row) const
{
    float bw = getBeatWidth();
    double rowStartBeat = row * beatsPerRow;
    return rowStartBeat + (x - leftPad) / bw;
}

double MelodyChartComponent::snapBeat (double beat) const
{
    if (quantizeGrid <= 0.0)
        return beat;
    return std::round (beat / quantizeGrid) * quantizeGrid;
}

void MelodyChartComponent::computePitchRange (int& minInterval, int& maxInterval) const
{
    auto* mel = getMelody();
    if (mel == nullptr || mel->notes.empty())
    {
        minInterval = -6;
        maxInterval = 18;
        return;
    }

    minInterval = mel->notes[0].intervalFromKeyRoot;
    maxInterval = mel->notes[0].intervalFromKeyRoot;
    for (const auto& note : mel->notes)
    {
        minInterval = std::min (minInterval, note.intervalFromKeyRoot);
        maxInterval = std::max (maxInterval, note.intervalFromKeyRoot);
    }

    // Add padding and ensure minimum range
    minInterval -= 2;
    maxInterval += 2;
    if (maxInterval - minInterval < 12)
    {
        int center = (minInterval + maxInterval) / 2;
        minInterval = center - 6;
        maxInterval = center + 6;
    }
}

float MelodyChartComponent::intervalToY (int interval, int minInterval, int maxInterval, int row) const
{
    float rowY = static_cast<float> (row * (rowHeight() + rowGap));
    float range = static_cast<float> (maxInterval - minInterval);
    if (range <= 0.0f) range = 12.0f;

    // Invert: higher pitch = lower Y (top of area)
    float normalised = static_cast<float> (interval - minInterval) / range;
    float topPad = 3.0f;
    float botPad = 3.0f;
    float availableH = static_cast<float> (noteAreaHeight) - topPad - botPad;
    return rowY + topPad + availableH * (1.0f - normalised);
}

juce::Rectangle<float> MelodyChartComponent::getNoteRect (
    const MelodyNote& note, int minInterval, int maxInterval, int row) const
{
    float bw = getBeatWidth();
    double rowStartBeat = row * beatsPerRow;
    double rowEndBeat = rowStartBeat + beatsPerRow;

    double clipStart = juce::jmax (note.startBeat, rowStartBeat);
    double clipEnd = juce::jmin (note.startBeat + note.durationBeats, rowEndBeat);
    if (clipEnd <= clipStart)
        return {};

    float x = leftPad + static_cast<float> (clipStart - rowStartBeat) * bw;
    float w = std::max (12.0f, static_cast<float> (clipEnd - clipStart) * bw);
    float y = intervalToY (note.intervalFromKeyRoot, minInterval, maxInterval, row);
    float h = 18.0f;

    return { x, y - h * 0.5f, w, h };
}

juce::Rectangle<float> MelodyChartComponent::getChordContextRect (
    const MelodyChordContext& cc, int row) const
{
    float bw = getBeatWidth();
    double rowStartBeat = row * beatsPerRow;
    double rowEndBeat = rowStartBeat + beatsPerRow;

    double clipStart = juce::jmax (cc.startBeat, rowStartBeat);
    double clipEnd = juce::jmin (cc.startBeat + cc.durationBeats, rowEndBeat);
    if (clipEnd <= clipStart)
        return {};

    float x = leftPad + static_cast<float> (clipStart - rowStartBeat) * bw;
    float w = static_cast<float> (clipEnd - clipStart) * bw;
    float y = static_cast<float> (row * (rowHeight() + rowGap)) + noteAreaHeight;

    return { x, y, w, static_cast<float> (chordBarHeight) };
}

//==============================================================================
// Hit testing
//==============================================================================

int MelodyChartComponent::hitTestNote (juce::Point<float> pos) const
{
    auto* mel = getMelody();
    if (mel == nullptr)
        return -1;

    int minI, maxI;
    computePitchRange (minI, maxI);

    for (int i = 0; i < static_cast<int> (mel->notes.size()); ++i)
    {
        const auto& note = mel->notes[static_cast<size_t> (i)];
        int startRow = getRowForBeat (note.startBeat);
        int endRow = getRowForBeat (note.startBeat + note.durationBeats - 0.01);

        for (int row = startRow; row <= endRow; ++row)
        {
            auto rect = getNoteRect (note, minI, maxI, row);
            if (rect.expanded (2.0f).contains (pos))
                return i;
        }
    }
    return -1;
}

int MelodyChartComponent::hitTestChordContext (juce::Point<float> pos) const
{
    auto* mel = getMelody();
    if (mel == nullptr)
        return -1;

    for (int i = 0; i < static_cast<int> (mel->chordContexts.size()); ++i)
    {
        const auto& cc = mel->chordContexts[static_cast<size_t> (i)];
        int startRow = getRowForBeat (cc.startBeat);
        int endRow = getRowForBeat (cc.startBeat + cc.durationBeats - 0.01);

        for (int row = startRow; row <= endRow; ++row)
        {
            auto rect = getChordContextRect (cc, row);
            if (rect.contains (pos))
                return i;
        }
    }
    return -1;
}

MelodyChartComponent::DragEdge MelodyChartComponent::hitTestChordEdge (
    juce::Point<float> pos, int& outIndex) const
{
    auto* mel = getMelody();
    if (mel == nullptr || ! editMode)
        return DragEdge::None;

    for (int i = 0; i < static_cast<int> (mel->chordContexts.size()); ++i)
    {
        const auto& cc = mel->chordContexts[static_cast<size_t> (i)];
        int startRow = getRowForBeat (cc.startBeat);
        int endRow = getRowForBeat (cc.startBeat + cc.durationBeats - 0.01);

        for (int row = startRow; row <= endRow; ++row)
        {
            auto rect = getChordContextRect (cc, row);
            if (rect.isEmpty())
                continue;

            if (std::abs (pos.x - rect.getRight()) < edgeHitZone
                && pos.y >= rect.getY() && pos.y <= rect.getBottom())
            {
                outIndex = i;
                return DragEdge::Right;
            }

            if (std::abs (pos.x - rect.getX()) < edgeHitZone
                && pos.y >= rect.getY() && pos.y <= rect.getBottom())
            {
                outIndex = i;
                return DragEdge::Left;
            }
        }
    }

    outIndex = -1;
    return DragEdge::None;
}

//==============================================================================
// Mouse handling
//==============================================================================

void MelodyChartComponent::mouseMove (const juce::MouseEvent& e)
{
    if (! editMode || melody == nullptr)
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    int idx = -1;
    auto edge = hitTestChordEdge (e.position, idx);
    setMouseCursor (edge != DragEdge::None
                    ? juce::MouseCursor::LeftRightResizeCursor
                    : juce::MouseCursor::NormalCursor);
}

void MelodyChartComponent::mouseDown (const juce::MouseEvent& e)
{
    auto* mel = getMelody();
    if (mel == nullptr)
        return;

    // Chord context edge drag (edit mode)
    if (editMode && melody != nullptr)
    {
        int idx = -1;
        auto edge = hitTestChordEdge (e.position, idx);
        if (edge != DragEdge::None && idx >= 0)
        {
            dragEdge = edge;
            dragChordIndex = idx;
            return;
        }
    }

    // Chord context click
    int ccHit = hitTestChordContext (e.position);
    if (ccHit >= 0)
    {
        selectedChordContextIndex = ccHit;
        repaint();
        if (onChordContextSelected)
            onChordContextSelected (ccHit);
        return;
    }

    // Note click
    int noteHit = hitTestNote (e.position);
    if (noteHit >= 0)
    {
        highlightedNoteIndex = noteHit;
        repaint();
        if (onNoteSelected)
            onNoteSelected (noteHit);
        return;
    }

    // Click on empty space — deselect
    selectedChordContextIndex = -1;
    highlightedNoteIndex = -1;
    repaint();
}

void MelodyChartComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragEdge == DragEdge::None || melody == nullptr || dragChordIndex < 0)
        return;

    auto& cc = melody->chordContexts[static_cast<size_t> (dragChordIndex)];
    int row = getRowForBeat (cc.startBeat);
    double newBeat = snapBeat (xToBeat (e.position.x, row));

    if (dragEdge == DragEdge::Right)
    {
        double newDuration = newBeat - cc.startBeat;
        if (newDuration < quantizeGrid) newDuration = quantizeGrid;

        int nextIdx = dragChordIndex + 1;
        if (nextIdx < static_cast<int> (melody->chordContexts.size()))
        {
            double maxEnd = melody->chordContexts[static_cast<size_t> (nextIdx)].startBeat;
            if (cc.startBeat + newDuration > maxEnd)
                newDuration = maxEnd - cc.startBeat;
        }
        cc.durationBeats = newDuration;
    }
    else if (dragEdge == DragEdge::Left)
    {
        double newStart = std::max (0.0, newBeat);
        int prevIdx = dragChordIndex - 1;
        if (prevIdx >= 0)
        {
            auto& prev = melody->chordContexts[static_cast<size_t> (prevIdx)];
            double minStart = prev.startBeat + prev.durationBeats;
            if (newStart < minStart) newStart = minStart;
        }

        double endBeat = cc.startBeat + cc.durationBeats;
        double newDuration = endBeat - newStart;
        if (newDuration < quantizeGrid)
        {
            newStart = endBeat - quantizeGrid;
            newDuration = quantizeGrid;
        }
        cc.startBeat = newStart;
        cc.durationBeats = newDuration;
    }

    repaint();
}

void MelodyChartComponent::mouseUp (const juce::MouseEvent&)
{
    if (dragEdge != DragEdge::None)
    {
        dragEdge = DragEdge::None;
        dragChordIndex = -1;
        if (onChordContextResized)
            onChordContextResized();
    }
}

//==============================================================================
// Paint
//==============================================================================

void MelodyChartComponent::paint (juce::Graphics& g)
{
    auto* mel = getMelody();

    g.fillAll (juce::Colour (ChordyTheme::bgDeepest));

    if (mel == nullptr || mel->notes.empty())
    {
        g.setColour (juce::Colour (ChordyTheme::textTertiary));
        g.setFont (13.0f);
        g.drawText ("No melody", getLocalBounds(), juce::Justification::centred);
        return;
    }

    float bw = getBeatWidth();
    int beatsPerBar = mel->timeSignatureNum;
    if (beatsPerBar <= 0) beatsPerBar = 4;

    double totalBeats = mel->totalBeats;
    if (totalBeats <= 0.0)
    {
        for (const auto& n : mel->notes)
            totalBeats = juce::jmax (totalBeats, n.startBeat + n.durationBeats);
    }
    int numRows = juce::jmax (1, static_cast<int> (std::ceil (totalBeats / beatsPerRow)));

    int minI, maxI;
    computePitchRange (minI, maxI);

    // Draw rows (note area + chord bar)
    for (int row = 0; row < numRows; ++row)
    {
        float y = static_cast<float> (row * (rowHeight() + rowGap));

        // Note area background
        g.setColour (juce::Colour (ChordyTheme::chartGrid));
        g.fillRoundedRectangle (static_cast<float> (leftPad), y,
                                static_cast<float> (getWidth() - leftPad - rightPad),
                                static_cast<float> (noteAreaHeight), 4.0f);

        // Chord bar background
        float chordY = y + noteAreaHeight;
        g.setColour (juce::Colour (ChordyTheme::melodyChordBg));
        g.fillRoundedRectangle (static_cast<float> (leftPad), chordY,
                                static_cast<float> (getWidth() - leftPad - rightPad),
                                static_cast<float> (chordBarHeight), 3.0f);

        // Bar lines
        g.setColour (juce::Colour (ChordyTheme::chartBarLine));
        for (int beat = 0; beat <= beatsPerRow; beat += beatsPerBar)
        {
            float x = leftPad + beat * bw;
            g.drawVerticalLine (static_cast<int> (x), y, y + noteAreaHeight);
        }

        // Beat grid lines (lighter, within note area)
        g.setColour (juce::Colour (ChordyTheme::chartBarLine).withAlpha (0.3f));
        for (int beat = 1; beat < beatsPerRow; ++beat)
        {
            if (beat % beatsPerBar == 0) continue; // already drawn
            float x = leftPad + beat * bw;
            g.drawVerticalLine (static_cast<int> (x), y, y + noteAreaHeight);
        }
    }

    // Draw chord contexts
    for (int i = 0; i < static_cast<int> (mel->chordContexts.size()); ++i)
    {
        const auto& cc = mel->chordContexts[static_cast<size_t> (i)];
        int startRow = getRowForBeat (cc.startBeat);
        int endRow = getRowForBeat (cc.startBeat + cc.durationBeats - 0.01);

        for (int row = startRow; row <= endRow; ++row)
        {
            auto rect = getChordContextRect (cc, row);
            if (rect.isEmpty()) continue;

            auto inset = rect.reduced (1.0f, 1.0f);
            bool isSelected = (i == selectedChordContextIndex);

            g.setColour (juce::Colour (isSelected ? ChordyTheme::melodyChordActive : ChordyTheme::melodyChordBg));
            g.fillRoundedRectangle (inset, 3.0f);

            if (isSelected)
            {
                g.setColour (juce::Colour (ChordyTheme::accent));
                g.drawRoundedRectangle (inset, 3.0f, 1.0f);
            }

            // Chord name
            juce::String label = cc.name.isNotEmpty()
                ? cc.name
                : const_cast<MelodyChordContext&> (cc).getDisplayName (mel->keyPitchClass);
            g.setColour (juce::Colour (isSelected ? ChordyTheme::textPrimary : ChordyTheme::textSecondary));
            g.setFont (10.0f);
            g.drawText (label, inset.reduced (2.0f, 0), juce::Justification::centred, true);
        }
    }

    // Draw notes
    for (int i = 0; i < static_cast<int> (mel->notes.size()); ++i)
    {
        const auto& note = mel->notes[static_cast<size_t> (i)];
        int startRow = getRowForBeat (note.startBeat);
        int endRow = getRowForBeat (note.startBeat + note.durationBeats - 0.01);

        // Determine note colour from state
        auto stateIt = noteStates.find (i);
        NoteState state = (stateIt != noteStates.end()) ? stateIt->second : NoteState::Default;

        juce::uint32 noteColour;
        bool isTarget = (i == highlightedNoteIndex && (state == NoteState::Default || state == NoteState::Target));
        switch (state)
        {
            case NoteState::Correct: noteColour = ChordyTheme::melodyNoteCorrect; break;
            case NoteState::Missed:  noteColour = ChordyTheme::melodyNoteMissed; break;
            default:                 noteColour = ChordyTheme::melodyNoteBg; break;
        }

        for (int row = startRow; row <= endRow; ++row)
        {
            auto rect = getNoteRect (note, minI, maxI, row);
            if (rect.isEmpty()) continue;

            g.setColour (juce::Colour (noteColour));
            g.fillRoundedRectangle (rect, 3.0f);

            // Draw amber outline for the current target note
            if (isTarget)
            {
                g.setColour (juce::Colour (ChordyTheme::accent));
                g.drawRoundedRectangle (rect, 3.0f, 2.0f);
            }

            // Note name inside rect
            int pitchClass = ((mel->keyPitchClass + note.intervalFromKeyRoot) % 12 + 12) % 12;
            juce::String noteName = ChordDetector::noteNameFromPitchClass (pitchClass);

            g.setColour (state == NoteState::Correct || state == NoteState::Missed
                         ? juce::Colour (ChordyTheme::bgDeepest)
                         : juce::Colour (ChordyTheme::textPrimary));
            g.setFont (11.0f);
            g.drawText (noteName, rect, juce::Justification::centred, false);
        }
    }

    // Draw cursor
    if (cursorBeat >= 0.0 && cursorBeat < totalBeats)
    {
        int row = getRowForBeat (cursorBeat);
        double rowStartBeat = row * beatsPerRow;
        float x = leftPad + static_cast<float> (cursorBeat - rowStartBeat) * bw;
        float y = static_cast<float> (row * (rowHeight() + rowGap));

        g.setColour (juce::Colour (ChordyTheme::chartCursor));
        g.fillRect (x - 1.0f, y, 2.0f, static_cast<float> (rowHeight()));
    }
}
