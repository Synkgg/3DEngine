# Exporting a standalone Velcryn game (Windows)

Velcryn packages a saved project as a Windows x64 game powered by Vulkan.
The result is a **folder**, not a single EXE: game assets, Lua scripts, settings,
and any runtime DLLs must remain beside the executable.

## Export from the editor

1. Open the project's `.project` file and stop Play mode.
2. Open the scene you want players to start in. Choose **File > Export Game
   (Windows)... > Use open scene as startup** to save it and update the
   project's startup-scene setting.
3. Save edited UI assets and other content. The export dialog can save the
   current scene and project rendering settings; it cannot save every open UI
   document automatically.
4. Select a destination **parent folder** with Browse and enter a **new
   package folder name**. Existing exports are never overwritten.
5. Prefer the x64-Release executable. Build the Release preset first if the
   dialog reports that it will package the running Debug build.
6. Click **Export Game**. Open the output folder and launch `Game.exe`.
   Test menu input, scene switching, audio and rendering there.

The exporter checks the startup scene and asset folder, rejects linked assets
and paths outside the project, copies the complete saved asset tree, writes
the portable `Game.project` manifest and generates default project settings
when none are saved. It builds in a temporary sibling directory and publishes
the final folder only after copying succeeds. Failed exports clean up the
temporary folder.

Typical output:

```text
MyGame-Windows/
  Game.exe
  Game.project
  VelcrynGame.cfg
  ProjectSettings.cfg
  README.txt
  Assets/
    Scenes/
    Scripts/
    UI/
    Textures/
    Models/
    Audio/
  Engine/Branding/VelcrynLogo.png   (if present in the build)
  *.dll                              (if needed)
```

The game resolves assets relative to its exported project root, not the
current working directory. `Game.exe` detects the launch marker and runs
without the editor workspace.

## Command line

From the repository root on Windows:

```powershell
cmake --preset x64-Release
cmake --build --preset x64-Release --parallel 4
& .\out\build\x64-Release\VelcrynEditor.exe --export-game .\Projects\DuelFPS\DuelFPS.project C:\Games\Breakbulk-Windows
```

The final destination must not exist. Exporting into the source project (or
into one of its ancestor folders) is rejected.

## Regression test

After configuring and building:

```powershell
ctest --test-dir out/build/x64-Debug -R '^Game.Export$' --output-on-failure
```

The test creates a fixture project and verifies the packaged executable,
startup scene, Lua asset, generated settings, marker and descriptor. It also
checks that existing destinations and paths inside the project are rejected.
This test does not require a GPU. Run `RHI.Smoke` separately and test the
exported game interactively on a Vulkan-capable Windows computer.

## Scope

Windows x64 is the supported target. This is a standalone game-mode package
of the existing Velcryn executable, not a separate stripped game-only binary
or signed installer. The editor copies the selected build; it does not compile
a Release build automatically. Exported files are a snapshot of saved assets,
so changes require a new export.
