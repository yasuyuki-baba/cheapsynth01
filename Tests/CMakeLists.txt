cmake_minimum_required(VERSION 3.15)

# Test project name
project(CheapSynth01Tests VERSION 1.0.0 LANGUAGES C CXX)

# Include JUCE modules
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/JUCE.cmake)

# Compiler settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent for Google Test
include(FetchContent)

# Fetch Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

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
        unit/OriginalVCFProcessorTest.cpp
        unit/VCAProcessorTest.cpp
        unit/EGProcessorTest.cpp
        unit/LFOProcessorTest.cpp
        unit/VCOProcessorTest.cpp
        unit/MidiProcessorTest.cpp
        unit/ModernVCFProcessorTest.cpp
        unit/NoiseGeneratorTest.cpp
        unit/ProgramManagerTest.cpp
        integration/AudioGraphTest.cpp
)

# Include source files to be tested
target_sources(CheapSynth01Tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/ProgramManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/IG02610LPF.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/ToneGenerator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/CS01Synth/OriginalVCFProcessor.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/ProgramPanel.cpp
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

# Link JUCE modules and Google Test
target_link_juce_modules(CheapSynth01Tests)
target_link_libraries(CheapSynth01Tests
    PRIVATE
        gmock_main
)

# Compiler warning settings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Windows (MSVC) specific compiler options
    target_compile_options(CheapSynth01Tests
        PRIVATE
        /W4             # Warning level 4
        /permissive-    # Enforce standards conformance
        /wd4996        # Disable deprecated functions warnings
        /MP             # Multi-processor compilation
    )
else()
    # GCC/Clang (Unix) specific compiler options
    target_compile_options(CheapSynth01Tests
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
        target_compile_options(CheapSynth01Tests
            PRIVATE
            -ffunction-sections
            -fdata-sections
        )
    endif()
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
