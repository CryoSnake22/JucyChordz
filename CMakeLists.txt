cmake_minimum_required(VERSION 3.24)

# Change to your project name

project(
  Chordy
  VERSION 0.0.1
  LANGUAGES C CXX)

add_subdirectory(JUCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(SOURCE_FILES Source/BeatIndicatorComponent.cpp Source/BeatIndicatorComponent.h
                 Source/ChordDetector.cpp Source/ChordDetector.h
                 Source/ChordySynth.cpp Source/ChordySynth.h
                 Source/ChordyKeyboardComponent.cpp Source/ChordyKeyboardComponent.h
                 Source/ChordyLookAndFeel.cpp Source/ChordyLookAndFeel.h
                 Source/PluginEditor.cpp Source/PluginEditor.h
                 Source/PluginProcessor.cpp Source/PluginProcessor.h
                 Source/PracticePanel.cpp Source/PracticePanel.h
                 Source/SpacedRepetition.cpp Source/SpacedRepetition.h
                 Source/TempoEngine.cpp Source/TempoEngine.h
                 Source/VoicingLibraryPanel.cpp Source/VoicingLibraryPanel.h
                 Source/VoicingModel.cpp Source/VoicingModel.h
                 Source/VoicingStatsChart.cpp Source/VoicingStatsChart.h)

# Change these to your own preferences
juce_add_plugin(
  ${PROJECT_NAME}
  COMPANY_NAME
  JezzterInc
  IS_SYNTH
  FALSE
  NEEDS_MIDI_INPUT
  TRUE
  NEEDS_MIDI_OUTPUT
  TRUE
  IS_MIDI_EFFECT
  FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS
  TRUE
  JUCE_VST3_CAN_REPLACE_VST2
  FALSE
  COPY_PLUGIN_AFTER_BUILD
  TRUE
  PLUGIN_MANUFACTURER_CODE
  Tap1
  PLUGIN_CODE
  Chrd
  FORMATS
  VST3
  AU
  Standalone
  PRODUCT_NAME
  "${PROJECT_NAME}")

# How we want our SourceFiles to appear in our IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_FILES})

# Make the SourceFiles buildable
target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})

# These are some toggleable options from the JUCE CMake API
target_compile_definitions(
  ${PROJECT_NAME} PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
                         JUCE_VST3_CAN_REPLACE_VST2=0)

# JUCE libraries to bring into our project
target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC juce::juce_analytics
         juce::juce_audio_basics
         juce::juce_audio_devices
         juce::juce_core
         juce::juce_data_structures
         juce::juce_graphics
         juce::juce_gui_basics
         juce::juce_gui_extra
         juce::juce_audio_utils
         juce::juce_dsp
         juce::juce_recommended_config_flags
         juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)
