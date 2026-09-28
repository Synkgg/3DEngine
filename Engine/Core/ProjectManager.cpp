#include "ProjectManager.h"
#include "Logger.h"
#include <fstream>
#include <iomanip>
#include <system_error>

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

    std::filesystem::path root = std::filesystem::absolute(directory).lexically_normal();
    std::error_code error;
    std::filesystem::create_directories(root / "Assets" / "Scenes", error);
    std::filesystem::create_directories(root / "Assets" / "Scripts", error);
    std::filesystem::create_directories(root / "Assets" / "UI", error);
    std::filesystem::create_directories(root / "Assets" / "Textures", error);
    std::filesystem::create_directories(root / "Assets" / "Models", error);
    if (error)
    {
        Logger::Error("Failed to create project directories: " + root.string());
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
    out << "StartupScene " << std::quoted("") << "\n";
    out << "Settings " << std::quoted("ProjectSettings.cfg") << "\n";
    out.close();

    return Load(descriptor.string());
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
