cmake_minimum_required(VERSION 3.10)

project(
    SmallGine
    VERSION 0.1
    LANGUAGES CXX
    )

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(smallgine
    game.cpp
    resources/stb_impl.cpp
    core/node.hpp
    core/scene.hpp
    core/camera.hpp
    core/config.hpp
    core/constants.hpp
    core/glmjson.hpp
    core/material.hpp
    core/light.hpp
    core/engine.hpp
    platform/window.hpp
    platform/glcontext.hpp
    platform/paths.hpp
    tools/helpers.hpp
    tools/shader.hpp
    tools/glcheck.hpp
    tools/text.hpp
    tools/frustum.hpp
    resources/mesh.hpp
    resources/objloader.hpp
    resources/gltf.hpp
    resources/skybox.hpp
    resources/instanced.hpp
    resources/postfx.hpp
    resources/shadow.hpp
    resources/texture.hpp
    audio/audio.hpp)

target_compile_options(smallgine PRIVATE -Wall -Wextra)

target_include_directories(smallgine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        ${CMAKE_CURRENT_SOURCE_DIR}/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(smallgine
    PRIVATE
        OpenGL::GL
        GLESv2
        glfw
        openal
)

# Stage runtime assets next to the binary so it runs from any working directory.
add_custom_command(TARGET smallgine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/settings.json $<TARGET_FILE_DIR:smallgine>/settings.json
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:smallgine>/assets
)
