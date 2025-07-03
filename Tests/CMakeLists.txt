cmake_minimum_required(VERSION 3.15)

# Test project name
project(CheapSynth01Tests VERSION 1.0.0 LANGUAGES C CXX)

# Include JUCE modules
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JUCE.cmake)

# Compiler settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create test executable
juce_add_console_app(CheapSynth01Tests
    PRODUCT_NAME "CheapSynth01Tests"
    COMPANY_NAME "Yasuyuki Baba"
    BUNDLE_ID "org.github.yasuyukibaba.cheapsynth01tests"
    MODULES
        juce_core
        juce_events
        juce_audio_basics
        juce_audio_devices
        juce_audio_formats
        juce_audio_processors
        juce_dsp
        juce_gui_basics
        juce_gui_extra
        juce_graphics
        juce_data_structures
)

# Generate JUCE header
juce_generate_juce_header(CheapSynth01Tests)

# Source files
target_sources(CheapSynth01Tests
    PRIVATE
        TestRunner.cpp
        BinaryData.cpp
        mocks/MockTimer.h
        mocks/MockOscillator.h
        mocks/MockToneGenerator.h
        unit/IG02610LPFTest.cpp
        unit/ToneGeneratorTest.cpp
        unit/CS01VCFProcessorTest.cpp
        unit/VCAProcessorTest.cpp
        unit/EGProcessorTest.cpp
        unit/LFOProcessorTest.cpp
        unit/VCOProcessorTest.cpp
        unit/MidiProcessorTest.cpp
        unit/ModernVCFProcessorTest.cpp
        unit/NoiseGeneratorTest.cpp
        integration/AudioGraphTest.cpp
)

# Include source files to be tested
target_sources(CheapSynth01Tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/IG02610LPF.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/ToneGenerator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/CS01VCFProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/VCAProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/EGProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/LFOProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/VCOProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/MidiProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/ModernVCFProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/NoiseGenerator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01AudioProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01AudioProcessorEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/BreathControlComponent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/CS01LookAndFeel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/EGComponent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/FilterTypeComponent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/LFOComponent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/ModulationComponent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/OscilloscopeComponent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/PresetPanel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/VCAComponent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/VCFComponent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/VCOComponent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/VolumeComponent.cpp
)

# Include directories
target_include_directories(CheapSynth01Tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/mocks"
        "${CMAKE_CURRENT_BINARY_DIR}/JuceLibraryCode"
)

# Link JUCE modules
target_link_juce_modules(CheapSynth01Tests)

# Compiler warning settings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Windows (MSVC) specific compiler options
    target_compile_options(CheapSynth01Tests
        PRIVATE
        /wd4996  # Equivalent to -Wno-deprecated-declarations
    )
else()
    # GCC/Clang (Unix) specific compiler options
    target_compile_options(CheapSynth01Tests
        PRIVATE
        -Wno-deprecated-declarations
        -Wno-error=deprecated-declarations
    )
endif()

# JUCE settings
target_compile_definitions(CheapSynth01Tests
    PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISABLE_NATIVE_SCREEN_CAPTURE=1
    JUCE_DISABLE_WEBKIT=1
)

message(STATUS "CheapSynth01Tests configuration complete!")
