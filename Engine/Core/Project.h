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

    std::filesystem::path GetAssetRoot() const { return (rootDirectory / assetDirectory).lexically_normal(); }
    std::filesystem::path GetSettingsPath() const { return (rootDirectory / settingsFile).lexically_normal(); }
    std::filesystem::path GetStartupScenePath() const { return startupScene.empty() ? std::filesystem::path{} : (rootDirectory / startupScene).lexically_normal(); }
    std::filesystem::path Resolve(const std::filesystem::path& path) const
    {
        if (path.empty() || path.is_absolute()) return path.lexically_normal();
        const auto generic = path.generic_string();
        if (generic == "Assets" || generic.rfind("Assets/", 0) == 0)
        {
            const std::filesystem::path relative = generic == "Assets" ? std::filesystem::path{} : std::filesystem::path(generic.substr(7));
            return (GetAssetRoot() / relative).lexically_normal();
        }
        return (rootDirectory / path).lexically_normal();
    }
};
