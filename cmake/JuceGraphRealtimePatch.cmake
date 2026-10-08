# JUCE 9.0.3 GraphRenderSequence::perform resizes its prepared output scratch
# with avoidReallocating=false. MIDI segmentation therefore reallocates it on
# the callback when lengths change. Override just this implementation include,
# retaining the upstream source/headers and all licence notices unchanged.
# Re-audit and remove/update this patch on any JUCE upgrade.
file(READ "${juce_SOURCE_DIR}/CMakeLists.txt" juce_project_source)
if(NOT juce_project_source MATCHES "project\\(JUCE VERSION 9\\.0\\.3 LANGUAGES")
    message(FATAL_ERROR "Re-audit the graph realtime patch before changing JUCE version")
endif()
set(CHEAPSYNTH_JUCE_RT_INCLUDE "${CMAKE_BINARY_DIR}/juce-rt-overrides")
set(graph_source "${juce_SOURCE_DIR}/modules/juce_audio_processors_headless/processors/juce_AudioProcessorGraph.cpp")
file(READ "${graph_source}" graph_original)
set(graph_needle "currentAudioOutputBuffer.setSize (jmax (1, buffer.getNumChannels()), numSamples);")
string(REPLACE "${graph_needle}" "" graph_without_needle "${graph_original}")
string(LENGTH "${graph_original}" graph_original_length)
string(LENGTH "${graph_without_needle}" graph_stripped_length)
string(LENGTH "${graph_needle}" graph_needle_length)
math(EXPR graph_removed_length "${graph_original_length} - ${graph_stripped_length}")
if(NOT graph_removed_length EQUAL graph_needle_length)
    message(FATAL_ERROR "JUCE graph patch context changed; refusing an unverified patch")
endif()
string(REPLACE "${graph_needle}"
    "currentAudioOutputBuffer.setSize (jmax (1, buffer.getNumChannels()), numSamples, false, false, true);"
    graph_patched "${graph_original}")
set(graph_patched "// Modified by CheapSynth01 on 2026-10-09: reuse prepared graph output storage.\n// Reproduce this change with cmake/JuceGraphRealtimePatch.cmake.\n${graph_patched}")
set(graph_override "${CHEAPSYNTH_JUCE_RT_INCLUDE}/juce_audio_processors_headless/processors/juce_AudioProcessorGraph.cpp")
set(graph_existing "")
if(EXISTS "${graph_override}")
    file(READ "${graph_override}" graph_existing)
endif()
if(NOT graph_existing STREQUAL graph_patched)
    file(WRITE "${graph_override}" "${graph_patched}")
endif()
