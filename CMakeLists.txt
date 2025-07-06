cmake_minimum_required(VERSION 3.15)

# Project name and language settings
project(CheapSynth01 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type to Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Include JUCE modules
include(cmake/JUCE.cmake)

# Add CLAP JUCE extensions
add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

# Compiler settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable GUI support
option(JUCE_HEADLESS_PLUGIN_CLIENT "Build in headless mode" OFF)

# Option for standalone only build
option(STANDALONE_ONLY "Build only standalone version for faster development" OFF)

# Set plugin formats based on platform and build type
if(STANDALONE_ONLY)
    set(PLUGIN_FORMATS Standalone)
    message(STATUS "STANDALONE_ONLY: Building standalone version only")
else()
    set(PLUGIN_FORMATS Standalone VST3)
    
    if(APPLE)
        list(APPEND PLUGIN_FORMATS AU)
    endif()
    
    if(NOT WIN32 OR UNIX)
        list(APPEND PLUGIN_FORMATS LV2)
    endif()
    
    message(STATUS "Building with plugin formats: ${PLUGIN_FORMATS}")
endif()

# JUCE plugin settings
juce_add_plugin(CheapSynth01
    VERSION 1.0.0
    COMPANY_NAME "Yasuyuki Baba"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE BABA
    PLUGIN_CODE CS01
    LV2URI "http://github.com/yasuyukibaba/cheapsynth01"
    FORMATS
        ${PLUGIN_FORMATS}
    PRODUCT_NAME "CheapSynth01"
    BUNDLE_ID "org.github.yasuyukibaba.cheapsynth01"
    MODULES
        juce_core
        juce_events
        juce_graphics
        juce_gui_basics
        juce_audio_basics
        juce_audio_devices
        juce_audio_formats
        juce_audio_processors
        juce_audio_plugin_client
        juce_dsp
)

# Add resources
juce_add_binary_data(CheapSynth01Resources
    SOURCES
        Source/Resources/Presets/Default.xml
        Source/Resources/Presets/Flute.xml
        Source/Resources/Presets/Violin.xml
        Source/Resources/Presets/Trumpet.xml
        Source/Resources/Presets/Clavinet.xml
        Source/Resources/Presets/Solo_Synth_Lead.xml
        Source/Resources/Presets/Synth_Bass.xml
)

# JUCE target settings
target_compile_definitions(CheapSynth01
    PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISABLE_NATIVE_SCREEN_CAPTURE=1
    JUCE_DISABLE_WEBKIT=1
)

# Compiler warning settings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Windows (MSVC) specific compiler options
    target_compile_options(CheapSynth01
        PRIVATE
        /W4             # Warning level 4
        /permissive-    # Enforce standards conformance
        /wd4996        # Disable deprecated functions warnings
        /MP             # Multi-processor compilation
    )
else()
    # GCC/Clang (Unix) specific compiler options
    target_compile_options(CheapSynth01
        PRIVATE
        -Wall           # All warnings
        -Wextra         # Extra warnings
        -Wpedantic      # Strict ISO C/C++
        -Wno-deprecated-declarations  # Needed for JUCE compatibility
        -Wno-error=deprecated-declarations
        -Wno-unused-parameter  # Don't warn about unused parameters
        -Wformat=2     # Stricter format checking
        -fvisibility=hidden  # Better optimization/binary size
    )
    
    # Extra optimization flags for non-Debug builds
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(CheapSynth01
            PRIVATE
            -ffunction-sections
            -fdata-sections
        )
    endif()
endif()

# Standalone settings
juce_generate_juce_header(CheapSynth01)

# Source files
target_sources(CheapSynth01
    PRIVATE
        Source/CS01AudioProcessor.cpp
        Source/CS01AudioProcessorEditor.cpp
        Source/ProgramManager.cpp
        Source/UI/ModulationComponent.cpp
        Source/UI/VCOComponent.cpp
        Source/UI/LFOComponent.cpp
        Source/UI/VCFComponent.cpp
        Source/UI/ProgramPanel.cpp
        Source/UI/VCAComponent.cpp
        Source/UI/EGComponent.cpp
        Source/UI/BreathControlComponent.cpp
        Source/UI/VolumeComponent.cpp
        Source/UI/OscilloscopeComponent.cpp
        Source/CS01Synth/ToneGenerator.cpp
        Source/CS01Synth/OriginalVCFProcessor.cpp
        Source/CS01Synth/VCAProcessor.cpp
        Source/CS01Synth/VCOProcessor.cpp
        Source/CS01Synth/MidiProcessor.cpp
        Source/UI/CS01LookAndFeel.cpp
        Source/CS01Synth/EGProcessor.cpp
        Source/CS01Synth/LFOProcessor.cpp
        Source/CS01Synth/MidiProcessor.cpp
        Source/CS01Synth/ModernVCFProcessor.cpp
        Source/CS01Synth/NoiseGenerator.cpp
        Source/CS01Synth/IG02610LPF.cpp
        Source/UI/FilterTypeComponent.cpp
)

# Header search paths
target_include_directories(CheapSynth01
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

# Link JUCE modules
target_link_juce_modules(CheapSynth01)
target_link_libraries(CheapSynth01 PUBLIC CheapSynth01Resources)

message(STATUS "CheapSynth01 configuration complete!")

# Add CLAP plugin support (only when not building standalone only)
if(NOT STANDALONE_ONLY)
    clap_juce_extensions_plugin(TARGET CheapSynth01
        CLAP_ID "com.yasuyukibaba.cheapsynth01"
        CLAP_FEATURES instrument synthesizer vintage analog
        CLAP_MANUAL_URL "https://github.com/yasuyuki-baba/cheapsynth01"
        CLAP_SUPPORT_URL "https://github.com/yasuyuki-baba/cheapsynth01/issues"
    )
    message(STATUS "CLAP plugin target added: CheapSynth01_CLAP")
endif()

# Add Tests directory
add_subdirectory(Tests)
