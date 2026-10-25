# GLFW: use the project copy when present, otherwise find a system package.
if(GLFW_INCLUDE_DIR AND NOT EXISTS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h")
    unset(GLFW_INCLUDE_DIR CACHE)
endif()
if(GLFW_LIBRARY AND NOT EXISTS "${GLFW_LIBRARY}")
    unset(GLFW_LIBRARY CACHE)
endif()

find_path(GLFW_INCLUDE_DIR
    NAMES GLFW/glfw3.h
    PATHS "${PROJECT_SOURCE_DIR}/third_party/glfw/include"
    NO_DEFAULT_PATH
)
find_library(GLFW_LIBRARY
    NAMES glfw glfw3 glfw.3.5
    PATHS
        "${PROJECT_SOURCE_DIR}/third_party/glfw"
        "${PROJECT_SOURCE_DIR}/third_party/glfw/lib"
    NO_DEFAULT_PATH
)

if(GLFW_INCLUDE_DIR AND GLFW_LIBRARY)
    add_library(project_glfw UNKNOWN IMPORTED)
    set_target_properties(project_glfw PROPERTIES
        IMPORTED_LOCATION "${GLFW_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
    )
    set(GLFW_TARGET project_glfw)
else()
    set(GLFW_PACKAGE_HINTS)
    if(APPLE)
        find_program(HOMEBREW_EXECUTABLE brew)
        if(HOMEBREW_EXECUTABLE)
            execute_process(
                COMMAND "${HOMEBREW_EXECUTABLE}" --prefix glfw
                OUTPUT_VARIABLE HOMEBREW_GLFW_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE HOMEBREW_GLFW_RESULT
                ERROR_QUIET
            )
            if(HOMEBREW_GLFW_RESULT EQUAL 0)
                list(APPEND GLFW_PACKAGE_HINTS "${HOMEBREW_GLFW_PREFIX}")
            endif()
        endif()
    endif()
    find_package(glfw3 3.3 CONFIG QUIET HINTS ${GLFW_PACKAGE_HINTS})
    if(TARGET glfw)
        set(GLFW_TARGET glfw)
    elseif(TARGET glfw3::glfw)
        set(GLFW_TARGET glfw3::glfw)
    else()
        # Some GLFW installations (including Homebrew setups without the CMake
        # config package) expose only headers and a library. Import those
        # directly so a working Homebrew install does not depend on package
        # config discovery.
        set(GLFW_SYSTEM_INCLUDE_HINTS)
        set(GLFW_SYSTEM_LIBRARY_HINTS)
        foreach(GLFW_PACKAGE_HINT IN LISTS GLFW_PACKAGE_HINTS)
            list(APPEND GLFW_SYSTEM_INCLUDE_HINTS "${GLFW_PACKAGE_HINT}/include")
            list(APPEND GLFW_SYSTEM_LIBRARY_HINTS "${GLFW_PACKAGE_HINT}/lib")
        endforeach()

        find_path(GLFW_SYSTEM_INCLUDE_DIR
            NAMES GLFW/glfw3.h
            HINTS ${GLFW_SYSTEM_INCLUDE_HINTS}
        )
        find_library(GLFW_SYSTEM_LIBRARY
            NAMES glfw glfw3
            HINTS ${GLFW_SYSTEM_LIBRARY_HINTS}
        )

        if(GLFW_SYSTEM_INCLUDE_DIR AND GLFW_SYSTEM_LIBRARY)
            add_library(system_glfw UNKNOWN IMPORTED)
            set_target_properties(system_glfw PROPERTIES
                IMPORTED_LOCATION "${GLFW_SYSTEM_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${GLFW_SYSTEM_INCLUDE_DIR}"
            )
            set(GLFW_TARGET system_glfw)
        else()
            message(FATAL_ERROR
                "GLFW 3.3+ was not found. On macOS, install it with "
                "'brew install glfw', then rerun ./scripts/manage.sh build. "
                "On other systems, install the GLFW development package or "
                "provide a compatible copy under third_party/glfw/."
            )
        endif()
    endif()
endif()

# GLM: support both its CMake package and a plain Homebrew include directory.
find_package(glm CONFIG QUIET)
if(TARGET glm::glm)
    set(GLM_TARGET glm::glm)
else()
    set(GLM_INCLUDE_HINTS)
    if(APPLE)
        find_program(HOMEBREW_EXECUTABLE brew)
        if(HOMEBREW_EXECUTABLE)
            execute_process(
                COMMAND "${HOMEBREW_EXECUTABLE}" --prefix glm
                OUTPUT_VARIABLE HOMEBREW_GLM_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE HOMEBREW_GLM_RESULT
                ERROR_QUIET
            )
            if(HOMEBREW_GLM_RESULT EQUAL 0)
                list(APPEND GLM_INCLUDE_HINTS "${HOMEBREW_GLM_PREFIX}/include")
            endif()
        endif()
    endif()
    find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp HINTS ${GLM_INCLUDE_HINTS})
    if(NOT GLM_INCLUDE_DIR)
        message(FATAL_ERROR "GLM was not found. Install it (for example, with 'brew install glm').")
    endif()
    add_library(project_glm INTERFACE)
    target_include_directories(project_glm SYSTEM INTERFACE "${GLM_INCLUDE_DIR}")
    set(GLM_TARGET project_glm)
endif()

if(APPLE)
    find_package(OpenGL REQUIRED)
    set(OPENGL_TARGET OpenGL::GL)
else()
    find_package(GLEW REQUIRED)
    set(OPENGL_TARGET GLEW::GLEW)
endif()
