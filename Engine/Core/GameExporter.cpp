#include "GameExporter.h"
#include "ProjectManager.h"
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace fs = std::filesystem;

bool ExportGame(const fs::path& descriptor, const fs::path& destination,
                const fs::path& executable, std::string& error)
{
    try
    {
        ProjectManager manager;
        if (!manager.Load(descriptor.string())) throw std::runtime_error("Cannot load the project descriptor.");
        const auto& project = manager.GetActiveProject();
        const auto root = fs::weakly_canonical(project.rootDirectory);
        const auto output = fs::weakly_canonical(fs::absolute(destination));
        const auto within = [](const fs::path& child, const fs::path& parent) {
            auto relative = child.lexically_relative(parent);
            return !relative.empty() && *relative.begin() != "..";
        };
        if (fs::exists(output)) throw std::runtime_error("Choose a new export folder; existing folders are never overwritten.");
        if (within(output, root) || within(root, output))
            throw std::runtime_error("Export outside the source project folder.");
        for (auto path : {project.assetDirectory, project.startupScene, project.settingsFile})
        {
            if (path.empty() || path.is_absolute() || !within(fs::weakly_canonical(root / path), root))
                throw std::runtime_error("Assets, startup scene and settings must use project-relative paths.");
        }
        if (!fs::is_regular_file(project.GetStartupScenePath())) throw std::runtime_error("The project needs a saved startup scene.");
        if (!fs::is_directory(project.GetAssetRoot())) throw std::runtime_error("Project asset directory is missing.");
        if (!fs::is_regular_file(executable)) throw std::runtime_error("Build the editor before exporting.");

        // Validate the complete copy set before creating any output. Linked folders
        // could escape the project or recurse into the export.
        for (const auto& item : fs::recursive_directory_iterator(project.GetAssetRoot()))
            if (fs::is_symlink(item.symlink_status()) || !within(fs::weakly_canonical(item.path()), root))
                throw std::runtime_error("Export does not support linked assets outside the project.");

        fs::create_directories(output);
        fs::copy_file(executable, output / "Game.exe");
        fs::create_directories((output / project.assetDirectory).parent_path());
        fs::copy(project.GetAssetRoot(), output / project.assetDirectory, fs::copy_options::recursive);
        if (fs::is_regular_file(project.GetSettingsPath()))
        {
            fs::create_directories((output / project.settingsFile).parent_path());
            fs::copy_file(project.GetSettingsPath(), output / project.settingsFile);
        }
        for (const auto& item : fs::directory_iterator(executable.parent_path()))
            if (item.is_regular_file() && item.path().extension() == ".dll")
                fs::copy_file(item.path(), output / item.path().filename());
        std::ofstream manifest(output / "Game.project");
        manifest << "Version 1\nName " << std::quoted(project.name)
                 << "\nAssetDirectory " << std::quoted(project.assetDirectory.generic_string())
                 << "\nStartupScene " << std::quoted(project.startupScene.generic_string())
                 << "\nSettings " << std::quoted(project.settingsFile.generic_string()) << '\n';
        manifest.close();
        if (!manifest) throw std::runtime_error("Could not write the exported descriptor.");
        std::ofstream readme(output / "README.txt");
        readme << project.name << "\n\nLaunch Game.exe. Share this entire folder, not only the executable.\n"
               << "Windows x64 with a Vulkan-capable graphics driver is required.\n"
               << "F11 toggles fullscreen. Alt+F4 closes the game.\n"
               << "Export includes saved project assets and settings; save editor changes first.\n";
        // Publish the launch marker last so an interrupted copy cannot look complete.
        std::ofstream marker(output / "VelcrynGame.cfg");
        marker << "Game.project\n";
        marker.close();
        if (!marker) throw std::runtime_error("Could not finish the exported launch configuration.");
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}
