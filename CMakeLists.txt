cmake_minimum_required(VERSION 3.24)

project(3DEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# Dependencies
#

include(FetchContent)

#
# SDL3
#
add_subdirectory(
    vendor/SDL
    EXCLUDE_FROM_ALL
)

#
# Lua 5.4
#
FetchContent_Declare(
    lua54
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.9
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(lua54)

#
# Build Lua as a static library
#
file(
    GLOB LUA_SOURCES
    "${lua54_SOURCE_DIR}/*.c"
)

list(
    REMOVE_ITEM
    LUA_SOURCES

    "${lua54_SOURCE_DIR}/lua.c"
    "${lua54_SOURCE_DIR}/luac.c"
    "${lua54_SOURCE_DIR}/onelua.c"
    "${lua54_SOURCE_DIR}/ltests.c"
)

add_library(
    lua54
    STATIC
    ${LUA_SOURCES}
)

target_include_directories(
    lua54
    PUBLIC
    "${lua54_SOURCE_DIR}"
)

if (MSVC)
    target_compile_definitions(
        lua54
        PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

#
# sol2
#
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhd/sol2.git
    GIT_TAG v3.3.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(sol2)

add_library(
    sol2_headers
    INTERFACE
)

target_include_directories(
    sol2_headers
    INTERFACE
    "${sol2_SOURCE_DIR}/include"
)

#
# Engine source files
#
file(
    GLOB_RECURSE
    ENGINE_SOURCES
    CONFIGURE_DEPENDS

    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.h"
)

#
# Main executable
#
add_executable(
    3DEngine

    ${ENGINE_SOURCES}

    #
    # GLAD
    #
    vendor/glad/src/gl.c

    #
    # ImGuizmo
    #
    vendor/ImGuizmo/src/ImGuizmo.cpp

    #
    # Dear ImGui
    #
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_demo.cpp

    #
    # Dear ImGui SDL3/OpenGL backends
    #
    vendor/imgui/backends/imgui_impl_sdl3.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

#
# Include directories
#
target_include_directories(
    3DEngine
    PRIVATE

    vendor/glad/include

    vendor/imgui
    vendor/imgui/backends

    vendor/ImGuizmo/src

    "${lua54_SOURCE_DIR}"
)

#
# Link libraries
#
target_link_libraries(
    3DEngine
    PRIVATE

    SDL3::SDL3
    lua54
    sol2_headers
)

#
# ImGui configuration
#
target_compile_definitions(
    3DEngine
    PRIVATE

    IMGUI_IMPL_OPENGL_LOADER_GLAD2
)

#
# Windows libraries
#
if (WIN32)
    target_link_libraries(
        3DEngine
        PRIVATE

        ole32
        shell32
    )
endif()

#
# Windows subsystem
#
#if (WIN32 AND MSVC)
#    target_link_options(
#        3DEngine
#        PRIVATE
#        "/SUBSYSTEM:WINDOWS"
#        "/ENTRY:mainCRTStartup"
#    )
#endif()