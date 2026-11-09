# Smoke-test the Windows game packager without initializing a graphics device.
# Creates a small project, exports it, checks the manifest/assets and ensures
# an existing destination cannot be overwritten.
if(NOT DEFINED EDITOR OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "Pass -DEDITOR=<exe> and -DTEST_DIR=<directory>.")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
set(SOURCE "${TEST_DIR}/Source")
set(DEST "${TEST_DIR}/Output")
set(PACKAGE "${DEST}/Smoke-Windows")
file(MAKE_DIRECTORY "${SOURCE}/Assets/Scenes" "${SOURCE}/Assets/Scripts" "${DEST}")

file(WRITE "${SOURCE}/Assets/Scenes/Main.scene"
    "MyEngineScene\nEnvironment 1 4 1 1 1 320 1 0.003 0.32 2 80\nEntities 0\nHierarchyFolders 0\n")
file(WRITE "${SOURCE}/Assets/Scripts/Smoke.lua" "function OnCreate() end\n")
file(WRITE "${SOURCE}/Smoke.project"
    "Version 1\nName \"Smoke Game\"\nAssetDirectory \"Assets\"\nStartupScene \"Assets/Scenes/Main.scene\"\nSettings \"ProjectSettings.cfg\"\n")

execute_process(
    COMMAND "${EDITOR}" --export-game "${SOURCE}/Smoke.project" "${PACKAGE}"
    RESULT_VARIABLE EXPORT_RESULT OUTPUT_VARIABLE EXPORT_OUT ERROR_VARIABLE EXPORT_ERR
    TIMEOUT 45)
if(NOT EXPORT_RESULT EQUAL 0)
    message(FATAL_ERROR "Export failed (${EXPORT_RESULT}): ${EXPORT_OUT} ${EXPORT_ERR}")
endif()

foreach(FILE Game.exe Game.project VelcrynGame.cfg README.txt
             ProjectSettings.cfg Assets/Scenes/Main.scene Assets/Scripts/Smoke.lua)
    if(NOT EXISTS "${PACKAGE}/${FILE}")
        message(FATAL_ERROR "Export is missing: ${FILE}")
    endif()
endforeach()
# Verify that the copied Game.exe finds its package beside itself even when
# launched from an unrelated current working directory, without starting Vulkan.
file(MAKE_DIRECTORY "${TEST_DIR}/UnrelatedWorkingDirectory")
execute_process(
    COMMAND "${PACKAGE}/Game.exe" --verify-game-package
    WORKING_DIRECTORY "${TEST_DIR}/UnrelatedWorkingDirectory"
    RESULT_VARIABLE VERIFY_RESULT OUTPUT_VARIABLE VERIFY_OUT ERROR_VARIABLE VERIFY_ERR
    TIMEOUT 30)
if(NOT VERIFY_RESULT EQUAL 0)
    message(FATAL_ERROR "Exported Game.exe cannot verify its package: ${VERIFY_OUT} ${VERIFY_ERR}")
endif()

file(READ "${PACKAGE}/Game.project" MANIFEST)
if(NOT MANIFEST MATCHES "StartupScene \"Assets/Scenes/Main.scene\"")
    message(FATAL_ERROR "Export manifest has the wrong startup scene.")
endif()

execute_process(
    COMMAND "${EDITOR}" --export-game "${SOURCE}/Smoke.project" "${PACKAGE}"
    RESULT_VARIABLE REPEAT_RESULT TIMEOUT 45)
if(REPEAT_RESULT EQUAL 0)
    message(FATAL_ERROR "Exporter overwrote an existing game folder.")
endif()

execute_process(
    COMMAND "${EDITOR}" --export-game "${SOURCE}/Smoke.project" "${SOURCE}/Assets/BadExport"
    RESULT_VARIABLE INSIDE_RESULT TIMEOUT 45)
if(INSIDE_RESULT EQUAL 0)
    message(FATAL_ERROR "Exporter allowed output inside the source project.")
endif()

# Preflight must not create missing asset directories in the source project.
file(WRITE "${SOURCE}/MissingAssets.project"
    "Version 1\nName \"Broken\"\nAssetDirectory \"MissingAssets\"\nStartupScene \"MissingAssets/Scenes/Main.scene\"\nSettings \"ProjectSettings.cfg\"\n")
execute_process(
    COMMAND "${EDITOR}" --export-game "${SOURCE}/MissingAssets.project" "${DEST}/ShouldNotExist"
    RESULT_VARIABLE MISSING_RESULT TIMEOUT 45)
if(MISSING_RESULT EQUAL 0 OR EXISTS "${SOURCE}/MissingAssets")
    message(FATAL_ERROR "Missing asset directory was accepted or created during preflight.")
endif()

file(GLOB STAGING_FOLDERS "${DEST}/*.velcryn-staging-*")
if(STAGING_FOLDERS)
    message(FATAL_ERROR "Exporter left temporary staging folders: ${STAGING_FOLDERS}")
endif()
message(STATUS "Game export smoke test passed: portable files, startup scene, settings, safety checks.")
