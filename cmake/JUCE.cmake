# JUCE CMake support file

# Settings for downloading JUCE modules
include(FetchContent)

# Fetch content from JUCE Git repository
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3  # Pinned stable release
)

# Make JUCE available
FetchContent_MakeAvailable(JUCE)
include("${CMAKE_CURRENT_LIST_DIR}/JuceGraphRealtimePatch.cmake")

# JUCE related helper functions
function(target_link_juce_modules target)
    target_include_directories(${target} BEFORE PRIVATE "${CHEAPSYNTH_JUCE_RT_INCLUDE}")
    target_link_libraries(${target} 
        PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    )
endfunction()
