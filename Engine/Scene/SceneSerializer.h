#pragma once

#include <string>
#include <vector>

struct HierarchyFolder;

class Scene;

class SceneSerializer
{
public:
    explicit SceneSerializer(
        Scene& scene
    );

    bool Save(
        const std::string& filepath,
        const std::vector<HierarchyFolder>& hierarchyFolders
    );

    bool Load(
        const std::string& filepath,
        std::vector<HierarchyFolder>& hierarchyFolders
    );

private:
    Scene& m_Scene;
};