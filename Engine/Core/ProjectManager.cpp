#include "ProjectManager.h"
#include "Logger.h"
#include <fstream>
#include <iomanip>

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

    m_Project = std::move(project);
    m_HasProject = true;
    Logger::Info("Opened project: " + m_Project.name + " (" + m_Project.rootDirectory.string() + ")");
    return true;
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

    // Existing scenes store paths beginning with Assets/. Keep them valid while
    // projects migrate, but resolve them relative to the active project root.
    return (m_Project.rootDirectory / input).lexically_normal().string();
}
