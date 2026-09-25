#include "PrefabSerializer.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "../Editor/HierarchyFolder.h"
#include "../Core/Logger.h"

#include <vector>

bool PrefabSerializer::Save(Scene& scene, Entity root, const std::string& filepath)
{
    if (!root.IsValid()) return false;

    Scene prefabScene;
    Entity prefabRoot = scene.CloneEntityTo(root, prefabScene, true);
    if (!prefabRoot.IsValid()) return false;

    std::vector<HierarchyFolder> folders;
    SceneSerializer serializer(prefabScene);
    if (!serializer.Save(filepath, folders))
    {
        Logger::Error("Failed to save prefab: " + filepath);
        return false;
    }

    Logger::Info("Saved prefab: " + filepath);
    return true;
}

Entity PrefabSerializer::Instantiate(Scene& scene, const std::string& filepath, Entity parent)
{
    Scene prefabScene;
    std::vector<HierarchyFolder> folders;
    SceneSerializer serializer(prefabScene);
    if (!serializer.Load(filepath, folders))
    {
        Logger::Error("Failed to load prefab: " + filepath);
        return Entity();
    }

    const std::vector<Entity> roots = prefabScene.GetRootEntities();
    if (roots.empty())
    {
        Logger::Error("Prefab has no root entity: " + filepath);
        return Entity();
    }

    Entity instance = prefabScene.CloneEntityTo(roots.front(), scene, true);
    if (instance.IsValid() && parent.IsValid())
        scene.SetParent(instance, parent, false);

    return instance;
}
