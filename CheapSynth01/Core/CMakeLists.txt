include(FetchContent)
set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tinyxml2 GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git GIT_TAG 10.0.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(tinyxml2)
set(PRESET_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/FactoryPresets.h")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(preset_entries "")
set(PRESET_NAMES Default Flute Violin Trumpet Clavinet Solo_Synth_Lead Synth_Bass)
foreach(preset IN LISTS PRESET_NAMES)
    set(path "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Presets/${preset}.xml")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    file(READ "${path}" xml)
    string(REPLACE "_" " " display "${preset}")
    string(APPEND preset_entries "{\"${display}\",\"${preset}.xml\",R\"preset(${xml})preset\"},\n")
endforeach()
# file(CONFIGURE) updates the header only when its content changes. Substituting
# the entries once with @ONLY preserves literal XML text inside each preset.
file(CONFIGURE OUTPUT "${PRESET_HEADER}" CONTENT [=[
#pragma once
#include <array>
namespace cs01 {
struct FactoryPreset { const char* name; const char* filename; const char* xml; };
inline constexpr std::array<FactoryPreset,7> factoryPresets{{
@preset_entries@}};
}
]=] @ONLY)
add_library(CheapSynth01Core STATIC
    SynthEngine.cpp
    ProgramManager.cpp
    CS01Synth/ToneGenerator.cpp
    CS01Synth/YM10150.cpp
    CS01Synth/NoiseGenerator.cpp
    CS01Synth/EGProcessor.cpp
    CS01Synth/LFOProcessor.cpp
    CS01Synth/MidiProcessor.cpp
    CS01Synth/OriginalVCFProcessor.cpp
    CS01Synth/ModernVCFProcessor.cpp
    CS01Synth/VCAProcessor.cpp
    CS01Synth/IG02600BehavioralModel.cpp
    CS01Synth/CS01VCFCircuit.cpp
    CS01Synth/CS01IIVCFCircuit.cpp
    CS01Synth/IG05630BehavioralModel.cpp
)
set_target_properties(CheapSynth01Core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(CheapSynth01Core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(CheapSynth01Core PRIVATE tinyxml2::tinyxml2)
if(MSVC)
    target_compile_options(CheapSynth01Core PRIVATE /W4)
else()
    target_compile_options(CheapSynth01Core PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()
