cmake_minimum_required(VERSION 3.24)

project(VelcrynEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use one MSVC runtime consistently across Velcryn and every dependency created
# below (SDL, Lua, NRI, etc.). This must be set before FetchContent adds targets.
# A cache variable is used because several dependencies configure nested targets.
if (MSVC)
    set(
        CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        CACHE STRING "MSVC runtime used by Velcryn and dependencies"
        FORCE
    )
endif()

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

#
# cgltf - lightweight glTF 2.0 / GLB parser
#
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cgltf)

#
# NRI - Velcryn's explicit low-level rendering backend
#
# Keep NRI behind the engine-owned RHI. Vulkan is the first explicit backend;
# D3D12 stays disabled until the Vulkan path is established and tested.
set(NRI_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(NRI_ENABLE_VK_SUPPORT ON CACHE BOOL "" FORCE)
set(NRI_ENABLE_D3D11_SUPPORT OFF CACHE BOOL "" FORCE)
set(NRI_ENABLE_D3D12_SUPPORT OFF CACHE BOOL "" FORCE)
set(NRI_ENABLE_NONE_SUPPORT OFF CACHE BOOL "" FORCE)
set(NRI_ENABLE_VALIDATION_SUPPORT ON CACHE BOOL "" FORCE)
set(NRI_ENABLE_NVTX_SUPPORT OFF CACHE BOOL "" FORCE)
set(NRI_ENABLE_IMGUI_EXTENSION OFF CACHE BOOL "" FORCE)
set(NRI_ENABLE_NIS_SDK OFF CACHE BOOL "" FORCE)
set(NRI_ENABLE_NGX_SDK OFF CACHE BOOL "" FORCE)
set(NRI_ENABLE_FFX_SDK OFF CACHE BOOL "" FORCE)
set(NRI_ENABLE_XESS_SDK OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    NRI
    GIT_REPOSITORY https://github.com/NVIDIA-RTX/NRI.git
    GIT_TAG v180
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(NRI)

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
# Embedded editor resources
#
# Keep editor/runtime UI infrastructure independent from project Assets.
# The generated source is compiled into the executable and never needs to be
# shipped as a loose font file.
set(INTER_FONT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Editor/Fonts/InterVariable.ttf")
set(INTER_FONT_CPP "${CMAKE_CURRENT_BINARY_DIR}/Generated/InterFont.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated")
file(READ "${INTER_FONT_FILE}" INTER_FONT_HEX HEX)
string(REGEX REPLACE "(..)" "0x\\1," INTER_FONT_BYTES "${INTER_FONT_HEX}")
file(WRITE "${INTER_FONT_CPP}"
"#include <cstddef>\n"
"extern const unsigned char g_InterFontData[] = {${INTER_FONT_BYTES}};\n"
"extern const std::size_t g_InterFontDataSize = sizeof(g_InterFontData);\n")
unset(INTER_FONT_HEX)
unset(INTER_FONT_BYTES)

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

# Mirror the physical Engine directory hierarchy in IDE generators (Visual
# Studio/Xcode) instead of presenting the engine as one flat source list.
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
    PREFIX "Engine"
    FILES ${ENGINE_SOURCES}
)

#
# Main executable
#
add_executable(
    VelcrynEditor

    ${ENGINE_SOURCES}
    ${INTER_FONT_CPP}

    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/Engine/Branding/Velcryn.rc>

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
    VelcrynEditor
    PRIVATE

    vendor/glad/include

    vendor/imgui
    vendor/imgui/backends

    vendor/ImGuizmo/src

    "${lua54_SOURCE_DIR}"
    "${cgltf_SOURCE_DIR}"
)

#
# Link libraries
#
target_link_libraries(
    VelcrynEditor
    PRIVATE

    SDL3::SDL3-static
    lua54
    sol2_headers
    NRI
)

#
# ImGui / build configuration
#
target_compile_definitions(
    VelcrynEditor
    PRIVATE

    IMGUI_IMPL_OPENGL_LOADER_GLAD2
    $<$<CONFIG:Debug>:ENGINE_DEBUG=1>
    $<$<CONFIG:Release>:ENGINE_RELEASE=1>
    $<$<CONFIG:RelWithDebInfo>:ENGINE_RELEASE=1>
)

if (MSVC)
    target_compile_options(
        VelcrynEditor
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
        VelcrynEditor
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
#        VelcrynEditor
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
    VelcrynEditor
    PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# SDL3, Lua, GLAD, ImGui and ImGuizmo are statically linked, so no
# project-owned runtime DLL needs to be copied beside the executable.

# Make the engine executable the startup target for Visual Studio generators.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VelcrynEditor)

#
# Portable distribution
#
# Build with:
#   cmake --preset x64-Release
#   cmake --build --preset x64-Release
#   cmake --build --preset x64-Release --target package_portable
#
# Produces a self-contained editor executable plus external project folders:
#
#   portable/
#     VelcrynEditor.exe
#     Projects/
#       <ProjectName>/
#
# Engine/editor resources are compiled into the executable. Game content stays
# project-owned and portable under Projects/.
set(PORTABLE_DIR "${CMAKE_BINARY_DIR}/portable")

add_custom_target(
    package_portable
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PORTABLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PORTABLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:VelcrynEditor>"
            "${PORTABLE_DIR}/VelcrynEditor.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Projects"
            "${PORTABLE_DIR}/Projects"
    DEPENDS VelcrynEditor
    COMMENT "Creating portable Velcryn Editor + Projects package"
    VERBATIM
)
