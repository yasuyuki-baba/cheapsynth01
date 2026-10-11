include(FetchContent)
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.14.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(googletest)
add_executable(CheapSynth01Tests
    CoreTest.cpp
    unit/OnsetObservationTest.cpp
    unit/ToneMeasurementTest.cpp
    reference/BehavioralBeforeRefactor.cpp
    unit/RCEnvelopeModelTest.cpp
    unit/CircuitBehaviorTest.cpp
    unit/SignalPathTest.cpp
    unit/EngineTimelineTest.cpp
    unit/PresetBehaviorTest.cpp
    unit/VcoBehaviorTest.cpp
    unit/MidiBehaviorTest.cpp
    unit/EditorSessionTest.cpp
    unit/KeyboardInputTest.cpp
    unit/DspRegressionTest.cpp
    unit/PluginIdentityTest.cpp
    unit/PatchPublicationTest.cpp
    unit/EnvelopeContinuityTest.cpp
    unit/ExperimentalIG05630Test.cpp
    unit/ExperimentalOriginalVCFTest.cpp
)
target_link_libraries(CheapSynth01Tests PRIVATE CheapSynth01Core GTest::gtest_main tinyxml2::tinyxml2)
target_compile_definitions(CheapSynth01Tests PRIVATE CS01_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
include(GoogleTest)
gtest_discover_tests(CheapSynth01Tests DISCOVERY_TIMEOUT 30)
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/TestLabels.cmake")

# Exercise the real SDK interface types, without linking a plugin or opening an
# editor/device. Native-product CI prepares these headers. Core-only builds can
# opt in explicitly when auditing the pinned SDK.
set(boundary_tests_default OFF)
if(BUILD_PLUGINS AND NOT STANDALONE_ONLY)
    set(boundary_tests_default ON)
endif()
option(BUILD_VST3_BOUNDARY_TESTS "Test native preset queues and state streams using VST3 SDK interfaces" ${boundary_tests_default})
if(BUILD_VST3_BOUNDARY_TESTS)
    set(boundary_sdk "${IPLUG2_DIR}/Dependencies/IPlug/VST3_SDK")
    if(NOT EXISTS "${boundary_sdk}/pluginterfaces/vst/ivstparameterchanges.h")
        message(FATAL_ERROR "VST3 boundary tests require the prepared VST3 SDK")
    endif()
    add_executable(CheapSynth01ParameterPresentationTests
        unit/ParameterPresentationTest.cpp
        "${IPLUG2_DIR}/IPlug/IPlugParameter.cpp"
    )
    target_include_directories(CheapSynth01ParameterPresentationTests PRIVATE
        "${PROJECT_SOURCE_DIR}/CheapSynth01" "${PROJECT_SOURCE_DIR}/CheapSynth01/core"
        "${IPLUG2_DIR}/IPlug" "${IPLUG2_DIR}/WDL")
    # Match SDK product compilation: Windows headers must not replace std::min/max.
    target_compile_definitions(CheapSynth01ParameterPresentationTests PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_link_libraries(CheapSynth01ParameterPresentationTests PRIVATE GTest::gtest_main)
    gtest_discover_tests(CheapSynth01ParameterPresentationTests DISCOVERY_TIMEOUT 30
        PROPERTIES LABELS "regression;integration;parameter-presentation")
    add_executable(CheapSynth01VST3BoundaryTests
        unit/VST3ParameterChangesTest.cpp
    )
    target_include_directories(CheapSynth01VST3BoundaryTests PRIVATE
        "${PROJECT_SOURCE_DIR}/CheapSynth01" "${PROJECT_SOURCE_DIR}/CheapSynth01/core" "${boundary_sdk}")
    target_link_libraries(CheapSynth01VST3BoundaryTests PRIVATE GTest::gtest_main)
    gtest_discover_tests(CheapSynth01VST3BoundaryTests DISCOVERY_TIMEOUT 30
        PROPERTIES LABELS "regression;integration;vst3-boundary")
endif()
