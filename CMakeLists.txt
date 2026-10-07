cmake_minimum_required(VERSION 3.30)

project(VelcrynEngine)

# Compile engine-owned HLSL to Vulkan SPIR-V and embed it in the executable.
# VELCRYN_DXC can be set explicitly on hosts without dxc in PATH or a Windows SDK.
file(GLOB DXC_SDK_DIRS "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
list(SORT DXC_SDK_DIRS ORDER DESCENDING)
find_program(VELCRYN_DXC NAMES dxc HINTS "${CMAKE_CURRENT_SOURCE_DIR}/out/tools/dxc/bin/x64" "$ENV{VULKAN_SDK}/Bin" ${DXC_SDK_DIRS} REQUIRED)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Generated")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/Generated/CompilerProbe.hlsl" "float4 main() : SV_Target { return 1; }\n")
execute_process(COMMAND "${VELCRYN_DXC}" -spirv -T ps_6_0 -E main
    -Fo "${CMAKE_CURRENT_BINARY_DIR}/Generated/CompilerProbe.spv"
    "${CMAKE_CURRENT_BINARY_DIR}/Generated/CompilerProbe.hlsl"
    RESULT_VARIABLE DXC_PROBE_RESULT ERROR_VARIABLE DXC_PROBE_ERROR)
if(NOT DXC_PROBE_RESULT EQUAL 0)
    message(FATAL_ERROR "DXC must support SPIR-V. Set VELCRYN_DXC to the standalone DXC or Vulkan SDK compiler. ${DXC_PROBE_ERROR}")
endif()
set(VELCRYN_SHADER_HEADERS)
foreach(STAGE vert frag)
    if(STAGE STREQUAL "vert")
        set(PROFILE vs_6_0)
    else()
        set(PROFILE ps_6_0)
    endif()
    set(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Graphics/Shaders/Mesh.${STAGE}.hlsl")
    set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/Generated/Mesh.${STAGE}.spv")
    set(HEADER "${CMAKE_CURRENT_BINARY_DIR}/Generated/Mesh.${STAGE}.h")
    add_custom_command(OUTPUT "${HEADER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/Generated"
        COMMAND "${VELCRYN_DXC}" -spirv -fspv-target-env=vulkan1.2 -T ${PROFILE} -E main
            "$<IF:$<CONFIG:Debug>,-Od,-O3>" -Fo "${SPIRV}" "${SOURCE}"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${SPIRV}" "-DOUTPUT=${HEADER}" "-DSYMBOL=Mesh_${STAGE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
        DEPENDS "${SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
        VERBATIM)
    list(APPEND VELCRYN_SHADER_HEADERS "${HEADER}")
endforeach()

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
set(SDL_OPENGL OFF CACHE BOOL "Vulkan renderer does not use SDL OpenGL" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "Vulkan renderer does not use SDL OpenGL ES" FORCE)
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
set(NRI_ENABLE_IMGUI_EXTENSION ON CACHE BOOL "" FORCE)
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
# NRI v180 mirrors ImGui's renderer-facing texture structs internally. Velcryn
# currently vendors ImGui 1.93 WIP, which added QueueUserData to ImTextureData.
# Patch the fetched NRI mirror before compiling it so both sides have the same
# ABI. Without this, NRI reads Width/Height at the old offsets and sees width=0.
FetchContent_GetProperties(NRI)
if (NOT nri_POPULATED)
    # Populate before add_subdirectory so the ImGui ABI patch is applied first.
    cmake_policy(PUSH)
    cmake_policy(SET CMP0169 OLD)
    FetchContent_Populate(NRI)
    cmake_policy(POP)
endif()

set(NRI_IMGUI_IMPL "${nri_SOURCE_DIR}/Source/Shared/ImguiInterface.hpp")
file(READ "${NRI_IMGUI_IMPL}" NRI_IMGUI_IMPL_CONTENT)

# Use real newlines here. The previous patch searched for the two-character
# sequence "\\n", so it never matched NRI's source and silently wrote it back
# unchanged.
set(NRI_IMGUI_OLD [=[    void* BackendUserData;
    ImTextureID TexID;]=])
set(NRI_IMGUI_NEW [=[    void* BackendUserData;
    void* QueueUserData;
    ImTextureID TexID;]=])

string(FIND "${NRI_IMGUI_IMPL_CONTENT}" "${NRI_IMGUI_NEW}" NRI_IMGUI_ALREADY_PATCHED)
if (NRI_IMGUI_ALREADY_PATCHED EQUAL -1)
    string(FIND "${NRI_IMGUI_IMPL_CONTENT}" "${NRI_IMGUI_OLD}" NRI_IMGUI_PATCH_POINT)
    if (NRI_IMGUI_PATCH_POINT EQUAL -1)
        message(FATAL_ERROR "NRI v180 ImGui ABI patch point was not found; refusing to build an ABI-mismatched NRIImgui extension.")
    endif()
    string(REPLACE "${NRI_IMGUI_OLD}" "${NRI_IMGUI_NEW}" NRI_IMGUI_IMPL_CONTENT "${NRI_IMGUI_IMPL_CONTENT}")
    file(WRITE "${NRI_IMGUI_IMPL}" "${NRI_IMGUI_IMPL_CONTENT}")
endif()

# ImGui 1.93 also extended ImDrawCmd with TexRef before VtxOffset. NRI v180
# already has TexRef, but its mirrored ImTextureData is the only layout that
# changed in this revision. Patch NRI's font-texture lookup to use the texture
# pointer key it owns instead of the ImGui TexID value. This avoids operator[]
# inserting a null descriptor when ImGui 1.93's TexID lifecycle differs.
set(NRI_IMGUI_FONT_LOOKUP_OLD [=[                    ImTextureID key = drawCmd.TexRef.TexData->TexID;
                    descriptor = m_Textures[key].descriptor;]=])
set(NRI_IMGUI_FONT_LOOKUP_NEW [=[                    ImTextureData* textureData = drawCmd.TexRef.TexData;
                    uint64_t key = (uint64_t)textureData | ((uint64_t)textureData->UniqueID << 54ull);
                    auto textureIt = m_Textures.find(key);
                    descriptor = textureIt != m_Textures.end() ? textureIt->second.descriptor : nullptr;]=])
string(REPLACE "${NRI_IMGUI_FONT_LOOKUP_OLD}" "${NRI_IMGUI_FONT_LOOKUP_NEW}" NRI_IMGUI_IMPL_CONTENT "${NRI_IMGUI_IMPL_CONTENT}")
file(WRITE "${NRI_IMGUI_IMPL}" "${NRI_IMGUI_IMPL_CONTENT}")

# Verify the fetched source actually contains the ABI field before NRI compiles.
file(READ "${NRI_IMGUI_IMPL}" NRI_IMGUI_IMPL_VERIFIED)
string(FIND "${NRI_IMGUI_IMPL_VERIFIED}" "void* QueueUserData;" NRI_IMGUI_QUEUE_USER_DATA)
if (NRI_IMGUI_QUEUE_USER_DATA EQUAL -1)
    message(FATAL_ERROR "NRI v180 ImGui ABI patch failed: QueueUserData is missing.")
endif()

add_subdirectory("${nri_SOURCE_DIR}" "${nri_BINARY_DIR}")

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
    ${VELCRYN_SHADER_HEADERS}

    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/Engine/Branding/Velcryn.rc>

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
    # Dear ImGui SDL3 platform backend; NRI owns GPU rendering.
    #
    vendor/imgui/backends/imgui_impl_sdl3.cpp
)

#
# Include directories
#
target_include_directories(
    VelcrynEditor
    PRIVATE

    vendor/imgui
    "${CMAKE_CURRENT_BINARY_DIR}/Generated"
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

# Keep runtime branding available when launching the executable outside the
# repository working directory. The native .ico remains compiled into the EXE.
add_custom_command(
    TARGET VelcrynEditor
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:VelcrynEditor>/Engine/Branding"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Branding/VelcrynLogo.png"
            "$<TARGET_FILE_DIR:VelcrynEditor>/Engine/Branding/VelcrynLogo.png"
)

# SDL3, Lua, NRI, ImGui and ImGuizmo are statically linked, so no
# project-owned runtime DLL needs to be copied beside the executable.

# Make the engine executable the startup target for Visual Studio generators.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VelcrynEditor)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME RHI.Smoke COMMAND ${CMAKE_COMMAND}
        "-DEDITOR=$<TARGET_FILE:VelcrynEditor>" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DLOG_DIR=${CMAKE_CURRENT_BINARY_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunRHISmokeTest.cmake")
    set_tests_properties(RHI.Smoke PROPERTIES TIMEOUT 45 LABELS "gpu;vulkan")
endif()

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
