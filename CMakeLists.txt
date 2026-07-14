cmake_minimum_required(VERSION 3.10)

project(
    SmallGine
    VERSION 0.1
    LANGUAGES CXX
    )

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(smallgine ../game.cpp
    core/node.hpp
    core/types.hpp
    core/scene.hpp
    core/engine.hpp
    platform/window.hpp
    tools/helpers.hpp)

target_include_directories(smallgine
    PRIVATE
        ..
        ../core
        ../platform
        ../audio
        ../tools
)

target_link_libraries(smallgine
    PRIVATE
        OpenGL::GL
        glfw
)