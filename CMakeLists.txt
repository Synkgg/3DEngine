cmake_minimum_required(VERSION 3.24)

project(3DEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep single-config generators useful too. Visual Studio remains multi-config
# and can switch Debug/Release from the IDE normally.
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

#
# Dependencies
#

include(FetchContent)

#
# SDL3
#
# SDL is not checked into vendor/. Fetch the pinned SDL3 release just like
# Lua and sol2 so a fresh clone/configure works without a manual dependency.
#
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.20
    GIT_SHALLOW TRUE
)

# Build SDL into the executable instead of shipping SDL3.dll.
# SDL's CMake project exposes SDL3::SDL3-static when SDL_STATIC is enabled.
set(SDL_SHARED OFF CACHE BOOL "Build SDL shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)
FetchContent_MakeAvailable(SDL3)

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

    SDL3::SDL3-static
    lua54
    sol2_headers
)

#
# ImGui / build configuration
#
target_compile_definitions(
    3DEngine
    PRIVATE

    IMGUI_IMPL_OPENGL_LOADER_GLAD2
    $<$<CONFIG:Debug>:ENGINE_DEBUG=1>
    $<$<CONFIG:Release>:ENGINE_RELEASE=1>
    $<$<CONFIG:RelWithDebInfo>:ENGINE_RELEASE=1>
)

# Link the Microsoft C/C++ runtime statically as well, so the portable
# package does not depend on the VC runtime DLLs being installed beside it.
if (MSVC)
    set_property(TARGET 3DEngine PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_property(TARGET lua54 PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    if (TARGET SDL3-static)
        set_property(TARGET SDL3-static PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()


    target_compile_options(
        3DEngine
        PRIVATE
        $<$<CONFIG:Debug>:/Od;/Zi>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2;/Zi>
    )
endif()

#
# Windows libraries
#
if (WIN32)
    target_link_libraries(
        3DEngine
        PRIVATE

        ole32
        shell32
        ws2_32
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

#
# Runtime assets / debugger working directory
#
# The editor resolves project content from the repository working directory.
# Force Visual Studio's debugger to use that directory in both Debug and
# Release, while also copying Assets beside the executable for direct launches.
#
set_target_properties(
    3DEngine
    PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# SDL3, Lua, GLAD, ImGui and ImGuizmo are statically linked, so no
# project-owned runtime DLL needs to be copied beside the executable.

add_custom_command(
    TARGET 3DEngine
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:3DEngine>/Assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
            "$<TARGET_FILE_DIR:3DEngine>/Assets"
    COMMENT "Syncing Assets for $<CONFIG> runtime"
    VERBATIM
)


# Make the engine executable the startup target for Visual Studio generators.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT 3DEngine)

#
# Portable distribution
#
# Build with:
#   cmake --preset x64-Release
#   cmake --build --preset x64-Release
#   cmake --build --preset x64-Release --target package_portable
#
# The resulting folder contains only the executable and Assets directory.
set(PORTABLE_DIR "${CMAKE_BINARY_DIR}/portable")

add_custom_target(
    package_portable
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PORTABLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PORTABLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:3DEngine>" "${PORTABLE_DIR}/3DEngine.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
            "${PORTABLE_DIR}/Assets"
    DEPENDS 3DEngine
    COMMENT "Creating portable 3DEngine.exe + Assets package"
    VERBATIM
)
