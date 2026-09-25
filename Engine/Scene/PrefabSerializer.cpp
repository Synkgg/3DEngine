#include "PrefabSerializer.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "Components/TransformComponent.h"
#include "../Editor/HierarchyFolder.h"
#include "../Core/Logger.h"

#include <vector>

std::unordered_map<const Scene*, std::unordered_map<std::uint32_t, PrefabSerializer::InstanceInfo>> PrefabSerializer::s_Instances;

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
    s_Instances[&scene][instance.GetID()] = { filepath };
    Logger::Info("Instantiated prefab: " + filepath);
    return instance;
}

bool PrefabSerializer::IsInstanceRoot(const Scene& scene, Entity entity)
{
    auto sceneIt=s_Instances.find(&scene);
    return entity.IsValid() && sceneIt!=s_Instances.end() && sceneIt->second.find(entity.GetID())!=sceneIt->second.end();
}

std::string PrefabSerializer::GetSource(const Scene& scene, Entity entity)
{
    auto sceneIt=s_Instances.find(&scene);
    if(sceneIt==s_Instances.end()) return {};
    auto it=sceneIt->second.find(entity.GetID());
    return it==sceneIt->second.end()?std::string():it->second.source;
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
    scene.DestroyEntityHierarchy(instanceRoot);
    s_Instances[&scene].erase(instanceRoot.GetID());
    Entity replacement=Instantiate(scene,source,parent);
    if(!replacement.IsValid()) return false;
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
    return true;
}

bool PrefabSerializer::Unpack(Scene& scene, Entity instanceRoot, bool)
{
    auto sceneIt=s_Instances.find(&scene);
    if(sceneIt==s_Instances.end()) return false;
    return sceneIt->second.erase(instanceRoot.GetID())>0;
}

void PrefabSerializer::ForgetEntity(const Scene& scene, Entity entity)
{
    auto it=s_Instances.find(&scene);
    if(it!=s_Instances.end()) it->second.erase(entity.GetID());
}

void PrefabSerializer::ForgetScene(const Scene& scene)
{
    s_Instances.erase(&scene);
}
