# JUCE CMake support file

# Settings for downloading JUCE modules
include(FetchContent)

# Fetch content from JUCE Git repository
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8  # Latest stable version
)

# Make JUCE available
FetchContent_MakeAvailable(JUCE)

# JUCE related helper functions
function(target_link_juce_modules target)
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
