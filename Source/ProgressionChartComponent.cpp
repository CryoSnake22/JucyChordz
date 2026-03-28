#include "ProgressionChartComponent.h"
#include "ChordDetector.h"
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
    noteStates.clear();
    repaint();
}

void ProgressionChartComponent::setProgressionReadOnly (const Progression* prog)
{
    progressionReadOnly = prog;
    progression = nullptr;
    selectedChordIndex = -1;
    noteStates.clear();
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

void ProgressionChartComponent::setDetailedView (bool enabled)
{
    if (detailedView != enabled)
    {
        detailedView = enabled;
        repaint();
    }
}

void ProgressionChartComponent::setNoteState (int chordIndex, int noteIndex, NoteState state)
{
    noteStates[{chordIndex, noteIndex}] = state;
    repaint();
}

ProgressionChartComponent::NoteState ProgressionChartComponent::getNoteState (int chordIndex, int noteIndex) const
{
    auto it = noteStates.find ({chordIndex, noteIndex});
    return (it != noteStates.end()) ? it->second : NoteState::Default;
}

void ProgressionChartComponent::setAllChordNoteStates (int chordIndex, NoteState state)
{
    auto* prog = getProgression();
    if (prog == nullptr || chordIndex < 0 || chordIndex >= static_cast<int> (prog->chords.size()))
        return;
    int numNotes = static_cast<int> (prog->chords[static_cast<size_t> (chordIndex)].midiNotes.size());
    for (int i = 0; i < numNotes; ++i)
        noteStates[{chordIndex, i}] = state;
    repaint();
}

void ProgressionChartComponent::clearNoteStates()
{
    noteStates.clear();
    repaint();
}

//==============================================================================

int ProgressionChartComponent::getIdealHeight() const
{
    auto* prog = getProgression();
    if (prog == nullptr || prog->chords.empty())
        return getEffectiveRowHeight();

    double totalBeats = prog->totalBeats;
    if (totalBeats <= 0.0)
    {
        for (const auto& c : prog->chords)
            totalBeats = juce::jmax (totalBeats, c.startBeat + c.durationBeats);
    }
    int numRows = juce::jmax (1, static_cast<int> (std::ceil (totalBeats / beatsPerRow)));
    return numRows * (getEffectiveRowHeight() + rowGap);
}

void ProgressionChartComponent::setViewportHeight (int height)
{
    viewportHeight = juce::jmax (60, height);
}

int ProgressionChartComponent::getDetailedNoteAreaHeight() const
{
    // Each row fills the viewport: row = padding + noteArea + gap + chordLabel + padding
    return viewportHeight - detailedChordLabelHeight - detailedPad * 2 - 2;
}

int ProgressionChartComponent::getEffectiveRowHeight() const
{
    if (detailedView && ! editMode)
        return viewportHeight;  // each row fills one viewport height
    return simpleRowHeight;
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
    int rh = getEffectiveRowHeight();
    double rowStartBeat = row * beatsPerRow;
    double rowEndBeat = rowStartBeat + beatsPerRow;

    double clipStart = juce::jmax (chord.startBeat, rowStartBeat);
    double clipEnd = juce::jmin (chord.startBeat + chord.durationBeats, rowEndBeat);

    if (clipEnd <= clipStart)
        return {};

    float x = leftPad + static_cast<float> (clipStart - rowStartBeat) * bw;
    float w = static_cast<float> (clipEnd - clipStart) * bw;
    float y = static_cast<float> (row * (rh + rowGap));

    return { x, y, w, static_cast<float> (rh) };
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

    int rh = getEffectiveRowHeight();

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

            if (std::abs (pos.x - rect.getRight()) < edgeHitZone
                && pos.y >= rect.getY() && pos.y <= rect.getBottom())
            {
                outChordIndex = i;
                return DragEdge::Right;
            }

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
        float endY = static_cast<float> (endRow * (rh + rowGap));

        if (std::abs (pos.x - endX) < edgeHitZone + 2.0f
            && pos.y >= endY && pos.y <= endY + rh)
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

    int hit = hitTestChord (e.position);
    selectedChordIndex = hit;

    // Record the beat position of the click for note filtering
    int row = static_cast<int> (e.position.y) / (getEffectiveRowHeight() + rowGap);
    lastClickedBeat = xToBeat (e.position.x, row);

    repaint();

    if (onChordSelected)
        onChordSelected (selectedChordIndex);
}

void ProgressionChartComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragEdge == DragEdge::None || progression == nullptr)
        return;

    if (dragEdge == DragEdge::EndMarker)
    {
        int row = getRowForBeat (progression->totalBeats);
        // End marker always snaps to whole beats regardless of quantize setting
        double newEnd = std::round (xToBeat (e.position.x, row));

        if (newEnd < 1.0)
            newEnd = 1.0;

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
        double newDuration = newBeat - chord.startBeat;
        if (newDuration < quantizeGrid)
            newDuration = quantizeGrid;

        int nextIdx = dragChordIndex + 1;
        if (nextIdx < static_cast<int> (progression->chords.size()))
        {
            double maxEnd = progression->chords[static_cast<size_t> (nextIdx)].startBeat;
            if (chord.startBeat + newDuration > maxEnd)
                newDuration = maxEnd - chord.startBeat;
        }

        // Scale per-note durations proportionally
        double oldDuration = chord.durationBeats;
        if (oldDuration > 0.0 && ! chord.noteDurations.empty())
        {
            double scale = newDuration / oldDuration;
            for (auto& d : chord.noteDurations)
                d = std::max (0.05, d * scale);
        }

        chord.durationBeats = newDuration;
    }
    else if (dragEdge == DragEdge::Left)
    {
        double newStart = newBeat;
        if (newStart < 0.0)
            newStart = 0.0;

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

        // Shift per-note start beats by the same delta
        double delta = newStart - chord.startBeat;
        for (auto& s : chord.noteStartBeats)
            s += delta;

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

//==============================================================================
// Detailed view helpers
//==============================================================================

void ProgressionChartComponent::computePitchRange (int& minMidi, int& maxMidi) const
{
    auto* prog = getProgression();
    if (prog == nullptr || prog->chords.empty())
    {
        minMidi = 48;
        maxMidi = 72;
        return;
    }

    minMidi = 127;
    maxMidi = 0;
    for (const auto& chord : prog->chords)
    {
        for (int note : chord.midiNotes)
        {
            minMidi = juce::jmin (minMidi, note);
            maxMidi = juce::jmax (maxMidi, note);
        }
    }

    minMidi -= 2;
    maxMidi += 2;

    if (maxMidi - minMidi < 12)
    {
        int center = (minMidi + maxMidi) / 2;
        minMidi = center - 6;
        maxMidi = center + 6;
    }
}

float ProgressionChartComponent::midiToY (int midiNote, int minMidi, int maxMidi, int row) const
{
    int rh = getEffectiveRowHeight();
    float rowY = static_cast<float> (row * (rh + rowGap));
    float range = static_cast<float> (maxMidi - minMidi);
    if (range <= 0.0f) range = 12.0f;
    float normalised = static_cast<float> (midiNote - minMidi) / range;
    float topOff = static_cast<float> (detailedPad);
    float availableH = static_cast<float> (getDetailedNoteAreaHeight()) - topOff * 2.0f;
    return rowY + topOff + availableH * (1.0f - normalised);
}

juce::Rectangle<float> ProgressionChartComponent::getNoteDetailRect (
    int midiNote, double startBeat, double durationBeats,
    int minMidi, int maxMidi, int row) const
{
    float bw = getBeatWidth();
    double rowStartBeat = row * beatsPerRow;
    double rowEndBeat = rowStartBeat + beatsPerRow;
    double clipStart = juce::jmax (startBeat, rowStartBeat);
    double clipEnd = juce::jmin (startBeat + durationBeats, rowEndBeat);
    if (clipEnd <= clipStart)
        return {};

    float x = leftPad + static_cast<float> (clipStart - rowStartBeat) * bw;
    float w = juce::jmax (6.0f, static_cast<float> (clipEnd - clipStart) * bw);
    float y = midiToY (midiNote, minMidi, maxMidi, row);
    // Height = one semitone's worth of vertical space, capped at 16px
    int noteAreaH = getDetailedNoteAreaHeight();
    float range = static_cast<float> (maxMidi - minMidi);
    if (range <= 0.0f) range = 12.0f;
    float innerPad = static_cast<float> (detailedPad);
    float availH = static_cast<float> (noteAreaH) - innerPad * 2.0f;
    float h = juce::jmin (16.0f, availH / range);
    return { x, y - h * 0.5f, w, h };
}

//==============================================================================
// Paint
//==============================================================================

void ProgressionChartComponent::paint (juce::Graphics& g)
{
    auto* prog = getProgression();

    g.fillAll (juce::Colour (ChordyTheme::bgSurface));

    if (prog == nullptr || prog->chords.empty())
    {
        g.setColour (juce::Colour (ChordyTheme::textTertiary));
        g.setFont (13.0f);
        g.drawText ("No progression", getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (detailedView && ! editMode)
        paintDetailed (g, prog);
    else
        paintSimple (g, prog);
}

void ProgressionChartComponent::paintSimple (juce::Graphics& g, const Progression* prog)
{
    float bw = getBeatWidth();
    int rh = simpleRowHeight;
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
        float y = static_cast<float> (row * (rh + rowGap));

        g.setColour (juce::Colour (ChordyTheme::chartGrid));
        g.fillRoundedRectangle (static_cast<float> (leftPad), y,
                                static_cast<float> (getWidth() - leftPad - rightPad),
                                static_cast<float> (rh), 4.0f);

        g.setColour (juce::Colour (ChordyTheme::chartBarLine));
        for (int beat = 0; beat <= beatsPerRow; beat += beatsPerBar)
        {
            float x = leftPad + beat * bw;
            g.drawVerticalLine (static_cast<int> (x), y, y + rh);
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
        float endY = static_cast<float> (endRow * (rh + rowGap));

        g.setColour (juce::Colour (ChordyTheme::accent));
        g.fillRect (endX - 1.5f, endY, 3.0f, static_cast<float> (rh));

        juce::Path triangle;
        triangle.addTriangle (endX - 5.0f, endY + rh,
                              endX + 5.0f, endY + rh,
                              endX, endY + rh - 6.0f);
        g.fillPath (triangle);
    }

    // Draw cursor
    if (cursorBeat >= 0.0 && cursorBeat < totalBeats)
    {
        int row = getRowForBeat (cursorBeat);
        double rowStartBeat = row * beatsPerRow;
        float x = leftPad + static_cast<float> (cursorBeat - rowStartBeat) * bw;
        float y = static_cast<float> (row * (rh + rowGap));

        g.setColour (juce::Colour (ChordyTheme::chartCursor));
        g.fillRect (x - 1.0f, y, 2.0f, static_cast<float> (rh));
    }
}

void ProgressionChartComponent::paintDetailed (juce::Graphics& g, const Progression* prog)
{
    float bw = getBeatWidth();
    int rh = getEffectiveRowHeight();
    int beatsPerBar = prog->timeSignatureNum;
    if (beatsPerBar <= 0) beatsPerBar = 4;
    double totalBeats = prog->totalBeats;
    if (totalBeats <= 0.0)
    {
        for (const auto& c : prog->chords)
            totalBeats = juce::jmax (totalBeats, c.startBeat + c.durationBeats);
    }
    int numRows = juce::jmax (1, static_cast<int> (std::ceil (totalBeats / beatsPerRow)));

    int minMidi, maxMidi;
    computePitchRange (minMidi, maxMidi);

    float dPad = static_cast<float> (detailedPad);
    int noteAreaH = getDetailedNoteAreaHeight();

    // Draw rows
    for (int row = 0; row < numRows; ++row)
    {
        float y = static_cast<float> (row * (rh + rowGap));
        float noteAreaW = static_cast<float> (getWidth() - leftPad - rightPad);
        float noteY = y + dPad;  // top padding before note area

        // Chord label bar background (below note area, no rounding)
        float labelY = noteY + static_cast<float> (noteAreaH) + 2.0f;
        g.setColour (juce::Colour (ChordyTheme::melodyChordBg));
        g.fillRect (static_cast<float> (leftPad), labelY,
                    noteAreaW, static_cast<float> (detailedChordLabelHeight));

        // Bar lines in note area
        g.setColour (juce::Colour (ChordyTheme::chartBarLine).withAlpha (0.4f));
        for (int beat = 0; beat <= beatsPerRow; beat += beatsPerBar)
        {
            float x = leftPad + beat * bw;
            g.drawVerticalLine (static_cast<int> (x), noteY, noteY + static_cast<float> (noteAreaH));
        }
    }

    // Draw selected chord highlight band
    if (selectedChordIndex >= 0 && selectedChordIndex < static_cast<int> (prog->chords.size()))
    {
        const auto& selChord = prog->chords[static_cast<size_t> (selectedChordIndex)];
        int startRow = getRowForBeat (selChord.startBeat);
        int endRow = getRowForBeat (selChord.startBeat + selChord.durationBeats - 0.01);

        for (int row = startRow; row <= endRow; ++row)
        {
            double rowStartBeat = row * beatsPerRow;
            double rowEndBeat = rowStartBeat + beatsPerRow;
            double clipStart = juce::jmax (selChord.startBeat, rowStartBeat);
            double clipEnd = juce::jmin (selChord.startBeat + selChord.durationBeats, rowEndBeat);
            if (clipEnd <= clipStart) continue;

            float x = leftPad + static_cast<float> (clipStart - rowStartBeat) * bw;
            float w = static_cast<float> (clipEnd - clipStart) * bw;
            float y = static_cast<float> (row * (rh + rowGap)) + dPad;

            g.setColour (juce::Colour (ChordyTheme::accent).withAlpha (0.12f));
            g.fillRect (x, y, w, static_cast<float> (noteAreaH));
        }
    }

    // Draw notes for each chord
    for (int ci = 0; ci < static_cast<int> (prog->chords.size()); ++ci)
    {
        const auto& chord = prog->chords[static_cast<size_t> (ci)];
        int startRow = getRowForBeat (chord.startBeat);
        int endRow = getRowForBeat (chord.startBeat + chord.durationBeats - 0.01);
        bool isSelected = (ci == selectedChordIndex);

        for (int ni = 0; ni < static_cast<int> (chord.midiNotes.size()); ++ni)
        {
            int midiNote = chord.midiNotes[static_cast<size_t> (ni)];

            // Look up note state
            auto stateIt = noteStates.find ({ci, ni});
            NoteState state = (stateIt != noteStates.end()) ? stateIt->second : NoteState::Default;

            // Determine color
            juce::uint32 noteColour;
            switch (state)
            {
                case NoteState::Correct: noteColour = ChordyTheme::melodyNoteCorrect; break;
                case NoteState::Missed:  noteColour = ChordyTheme::melodyNoteMissed; break;
                default:                 noteColour = ChordyTheme::melodyNoteBg; break;
            }

            // Use per-note timing if available, otherwise fall back to chord timing
            double noteStart = (static_cast<size_t> (ni) < chord.noteStartBeats.size())
                ? chord.noteStartBeats[static_cast<size_t> (ni)] : chord.startBeat;
            double noteDur = (static_cast<size_t> (ni) < chord.noteDurations.size())
                ? chord.noteDurations[static_cast<size_t> (ni)] : chord.durationBeats;

            for (int row = startRow; row <= endRow; ++row)
            {
                auto rect = getNoteDetailRect (midiNote, noteStart, noteDur,
                                               minMidi, maxMidi, row);
                if (rect.isEmpty()) continue;

                g.setColour (juce::Colour (noteColour));
                g.fillRoundedRectangle (rect, 2.0f);

                // Target outline (amber) — only for notes active at clicked beat when browsing
                bool noteActiveAtClick = isSelected && (editMode
                    || lastClickedBeat < 0.0
                    || (lastClickedBeat >= noteStart && lastClickedBeat < noteStart + noteDur));
                if (state == NoteState::Target || (state == NoteState::Default && noteActiveAtClick))
                {
                    g.setColour (juce::Colour (ChordyTheme::accent));
                    g.drawRoundedRectangle (rect, 2.0f, 1.5f);
                }

                // Note name — always show
                if (rect.getHeight() > 7.0f)
                {
                    bool darkText = (state == NoteState::Correct || state == NoteState::Missed);
                    g.setColour (juce::Colour (darkText ? 0xFF1A1A1A : ChordyTheme::textSecondary));
                    g.setFont (juce::FontOptions (8.0f));
                    g.drawText (ChordDetector::noteNameFromPitchClass (midiNote % 12),
                                rect.reduced (1.0f, 0), juce::Justification::centred, true);
                }
            }
        }

        // Chord label in label bar
        int labelRow = startRow;  // label appears in the row where chord starts
        float labelRowY = static_cast<float> (labelRow * (rh + rowGap)) + dPad + static_cast<float> (noteAreaH) + 2.0f;
        double rowStartBeat = labelRow * beatsPerRow;
        double rowEndBeat = rowStartBeat + beatsPerRow;
        double clipStart = juce::jmax (chord.startBeat, rowStartBeat);
        double clipEnd = juce::jmin (chord.startBeat + chord.durationBeats, rowEndBeat);
        if (clipEnd > clipStart)
        {
            float lx = leftPad + static_cast<float> (clipStart - rowStartBeat) * bw;
            float lw = static_cast<float> (clipEnd - clipStart) * bw;

            if (isSelected)
            {
                g.setColour (juce::Colour (ChordyTheme::accent).withAlpha (0.3f));
                g.fillRoundedRectangle (lx, labelRowY, lw,
                                        static_cast<float> (detailedChordLabelHeight), 3.0f);
            }

            // Only draw chord name if there's enough space to be readable
            if (lw > 40.0f)
            {
                g.setColour (juce::Colour (isSelected ? ChordyTheme::textPrimary : ChordyTheme::textSecondary));
                g.setFont (juce::FontOptions (11.0f));
                g.drawText (chord.getDisplayName(),
                            juce::Rectangle<float> (lx, labelRowY, lw,
                                                    static_cast<float> (detailedChordLabelHeight)),
                            juce::Justification::centred, false);
            }
        }
    }

    // Draw end marker (yellow) in edit mode
    if (editMode && totalBeats > 0.0)
    {
        int endRow = getRowForBeat (totalBeats);
        double endRowStart = endRow * beatsPerRow;
        float endX = leftPad + static_cast<float> (totalBeats - endRowStart) * bw;
        float endY = static_cast<float> (endRow * (rh + rowGap));

        g.setColour (juce::Colour (ChordyTheme::accent));
        g.fillRect (endX - 1.5f, endY, 3.0f, static_cast<float> (rh));

        juce::Path tri;
        float triY = endY + static_cast<float> (rh);
        tri.addTriangle (endX - 5.0f, triY, endX + 5.0f, triY, endX, triY - 6.0f);
        g.fillPath (tri);
    }

    // Draw cursor
    if (cursorBeat >= 0.0 && cursorBeat < totalBeats)
    {
        int row = getRowForBeat (cursorBeat);
        double rowStartBeat = row * beatsPerRow;
        float x = leftPad + static_cast<float> (cursorBeat - rowStartBeat) * bw;
        float y = static_cast<float> (row * (rh + rowGap));

        g.setColour (juce::Colour (ChordyTheme::chartCursor));
        g.fillRect (x - 1.0f, y, 2.0f, static_cast<float> (rh));
    }
}
