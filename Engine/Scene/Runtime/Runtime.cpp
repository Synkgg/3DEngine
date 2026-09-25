#include "Runtime.h"

#include "../Scene.h"
#include "../SceneSerializer.h"
#include "../../Editor/HierarchyFolder.h"

#include "../Scripting/Scripts/RotatorScript.h"

#include "../../Graphics/Renderer.h"
#include "../../Platform/SDL/Input.h"
#include "../../UI/UICanvas.h"

#include "../../Core/Logger.h"
#include "../Components/NameComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/CharacterControllerComponent.h"
#include "../Components/ColliderComponent.h"
#include "../Components/ScriptComponent.h"

#include <memory>


void Runtime::Start(
    Scene& scene,
    Renderer& renderer,
    Input& input,
    UICanvas& uiCanvas
)
{
    if (m_Running)
    {
        return;
    }

    m_Snapshot = scene;
    m_HasSnapshot = true;

    m_Running = true;
    m_Renderer = &renderer;
    m_Input = &input;
    m_UICanvas = &uiCanvas;
    m_PendingScenePath.clear();
    m_WantsCursor = false;
    m_Paused = false;

    m_LuaScriptSystem.Start(
        scene,
        input,
        renderer,
        uiCanvas,
        this,
        m_ProjectSettings
    );

    m_ScriptSystem.Register(
        "RotatorScript",
        []()
        {
            return std::make_unique<RotatorScript>();
        }
    );

    m_ScriptSystem.Start(
        scene
    );

    Logger::Info(
        "Runtime started."
    );
}

void Runtime::Update(
    Scene& scene,
    Renderer& renderer,
    Input& input,
    float deltaTime)
{
    if (!m_Running)
    {
        return;
    }


    m_Network.Update();
    UpdateNetworkPlayers(scene);

    m_LuaScriptSystem.Update(
        scene,
        deltaTime
    );

    if (!m_PendingScenePath.empty())
    {
        const std::string nextScene = m_PendingScenePath;
        m_PendingScenePath.clear();

        // Validate the next scene in isolation first. A bad path or malformed
        // scene must never destroy the currently running world.
        Scene loadedScene;
        std::vector<HierarchyFolder> ignoredFolders;
        SceneSerializer serializer(loadedScene);

        if (!serializer.Load(nextScene, ignoredFolders))
        {
            Logger::Error("Failed to switch runtime scene: " + nextScene);
            return;
        }

        m_LuaScriptSystem.Stop();
        m_ScriptSystem.Stop();

        if (m_UICanvas)
            m_UICanvas->Clear();

        scene = loadedScene;
        m_RemotePlayerEntities.clear();

        if (m_Renderer && m_Input && m_UICanvas)
        {
            m_LuaScriptSystem.Start(
                scene,
                *m_Input,
                *m_Renderer,
                *m_UICanvas,
                this,
                m_ProjectSettings
            );

            m_ScriptSystem.Start(scene);
        }

        m_CurrentScenePath = nextScene;
        Logger::Info("Runtime scene switched to: " + nextScene);
        return;
    }

    if (!m_Paused)
    {
        m_CharacterControllerSystem.Update(
            scene,
            renderer,
            input,
            deltaTime
        );

        m_InteractionSystem.Update(
            scene,
            renderer,
            input,
            m_LuaScriptSystem,
            m_Audio
        );

        m_CollisionSystem.Update(
            scene
        );
    }
}

void Runtime::Stop(Scene& scene)
{
    if (!m_Running)
    {
        return;
    }

    m_LuaScriptSystem.Stop();
    m_ScriptSystem.Stop();
    m_Network.Disconnect();
    m_RemotePlayerEntities.clear();

    if (m_HasSnapshot)
    {
        scene = m_Snapshot;
        m_HasSnapshot = false;
    }

    m_Running = false;
    m_Paused = false;
    m_WantsCursor = false;
    m_PendingScenePath.clear();
    m_CurrentScenePath.clear();
    m_Renderer = nullptr;
    m_Input = nullptr;
    m_UICanvas = nullptr;

    Logger::Info(
        "Runtime stopped."
    );
}

bool Runtime::IsRunning() const
{
    return m_Running;
}

void Runtime::SaveCameraState(
    const Renderer& renderer)
{
    m_SnapshotCameraPosition =
        renderer.GetCameraPosition();

    m_SnapshotCameraYaw =
        renderer.GetCameraYaw();

    m_SnapshotCameraPitch =
        renderer.GetCameraPitch();
}

void Runtime::RestoreCameraState(
    Renderer& renderer)
{
    renderer.SetCameraPosition(
        m_SnapshotCameraPosition
    );

    renderer.SetCameraRotation(
        m_SnapshotCameraYaw,
        m_SnapshotCameraPitch
    );
}

const std::string& Runtime::GetInteractionPrompt() const
{
    return m_InteractionSystem.GetPrompt();
}

bool Runtime::RequestSceneLoad(const std::string& path)
{
    if (!m_Running || path.empty())
        return false;

    m_PendingScenePath = path;
    return true;
}


const std::string& Runtime::GetCurrentScenePath() const
{
    return m_CurrentScenePath;
}

bool Runtime::WantsCursor() const
{
    return m_WantsCursor;
}

void Runtime::SetWantsCursor(bool wantsCursor)
{
    m_WantsCursor = wantsCursor;
}


bool Runtime::IsPaused() const
{
    return m_Paused;
}

void Runtime::SetPaused(bool paused)
{
    m_Paused = paused;
}


bool Runtime::IsLocalPlayerEntityOrChild(const Scene& scene, Entity entity) const
{
    if (!m_Running || !entity.IsValid()) return false;
    Entity current=entity;
    while(current.IsValid())
    {
        const NameComponent* name=scene.GetComponent<NameComponent>(current);
        if(name && name->name=="Player") return true;
        current=scene.GetParent(current);
    }
    return false;
}

void Runtime::UpdateNetworkPlayers(Scene& scene)
{
    if (!m_Network.IsConnected() || !m_Network.IsHandshakeComplete()) return;

    Entity localPlayer=scene.FindEntityByName("Player");
    TransformComponent* localTransform=localPlayer.IsValid()
        ? scene.GetComponent<TransformComponent>(localPlayer) : nullptr;

    if (localTransform)
    {
        NetworkTransformState state;
        state.x=localTransform->transform.position.x;
        state.y=localTransform->transform.position.y;
        state.z=localTransform->transform.position.z;
        state.rx=localTransform->transform.rotation.x;
        state.ry=m_Renderer ? m_Renderer->GetCameraYaw() : localTransform->transform.rotation.y;
        state.rz=localTransform->transform.rotation.z;
        m_Network.SendLocalTransform(state);
    }

    for (const auto& [playerID,state] : m_Network.GetRemoteTransforms())
    {
        Entity remote;
        auto existing=m_RemotePlayerEntities.find(playerID);
        if(existing!=m_RemotePlayerEntities.end()) remote=scene.FindEntityByID(existing->second);

        if(!remote.IsValid() && localPlayer.IsValid())
        {
            remote=scene.DuplicateEntity(localPlayer,true);
            if(!remote.IsValid()) continue;
            if(auto* name=scene.GetComponent<NameComponent>(remote))
                name->name="RemotePlayer_"+std::to_string(playerID);

            std::vector<Entity> stack{remote};
            while(!stack.empty())
            {
                Entity e=stack.back();stack.pop_back();
                scene.RemoveComponent<CharacterControllerComponent>(e);
                scene.RemoveComponent<ColliderComponent>(e);
                scene.RemoveComponent<ScriptComponent>(e);
                for(Entity child:scene.GetChildren(e)) stack.push_back(child);
            }
            m_RemotePlayerEntities[playerID]=remote.GetID();
            Logger::Info("Network: spawned remote player "+std::to_string(playerID)+".");
        }

        if(auto* transform=scene.GetComponent<TransformComponent>(remote))
        {
            // A small frame-rate-independent blend keeps UDP movement readable
            // without introducing a separate snapshot buffer yet.
            constexpr float blend=0.35f;
            transform->transform.position.x += (state.x-transform->transform.position.x)*blend;
            transform->transform.position.y += (state.y-transform->transform.position.y)*blend;
            transform->transform.position.z += (state.z-transform->transform.position.z)*blend;
            transform->transform.rotation.x=state.rx;
            transform->transform.rotation.y=state.ry;
            transform->transform.rotation.z=state.rz;
        }
    }
}
