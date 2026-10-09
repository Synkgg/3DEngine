#include "GameExporter.h"
#include "ProjectManager.h"
#include "ProjectSettings.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace
{
    bool IsChildOf(const fs::path& child, const fs::path& parent)
    {
        const fs::path relative = child.lexically_relative(parent);
        return !relative.empty() && relative != "." && *relative.begin() != "..";
    }

    void RequireProjectRelative(const fs::path& path, const fs::path& root, const char* name)
    {
        if (path.empty() || path.is_absolute() || path.has_root_name() ||
            !IsChildOf(fs::weakly_canonical(root / path), root))
            throw std::runtime_error(std::string(name) + " must be a path inside the project.");

        for (const auto& part : path)
            if (part == "..")
                throw std::runtime_error(std::string(name) + " cannot contain '..'.");
    }

    struct ExportSource
    {
        Project project;
        fs::path root;
        fs::path assets;
        fs::path scene;
        fs::path settings;
        GameExportSummary summary;
    };

    ExportSource ReadSource(const fs::path& descriptor)
    {
        if (!fs::is_regular_file(descriptor))
            throw std::runtime_error("Select a saved .project file before exporting.");

        ProjectManager manager;
        if (!manager.Load(descriptor.string()))
            throw std::runtime_error("Cannot read the project descriptor.");

        ExportSource source{};
        source.project = manager.GetActiveProject();
        source.root = fs::canonical(source.project.rootDirectory);
        RequireProjectRelative(source.project.assetDirectory, source.root, "Asset directory");
        RequireProjectRelative(source.project.startupScene, source.root, "Startup scene");
        RequireProjectRelative(source.project.settingsFile, source.root, "Project settings");

        source.assets = fs::weakly_canonical(source.project.GetAssetRoot());
        source.scene = fs::weakly_canonical(source.project.GetStartupScenePath());
        source.settings = fs::weakly_canonical(source.project.GetSettingsPath());

        if (!fs::is_directory(source.assets))
            throw std::runtime_error("The project's asset directory does not exist.");
        if (!fs::is_regular_file(source.scene) || source.scene.extension() != ".scene")
            throw std::runtime_error("Set and save a valid startup .scene before exporting.");
        if (!IsChildOf(source.scene, source.assets))
            throw std::runtime_error("The startup scene must be inside the project asset directory.");
        if (source.project.settingsFile == "Game.project" ||
            source.project.settingsFile == "VelcrynGame.cfg")
            throw std::runtime_error("Project settings path conflicts with an export manifest.");

        // A portable package must never include links into private/external
        // folders. Reject symlinks rather than following them during copying.
        if (fs::is_symlink(fs::symlink_status(source.project.GetAssetRoot())) ||
            fs::is_symlink(fs::symlink_status(source.project.GetStartupScenePath())) ||
            (fs::exists(source.project.GetSettingsPath()) &&
             fs::is_symlink(fs::symlink_status(source.project.GetSettingsPath()))))
            throw std::runtime_error("Linked project assets/settings are not supported.");

        source.summary.projectName = source.project.name;
        source.summary.startupScene = source.project.startupScene;
        for (const auto& item : fs::recursive_directory_iterator(source.assets))
        {
            if (fs::is_symlink(item.symlink_status()) ||
                !IsChildOf(fs::weakly_canonical(item.path()), source.root))
                throw std::runtime_error("Export cannot include linked assets or files outside the project.");
            if (item.is_regular_file())
            {
                ++source.summary.assetFileCount;
                source.summary.assetBytes += item.file_size();
            }
        }
        if (source.summary.projectName.empty() ||
            source.summary.projectName.find_first_of("\r\n") != std::string::npos)
            throw std::runtime_error("Project name is empty or contains a newline.");
        return source;
    }

    // Remove only a staging folder that this exporter successfully created.
    struct StagingCleanup
    {
        fs::path path;
        bool active = true;
        ~StagingCleanup()
        {
            if (active)
            {
                std::error_code ignored;
                fs::remove_all(path, ignored);
            }
        }
    };

    void WriteProjectManifest(const ExportSource& source, const fs::path& staging)
    {
        std::ofstream manifest(staging / "Game.project", std::ios::trunc);
        if (!manifest) throw std::runtime_error("Cannot write Game.project.");
        manifest << "Version 1\nName " << std::quoted(source.project.name)
                 << "\nAssetDirectory " << std::quoted(source.project.assetDirectory.generic_string())
                 << "\nStartupScene " << std::quoted(source.project.startupScene.generic_string())
                 << "\nSettings " << std::quoted(source.project.settingsFile.generic_string()) << '\n';
        manifest.close();
        if (!manifest) throw std::runtime_error("Could not finish Game.project.");
    }
}

bool InspectGameExport(const fs::path& descriptor, GameExportSummary& summary, std::string& error)
{
    summary = {};
    error.clear();
    try
    {
        summary = ReadSource(descriptor).summary;
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

bool ExportGame(const fs::path& descriptor, const fs::path& destination,
                const fs::path& executable, std::string& error)
{
    error.clear();
    try
    {
#if !defined(_WIN32)
        throw std::runtime_error("Windows export requires a Windows build of Velcryn.");
#else
        const ExportSource source = ReadSource(descriptor);
        if (destination.empty())
            throw std::runtime_error("Choose an output folder.");
        const fs::path output = fs::absolute(destination).lexically_normal();
        if (output.filename().empty() || output.filename() == "." || output.filename() == "..")
            throw std::runtime_error("Choose a named, new output folder.");
        if (fs::exists(output))
            throw std::runtime_error("That export folder already exists. Choose a new folder name.");

        const fs::path parent = fs::canonical(output.parent_path());
        if (!fs::is_directory(parent))
            throw std::runtime_error("The export destination parent folder does not exist.");
        const fs::path resolvedOutput = (parent / output.filename()).lexically_normal();
        if (IsChildOf(resolvedOutput, source.root) || IsChildOf(source.root, resolvedOutput) ||
            resolvedOutput == source.root)
            throw std::runtime_error("Choose a destination outside the source project.");

        if (!fs::is_regular_file(executable) || executable.extension() != ".exe")
            throw std::runtime_error("Build VelcrynEditor.exe for Windows before exporting.");
        const fs::path sourceExe = fs::canonical(executable);
        if (IsChildOf(sourceExe, resolvedOutput))
            throw std::runtime_error("The executable cannot be inside the export folder.");

        // Build in a new sibling folder and rename only after the entire
        // package has been written. A failed copy never publishes a half-game.
        fs::path staging;
        for (int i = 1; i <= 1000; ++i)
        {
            const fs::path candidate = parent /
                (output.filename().string() + ".velcryn-staging-" + std::to_string(i));
            std::error_code ec;
            if (fs::create_directory(candidate, ec))
            {
                staging = candidate;
                break;
            }
            if (ec && !fs::exists(candidate))
                throw std::runtime_error("Cannot create a temporary export folder: " + ec.message());
        }
        if (staging.empty())
            throw std::runtime_error("No available temporary export folder.");
        StagingCleanup cleanup{staging};

        fs::copy_file(sourceExe, staging / "Game.exe");
        fs::create_directories((staging / source.project.assetDirectory).parent_path());
        fs::copy(source.assets, staging / source.project.assetDirectory,
                 fs::copy_options::recursive);

        const fs::path packagedSettings = staging / source.project.settingsFile;
        if (!fs::is_regular_file(packagedSettings))
        {
            fs::create_directories(packagedSettings.parent_path());
            if (fs::is_regular_file(source.settings))
                fs::copy_file(source.settings, packagedSettings);
            else if (!ProjectSettings{}.Save(packagedSettings.string()))
                throw std::runtime_error("Cannot generate default project settings.");
        }

        // SDL/Lua/NRI are normally static, but include any runtime DLLs from
        // the selected build for configurations with dynamic dependencies.
        for (const auto& item : fs::directory_iterator(sourceExe.parent_path()))
        {
            if (item.is_regular_file() && item.path().extension() == ".dll")
                fs::copy_file(item.path(), staging / item.path().filename());
        }

        // Preserve runtime branding resources when present alongside the build.
        const fs::path branding = sourceExe.parent_path() / "Engine" / "Branding" / "VelcrynLogo.png";
        if (fs::is_regular_file(branding))
        {
            fs::create_directories(staging / "Engine" / "Branding");
            fs::copy_file(branding, staging / "Engine" / "Branding" / "VelcrynLogo.png");
        }

        WriteProjectManifest(source, staging);

        std::ofstream readme(staging / "README.txt");
        if (!readme) throw std::runtime_error("Cannot write README.txt.");
        readme << source.project.name << "\n\n"
               << "Launch Game.exe to play. Distribute the entire folder.\n"
               << "Windows x64 and a Vulkan-capable graphics driver are required.\n"
               << "F11 toggles fullscreen. Alt+F4 closes the game.\n"
               << "The package contains saved project assets and settings.\n"
               << "Re-export to include future edits; existing exports are never overwritten.\n";
        readme.close();
        if (!readme) throw std::runtime_error("Cannot finish README.txt.");

        // This marker activates standalone game mode. Write it last so
        // incomplete packages never launch as a game.
        std::ofstream marker(staging / "VelcrynGame.cfg", std::ios::trunc);
        if (!marker) throw std::runtime_error("Cannot write game launch marker.");
        marker << "Game.project\n";
        marker.close();
        if (!marker) throw std::runtime_error("Cannot finish game launch marker.");

        fs::rename(staging, resolvedOutput);
        cleanup.active = false;
        return true;
#endif
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}
