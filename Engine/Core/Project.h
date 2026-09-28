#pragma once
#include <filesystem>
#include <string>

struct Project
{
    std::string name = "Untitled";
    std::filesystem::path descriptorPath;
    std::filesystem::path rootDirectory;
    std::filesystem::path assetDirectory = "Assets";
    std::filesystem::path startupScene;
    std::filesystem::path settingsFile = "ProjectSettings.cfg";

    std::filesystem::path GetAssetRoot() const { return rootDirectory / assetDirectory; }
    std::filesystem::path GetSettingsPath() const { return rootDirectory / settingsFile; }
    std::filesystem::path GetStartupScenePath() const { return startupScene.empty() ? std::filesystem::path{} : rootDirectory / startupScene; }
};
