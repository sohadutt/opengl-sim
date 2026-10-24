cmake_minimum_required(VERSION 4.4)
project(opengl-sim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(cmake/Dependencies.cmake)
include(cmake/LocalGLFWRuntime.cmake)

function(configure_simulator_target target)
    target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()

# Core settings and logging are independent from rendering and physics.
add_library(sim_core STATIC
    src/core/config.cpp
    src/core/logger.cpp
)
configure_simulator_target(sim_core)

# Physics data and calculations depend only on GLM.
add_library(sim_physics STATIC
    src/physics/black_hole.cpp
)
configure_simulator_target(sim_physics)
target_link_libraries(sim_physics PUBLIC "${GLM_TARGET}")

# Rendering owns the camera and OpenGL drawing code.
add_library(sim_render STATIC
    src/render/camera.cpp
    src/render/renderer.cpp
)
configure_simulator_target(sim_render)
target_link_libraries(sim_render PUBLIC sim_core sim_physics PRIVATE "${OPENGL_TARGET}")
if(APPLE)
    target_compile_definitions(sim_render PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Reusable on-screen controls use their own OpenGL backend and know nothing
# about the simulation model; the app binds widgets to model properties.
add_library(sim_ui STATIC
    src/ui/user_interface.cpp
)
configure_simulator_target(sim_ui)
target_link_libraries(sim_ui PUBLIC sim_core PRIVATE "${OPENGL_TARGET}")
if(APPLE)
    target_compile_definitions(sim_ui PRIVATE GL_SILENCE_DEPRECATION)
endif()

# The executable is the composition root: it connects app input to the modules.
add_executable(opengl-sim
    src/main.cpp
    src/app/application.cpp
)
configure_simulator_target(opengl-sim)
target_link_libraries(opengl-sim PRIVATE sim_core sim_physics sim_render sim_ui "${GLFW_TARGET}")
target_compile_definitions(opengl-sim PRIVATE
    SIMULATOR_CONFIG_FILE="${PROJECT_SOURCE_DIR}/config/simulator.conf"
)
if(MSVC)
    target_compile_definitions(opengl-sim PRIVATE _CONSOLE)
endif()
set_target_properties(opengl-sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
configure_local_glfw_runtime(opengl-sim)
