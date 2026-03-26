#pragma once

#include <juce_core/juce_core.h>

namespace ChordyTheme
{
    // --- Backgrounds ---
    constexpr juce::uint32 bgDeepest      = 0xFF141414;
    constexpr juce::uint32 bgSurface      = 0xFF1E1E1E;
    constexpr juce::uint32 bgElevated     = 0xFF272727;
    constexpr juce::uint32 bgHover        = 0xFF303030;
    constexpr juce::uint32 bgSelected     = 0xFF383838;

    // --- Borders ---
    constexpr juce::uint32 border         = 0xFF3A3A3A;
    constexpr juce::uint32 borderFocus    = 0xFF505050;

    // --- Text ---
    constexpr juce::uint32 textPrimary    = 0xFFE8E8E8;
    constexpr juce::uint32 textSecondary  = 0xFF999999;
    constexpr juce::uint32 textTertiary   = 0xFF666666;

    // --- Accent (Amber) ---
    constexpr juce::uint32 accent         = 0xFFE8A634;
    constexpr juce::uint32 accentHover    = 0xFFF0B84A;
    constexpr juce::uint32 accentMuted    = 0xFF8C6420;

    // --- Semantic ---
    constexpr juce::uint32 success        = 0xFF4ADE80;
    constexpr juce::uint32 successBright  = 0xFF22FF66;
    constexpr juce::uint32 warning        = 0xFFEAB308;
    constexpr juce::uint32 danger         = 0xFFEF4444;
    constexpr juce::uint32 dangerMuted    = 0xFFB91C1C;
    constexpr juce::uint32 info           = 0xFF5EEAD4;

    // --- Keyboard ---
    constexpr juce::uint32 keyCorrect     = 0x884ADE80;
    constexpr juce::uint32 keyWrong       = 0x88EF4444;
    constexpr juce::uint32 keyTarget      = 0xAA5EEAD4;
    constexpr juce::uint32 keyDown        = 0xBBF0B84A;  // warm amber overlay when key is pressed
    constexpr juce::uint32 keyHover       = 0x44E8A634;  // subtle amber on mouse hover

    // --- Beat indicator ---
    constexpr juce::uint32 beatDownbeat   = 0xFFE8A634;
    constexpr juce::uint32 beatUpbeat     = 0xFF4ADE80;
    constexpr juce::uint32 beatInactive   = 0xFF3A3A3A;

    // --- Quality scoring ---
    constexpr juce::uint32 qualityPerfect   = 0xFF4ADE80;
    constexpr juce::uint32 qualityGood      = 0xFF6EE7A0;
    constexpr juce::uint32 qualityOk        = 0xFFEAB308;
    constexpr juce::uint32 qualitySlow      = 0xFFF97316;
    constexpr juce::uint32 qualityCorrected = 0xFFEF4444;
    constexpr juce::uint32 qualityTimeout   = 0xFFDC2626;

    // --- Typography ---
    constexpr const char* fontFamily  = "Avenir Next";
    constexpr float fontTitle         = 22.0f;
    constexpr float fontChordDisplay  = 36.0f;
    constexpr float fontChordBig      = 48.0f;
    constexpr float fontSectionHead   = 14.0f;
    constexpr float fontBody          = 13.0f;
    constexpr float fontSmall         = 11.0f;
    constexpr float fontMeta          = 10.0f;

    // --- Chart ---
    constexpr juce::uint32 chartGrid          = 0xFF2A2A2A;
    constexpr juce::uint32 chartBarLine       = 0xFF4A4A4A;
    constexpr juce::uint32 chartChordBg       = 0xFF2E2E2E;
    constexpr juce::uint32 chartChordSelected = 0x44E8A634;
    constexpr juce::uint32 chartCursor        = 0xAAE8A634;
    constexpr juce::uint32 chartPassingChord  = 0xFF252525;

    // --- Melody chart ---
    constexpr juce::uint32 melodyNoteBg        = 0xFF3A3A3A;
    constexpr juce::uint32 melodyNoteActive    = 0xFF6EE7A0;
    constexpr juce::uint32 melodyNoteCorrect   = 0xFF4ADE80;
    constexpr juce::uint32 melodyNoteMissed    = 0xFFEF4444;
    constexpr juce::uint32 melodyChordBg       = 0xFF2A2A3A;
    constexpr juce::uint32 melodyChordActive   = 0x664ADE80;

    // --- Spacing ---
    constexpr int panelPadding        = 12;
    constexpr int sectionGap          = 10;
    constexpr int itemGap             = 6;
    constexpr float cornerRadius      = 8.0f;
    constexpr float buttonRadius      = 6.0f;
    constexpr float smallRadius       = 4.0f;
}
