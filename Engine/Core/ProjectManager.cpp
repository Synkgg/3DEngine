#include "ProjectManager.h"
#include "Logger.h"
#include <fstream>
#include <iomanip>
#include <system_error>
#include <array>

bool ProjectManager::Load(const std::string& descriptorPath)
{
    std::filesystem::path descriptor = std::filesystem::absolute(descriptorPath).lexically_normal();
    std::ifstream file(descriptor);
    if (!file)
    {
        Logger::Error("Failed to open project: " + descriptor.string());
        return false;
    }

    Project project;
    project.descriptorPath = descriptor;
    project.rootDirectory = descriptor.parent_path();

    std::string key;
    int version = 0;
    while (file >> key)
    {
        if (key == "Version") file >> version;
        else if (key == "Name") file >> std::quoted(project.name);
        else if (key == "AssetDirectory") { std::string value; file >> std::quoted(value); project.assetDirectory = value; }
        else if (key == "StartupScene") { std::string value; file >> std::quoted(value); project.startupScene = value; }
        else if (key == "Settings") { std::string value; file >> std::quoted(value); project.settingsFile = value; }
        else { std::string ignored; std::getline(file, ignored); }
    }

    if (version != 1)
    {
        Logger::Error("Unsupported project descriptor version: " + std::to_string(version));
        return false;
    }

    if (project.name.empty() || project.assetDirectory.empty())
    {
        Logger::Error("Project descriptor is missing Name or AssetDirectory.");
        return false;
    }

    std::error_code directoryError;
    std::filesystem::create_directories(project.GetAssetRoot(), directoryError);
    if (directoryError)
    {
        Logger::Error("Failed to access project asset directory: " + project.GetAssetRoot().string());
        return false;
    }

    m_Project = std::move(project);
    m_HasProject = true;
    Logger::Info("Opened project: " + m_Project.name + " (" + m_Project.rootDirectory.string() + ")");
    return true;
}

bool ProjectManager::Create(const std::string& directory, const std::string& name)
{
    if (directory.empty() || name.empty())
        return false;

    if (name == "." || name == ".." ||
        name.find_first_of("<>:\\/|?*\"") != std::string::npos)
    {
        Logger::Error("Project name contains characters that are not valid in a folder name.");
        return false;
    }

    std::filesystem::path root = std::filesystem::absolute(directory).lexically_normal();
    std::error_code error;

    if (std::filesystem::exists(root, error) &&
        !std::filesystem::is_empty(root, error))
    {
        Logger::Error("Project directory already exists and is not empty: " + root.string());
        return false;
    }
    std::filesystem::create_directories(root / "Assets" / "Scenes", error);
    std::filesystem::create_directories(root / "Assets" / "Scripts", error);
    std::filesystem::create_directories(root / "Assets" / "UI", error);
    std::filesystem::create_directories(root / "Assets" / "Textures", error);
    std::filesystem::create_directories(root / "Assets" / "Models", error);
    std::filesystem::create_directories(root / "Assets" / "Materials", error);
    std::filesystem::create_directories(root / "Assets" / "Audio", error);
    std::filesystem::create_directories(root / "Assets" / "Fonts", error);
    std::filesystem::create_directories(root / "Assets" / "Prefabs", error);
    if (error)
    {
        Logger::Error("Failed to create project directories: " + root.string());
        return false;
    }

    // A project should be runnable immediately after creation. Give it a
    // project-owned startup scene rather than depending on any engine sample.
    const std::filesystem::path startupScene = root / "Assets" / "Scenes" / "Main.scene";
    if (!std::filesystem::exists(startupScene))
    {
        std::ofstream scene(startupScene);
        if (!scene)
        {
            Logger::Error("Failed to create startup scene: " + startupScene.string());
            return false;
        }

        scene << "MyEngineScene\n";
        scene << "Environment 1 4 1 1 1 320 1 0.003 0.32 2 80\n";
        scene << "Entities 0\n";
        scene << "HierarchyFolders 0\n";
    }

    std::filesystem::create_directories(root / "Saved", error);
    if (error)
    {
        Logger::Error("Failed to create project Saved directory: " + root.string());
        return false;
    }

    const std::filesystem::path descriptor = root / (name + ".project");
    std::ofstream out(descriptor);
    if (!out)
    {
        Logger::Error("Failed to create project descriptor: " + descriptor.string());
        return false;
    }

    out << "Version 1\n";
    out << "Name " << std::quoted(name) << "\n";
    out << "AssetDirectory " << std::quoted("Assets") << "\n";
    out << "StartupScene " << std::quoted("Assets/Scenes/Main.scene") << "\n";
    out << "Settings " << std::quoted("ProjectSettings.cfg") << "\n";
    out.close();

    // Keep generated/editor state out of source control by default.
    std::ofstream ignore(root / ".gitignore");
    if (ignore)
    {
        ignore << "Saved/\n";
        ignore << "*.user\n";
    }

    return Load(descriptor.string());
}


bool ProjectManager::Save()
{
    if (!m_HasProject || m_Project.descriptorPath.empty())
        return false;

    std::ofstream out(m_Project.descriptorPath, std::ios::trunc);
    if (!out)
    {
        Logger::Error("Failed to save project descriptor: " + m_Project.descriptorPath.string());
        return false;
    }

    out << "Version 1\n";
    out << "Name " << std::quoted(m_Project.name) << "\n";
    out << "AssetDirectory " << std::quoted(m_Project.assetDirectory.generic_string()) << "\n";
    out << "StartupScene " << std::quoted(m_Project.startupScene.generic_string()) << "\n";
    out << "Settings " << std::quoted(m_Project.settingsFile.generic_string()) << "\n";
    return static_cast<bool>(out);
}

bool ProjectManager::SetStartupScene(const std::string& scenePath)
{
    if (!m_HasProject || scenePath.empty())
        return false;

    std::filesystem::path input(scenePath);
    std::filesystem::path absolute = input.is_absolute()
        ? input.lexically_normal()
        : m_Project.Resolve(input);

    std::error_code error;
    const std::filesystem::path relative =
        std::filesystem::relative(absolute, m_Project.rootDirectory, error);

    if (error || relative.empty() || *relative.begin() == std::filesystem::path(".."))
    {
        Logger::Error("Startup scene must be inside the active project: " + absolute.string());
        return false;
    }

    if (!std::filesystem::exists(absolute) || absolute.extension() != ".scene")
    {
        Logger::Error("Invalid startup scene: " + absolute.string());
        return false;
    }

    m_Project.startupScene = relative.lexically_normal();
    return Save();
}

void ProjectManager::UseLegacyWorkspace()
{
    m_Project = {};
    m_Project.name = "Legacy Workspace";
    m_Project.rootDirectory = std::filesystem::current_path();
    m_Project.assetDirectory = "Assets";
    m_Project.settingsFile = "ProjectSettings.cfg";
    m_HasProject = false;
}

std::string ProjectManager::ResolveAssetPath(const std::string& path) const
{
    if (path.empty()) return path;
    std::filesystem::path input(path);
    if (input.is_absolute()) return input.lexically_normal().string();

    return m_Project.Resolve(input).string();
}

std::string ProjectManager::ResolveProjectPath(const std::string& path) const
{
    if (path.empty()) return path;
    std::filesystem::path input(path);
    if (input.is_absolute()) return input.lexically_normal().string();
    return (m_Project.rootDirectory / input).lexically_normal().string();
}
