# Native products share the framework-independent core.
set(formats APP VST3 CLAP)
if(APPLE)
    list(APPEND formats AU)
endif()
if(STANDALONE_ONLY)
    set(formats APP)
elseif(NOT IPLUG2_CLAP_SUPPORTED)
    message(FATAL_ERROR "CLAP SDK missing. Run bash scripts/setup-iplug2.sh")
endif()
iplug_add_plugin(CheapSynth01
    SOURCES CS01AudioProcessor.cpp CS01AudioProcessor.h
        CS01AudioProcessorEditor.cpp KeyboardFocus.cpp resources/resource.h
    RESOURCES resources/fonts/Roboto-Regular.ttf
    FORMATS ${formats}
    LINK CheapSynth01Core
    DEFINES _USE_MATH_DEFINES)
if(WIN32)
    foreach(format vst3 clap)
        if(TARGET CheapSynth01-${format})
            target_sources(CheapSynth01-${format} PRIVATE resources/main.rc)
            # Both SDK targets use CheapSynth01 as OUTPUT_NAME. MSVC otherwise
            # races while writing their same-named .lib/.exp and linker PDB.
            set_target_properties(CheapSynth01-${format} PROPERTIES
                ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/link/${format}/$<CONFIG>"
                PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/link/${format}/$<CONFIG>")
        endif()
    endforeach()
endif()

if(APPLE)
    set_source_files_properties(KeyboardFocus.cpp PROPERTIES LANGUAGE OBJCXX)
endif()
