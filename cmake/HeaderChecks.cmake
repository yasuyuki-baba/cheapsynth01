# Compile each project header in isolation using its owning target's settings.
# Do not link the owner: JUCE's interface sources would compile its modules again.
function(add_header_checks check_target owner header_root)
    file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${header_root}/*.h")
    set(check_sources)
    foreach(header IN LISTS headers)
        file(RELATIVE_PATH relative_header "${header_root}" "${header}")
        # BinaryData.h is generated/resource glue, not a maintained project header.
        if(relative_header STREQUAL "BinaryData.h")
            continue()
        endif()
        set(source "${CMAKE_CURRENT_BINARY_DIR}/header_checks/${check_target}/${relative_header}.cpp")
        get_filename_component(source_directory "${source}" DIRECTORY)
        file(MAKE_DIRECTORY "${source_directory}")
        # configure_file preserves timestamps when the content has not changed.
        set(HEADER_CHECK_INCLUDE "${relative_header}")
        configure_file("${PROJECT_SOURCE_DIR}/cmake/HeaderCheck.cpp.in" "${source}" @ONLY)
        list(APPEND check_sources "${source}")
    endforeach()

    add_library(${check_target} OBJECT ${check_sources})
    foreach(property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS)
        set_property(TARGET ${check_target} PROPERTY ${property}
            "$<TARGET_PROPERTY:${owner},${property}>")
    endforeach()
    set_target_properties(${check_target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        UNITY_BUILD OFF
        DISABLE_PRECOMPILE_HEADERS ON
    )
    # The owner generates JuceHeader.h and any binary resource headers.
    add_dependencies(${check_target} ${owner})
endfunction()