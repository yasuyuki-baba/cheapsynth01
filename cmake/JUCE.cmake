# JUCE CMake support file

# Settings for downloading JUCE modules
include(FetchContent)

# Build JUCE with the bundled helper tools so that juceaide is available
set(JUCE_MODULES_ONLY OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Disable unused X11 and web components to minimise dependencies
add_compile_definitions(
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISABLE_NATIVE_SCREEN_CAPTURE=1
    JUCE_DISABLE_WEBKIT=1
    JUCE_USE_XRANDR=0
    JUCE_USE_XINERAMA=0
    JUCE_USE_XSHM=0
    JUCE_USE_XCURSOR=0
)

# Fetch content from JUCE Git repository
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8  # Use latest version (improved compatibility with macOS 15)
)

# Make JUCE available
FetchContent_MakeAvailable(JUCE)



# Ensure JUCE's CMake helpers are discoverable
list(APPEND CMAKE_MODULE_PATH "${juce_SOURCE_DIR}/extras/Build/CMake")
include("${juce_SOURCE_DIR}/extras/Build/CMake/JUCEUtils.cmake")

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
