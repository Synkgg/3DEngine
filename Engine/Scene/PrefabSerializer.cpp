#include "PrefabSerializer.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "Components/TransformComponent.h"
#include "../Editor/HierarchyFolder.h"
#include "../Core/Logger.h"

#include <vector>

bool PrefabSerializer::Save(Scene& scene, Entity root, const std::string& filepath)
{
    if (!root.IsValid() || filepath.empty()) return false;
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
    if (!instance.IsValid()) return Entity();
    if (parent.IsValid()) scene.SetParent(instance, parent, false);
    scene.SetPrefabSource(instance, filepath);
    Logger::Info("Instantiated prefab: " + filepath);
    return instance;
}

bool PrefabSerializer::IsInstanceRoot(const Scene& scene, Entity entity)
{
    return entity.IsValid() && !scene.GetPrefabSource(entity).empty();
}

std::string PrefabSerializer::GetSource(const Scene& scene, Entity entity)
{
    return scene.GetPrefabSource(entity);
}

bool PrefabSerializer::Apply(Scene& scene, Entity instanceRoot)
{
    const std::string source=GetSource(scene,instanceRoot);
    if(source.empty()) return false;
    return Save(scene,instanceRoot,source);
}

bool PrefabSerializer::Revert(Scene& scene, Entity instanceRoot)
{
    const std::string source=GetSource(scene,instanceRoot);
    if(source.empty()) return false;
    Entity parent=scene.GetParent(instanceRoot);
    const Transform world=scene.GetWorldTransform(instanceRoot);

    // Load first so a malformed/deleted prefab never destroys the live instance.
    Scene prefabScene;
    std::vector<HierarchyFolder> folders;
    SceneSerializer serializer(prefabScene);
    if(!serializer.Load(source,folders)) return false;
    const std::vector<Entity> roots=prefabScene.GetRootEntities();
    if(roots.empty()) return false;

    Entity replacement=prefabScene.CloneEntityTo(roots.front(),scene,true);
    if(!replacement.IsValid()) return false;
    if(parent.IsValid()) scene.SetParent(replacement,parent,false);
    if(TransformComponent* transform=scene.GetComponent<TransformComponent>(replacement))
    {
        if(parent.IsValid())
        {
            transform->transform=world;
            scene.ClearParent(replacement,false);
            scene.SetParent(replacement,parent,true);
        }
        else transform->transform=world;
    }

    scene.DestroyEntityHierarchy(instanceRoot);
    scene.ClearPrefabSource(instanceRoot);
    scene.SetPrefabSource(replacement,source);
    return true;
}

bool PrefabSerializer::Unpack(Scene& scene, Entity instanceRoot, bool)
{
    if(scene.GetPrefabSource(instanceRoot).empty()) return false;
    scene.ClearPrefabSource(instanceRoot);
    return true;
}

void PrefabSerializer::ForgetEntity(const Scene& scene, Entity entity)
{
    const_cast<Scene&>(scene).ClearPrefabSource(entity);
}

void PrefabSerializer::ForgetScene(const Scene& scene)
{
    // Prefab metadata is owned by Scene and cleared with scene data.
}
