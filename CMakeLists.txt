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

option(SMALLGINE_DEMOS "Build the engine demo executables (demos/*)" ON)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT REQUIRED luajit)

# Shared engine sources, compiled once and linked by the app + every demo.
add_library(smallgine_core STATIC
    core/engine.cpp
    resources/stb_impl.cpp
    resources/texture.cpp
    resources/obj.cpp
    resources/gltf.cpp
    resources/skinnedgltf.cpp
    resources/script.cpp)

target_compile_options(smallgine_core PRIVATE -Wall -Wextra)

target_include_directories(smallgine_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        ${CMAKE_CURRENT_SOURCE_DIR}/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${LUAJIT_INCLUDE_DIRS}
)

target_link_libraries(smallgine_core
    PUBLIC
        OpenGL::GL
        GLESv2
        glfw
        openal
        Threads::Threads
        ${LUAJIT_LIBRARIES}
)

# Copy the shared runtime assets next to a target's binary (staged per output dir
# so the app and each demo run standalone from their own directory).
function(smallgine_stage_assets tgt)
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${tgt}>/assets)
endfunction()

# Asset packager: a standalone host tool (no GL) that bundles a folder into one
# .sgpk file the engine mounts transparently. Usage: smallgine_pack assets assets.sgpk
add_executable(smallgine_pack tools/packager/main.cpp)
target_compile_options(smallgine_pack PRIVATE -Wall -Wextra)
target_include_directories(smallgine_pack PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Stage a self-contained "assets.sgpk" next to a target's binary: copy the shared
# assets into a per-target stage dir, optionally drop in an extra scene JSON, then
# pack it. The engine mounts the pack at startup (loose files stay as a fallback),
# so every app + demo actually runs from a single-file package.
#   smallgine_stage_pack(<tgt> [<extraJsonSrc> <extraJsonName>])
function(smallgine_stage_pack tgt)
    set(stage ${CMAKE_BINARY_DIR}/_packstage/${tgt})
    set(extra_copy "")
    if(ARGC GREATER 2)
        set(extra_copy COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ARGV1} ${stage}/assets/${ARGV2})
    endif()
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${stage}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${stage}/assets
        ${extra_copy}
        COMMAND $<TARGET_FILE:smallgine_pack> ${stage}/assets $<TARGET_FILE_DIR:${tgt}>/assets.sgpk assets
        VERBATIM
        COMMENT "Packing assets -> ${tgt}/assets.sgpk")
    add_dependencies(${tgt} smallgine_pack)
endfunction()

# Main app: engine sandbox showcase.
add_executable(smallgine game.cpp)
target_link_libraries(smallgine PRIVATE smallgine_core)
smallgine_stage_assets(smallgine)
add_custom_command(TARGET smallgine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/settings.json $<TARGET_FILE_DIR:smallgine>/settings.json)
smallgine_stage_pack(smallgine)

# Demos: each links the engine lib and stages its own settings + scene.
if(SMALLGINE_DEMOS)
    add_executable(demo_fps demos/fps/main.cpp)
    target_link_libraries(demo_fps PRIVATE smallgine_core)
    set_target_properties(demo_fps PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/demos/fps)
    smallgine_stage_assets(demo_fps)
    add_custom_command(TARGET demo_fps POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/fps/settings.json $<TARGET_FILE_DIR:demo_fps>/settings.json
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/fps/fps.json $<TARGET_FILE_DIR:demo_fps>/assets/fps.json)
    smallgine_stage_pack(demo_fps ${CMAKE_CURRENT_SOURCE_DIR}/demos/fps/fps.json fps.json)

    add_executable(demo_rts demos/rts/main.cpp)
    target_link_libraries(demo_rts PRIVATE smallgine_core)
    set_target_properties(demo_rts PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/demos/rts)
    smallgine_stage_assets(demo_rts)
    add_custom_command(TARGET demo_rts POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/rts/settings.json $<TARGET_FILE_DIR:demo_rts>/settings.json
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/rts/rts.json $<TARGET_FILE_DIR:demo_rts>/assets/rts.json)
    smallgine_stage_pack(demo_rts ${CMAKE_CURRENT_SOURCE_DIR}/demos/rts/rts.json rts.json)

    add_executable(demo_race demos/race/main.cpp)
    target_link_libraries(demo_race PRIVATE smallgine_core)
    set_target_properties(demo_race PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/demos/race)
    smallgine_stage_assets(demo_race)
    add_custom_command(TARGET demo_race POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/race/settings.json $<TARGET_FILE_DIR:demo_race>/settings.json
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/race/race.json $<TARGET_FILE_DIR:demo_race>/assets/race.json)
    smallgine_stage_pack(demo_race ${CMAKE_CURRENT_SOURCE_DIR}/demos/race/race.json race.json)

    add_executable(demo_space demos/space/main.cpp)
    target_link_libraries(demo_space PRIVATE smallgine_core)
    set_target_properties(demo_space PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/demos/space)
    smallgine_stage_assets(demo_space)
    add_custom_command(TARGET demo_space POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/space/settings.json $<TARGET_FILE_DIR:demo_space>/settings.json
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/space/space.json $<TARGET_FILE_DIR:demo_space>/assets/space.json)
    smallgine_stage_pack(demo_space ${CMAKE_CURRENT_SOURCE_DIR}/demos/space/space.json space.json)

    add_executable(demo_noise demos/noise/main.cpp)
    target_link_libraries(demo_noise PRIVATE smallgine_core)
    set_target_properties(demo_noise PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/demos/noise)
    smallgine_stage_assets(demo_noise)
    add_custom_command(TARGET demo_noise POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/noise/settings.json $<TARGET_FILE_DIR:demo_noise>/settings.json
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/demos/noise/noise.json $<TARGET_FILE_DIR:demo_noise>/assets/noise.json)
    smallgine_stage_pack(demo_noise ${CMAKE_CURRENT_SOURCE_DIR}/demos/noise/noise.json noise.json)
endif()

# No-GL unit tests: data structures, JSON round-trips, math. No window/context.
enable_testing()
add_executable(smallgine_tests tests/unit.cpp)
target_include_directories(smallgine_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(smallgine_tests PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME unit COMMAND smallgine_tests)
