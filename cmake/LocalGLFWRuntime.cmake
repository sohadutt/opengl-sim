function(configure_local_glfw_runtime target)
    if(NOT APPLE OR NOT GLFW_INCLUDE_DIR OR NOT GLFW_LIBRARY)
        return()
    endif()

    find_program(OTOOL_EXECUTABLE otool REQUIRED)
    find_program(INSTALL_NAME_TOOL_EXECUTABLE install_name_tool REQUIRED)
    execute_process(
        COMMAND "${OTOOL_EXECUTABLE}" -D "${GLFW_LIBRARY}"
        OUTPUT_VARIABLE GLFW_DYLIB_ID_OUTPUT
        RESULT_VARIABLE GLFW_DYLIB_ID_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT GLFW_DYLIB_ID_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not read the install name from ${GLFW_LIBRARY}.")
    endif()

    string(REPLACE "\r" "" GLFW_DYLIB_ID_OUTPUT "${GLFW_DYLIB_ID_OUTPUT}")
    string(REPLACE "\n" ";" GLFW_DYLIB_ID_LINES "${GLFW_DYLIB_ID_OUTPUT}")
    list(LENGTH GLFW_DYLIB_ID_LINES GLFW_DYLIB_ID_LINE_COUNT)
    if(GLFW_DYLIB_ID_LINE_COUNT LESS 2)
        message(FATAL_ERROR "Could not parse the install name from ${GLFW_LIBRARY}.")
    endif()
    list(GET GLFW_DYLIB_ID_LINES 1 GLFW_DYLIB_ID)
    get_filename_component(GLFW_DYLIB_FILENAME "${GLFW_LIBRARY}" NAME)

    set_property(TARGET "${target}" PROPERTY BUILD_RPATH
        "@loader_path/../third_party/glfw;@loader_path/third_party/glfw;@loader_path/../third_party/glfw/lib;@loader_path/third_party/glfw/lib")
    add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${INSTALL_NAME_TOOL_EXECUTABLE}" -change
            "${GLFW_DYLIB_ID}" "@rpath/${GLFW_DYLIB_FILENAME}"
            "$<TARGET_FILE:${target}>"
        VERBATIM
    )
endfunction()
