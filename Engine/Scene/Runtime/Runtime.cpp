#include "Runtime.h"

#include "../Scene.h"
#include "../SceneSerializer.h"
#include "../../Editor/HierarchyFolder.h"

#include "../Scripting/Scripts/RotatorScript.h"

#include "../../Graphics/Renderer.h"
#include "../../Platform/SDL/Input.h"
#include "../../UI/UICanvas.h"

#include "../../Core/Logger.h"
#include "../../Core/ProjectManager.h"
#include "../Components/NameComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/PawnComponent.h"

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
        m_ProjectSettings,
        m_ProjectManager
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

        const std::string resolvedScene = ResolveProjectPath(nextScene);
        if (!serializer.Load(resolvedScene, ignoredFolders))
        {
            Logger::Error("Failed to switch runtime scene: " + nextScene);
            return;
        }

        m_LuaScriptSystem.Stop();
        m_ScriptSystem.Stop();

        if (m_UICanvas)
            m_UICanvas->Clear();

        scene = loadedScene;

        if (m_Renderer && m_Input && m_UICanvas)
        {
            m_LuaScriptSystem.Start(
                scene,
                *m_Input,
                *m_Renderer,
                *m_UICanvas,
                this,
                m_ProjectSettings,
                m_ProjectManager
            );

            m_ScriptSystem.Start(scene);
        }

        m_CurrentScenePath = resolvedScene;
        Logger::Info("Runtime scene switched to: " + resolvedScene);
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
    m_StateNumbers.clear();
    m_StateBools.clear();
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


std::uint32_t Runtime::GetLocalControllerID() const
{
    if (!m_Network.IsConnected()) return 1u;
    return m_Network.GetLocalPlayerID();
}

bool Runtime::PossessPawn(Scene& scene, Entity entity, std::uint32_t controllerID)
{
    PawnComponent* pawn = scene.GetComponent<PawnComponent>(entity);
    if (!pawn || controllerID == 0) return false;
    for (const Entity& candidate : scene.GetEntities())
    {
        PawnComponent* other = scene.GetComponent<PawnComponent>(candidate);
        if (other && other->controllerID == controllerID) other->controllerID = 0;
    }
    pawn->controllerID = controllerID;
    return true;
}

void Runtime::UnpossessPawn(Scene& scene, Entity entity)
{
    if (PawnComponent* pawn = scene.GetComponent<PawnComponent>(entity)) pawn->controllerID = 0;
}

bool Runtime::IsPawnLocallyControlled(const Scene& scene, Entity entity) const
{
    const PawnComponent* pawn = scene.GetComponent<PawnComponent>(entity);
    if (!pawn) return false;
    const std::uint32_t localController = GetLocalControllerID();
    return localController != 0 && pawn->controllerID == localController;
}

bool Runtime::IsLocalPlayerEntityOrChild(const Scene& scene, Entity entity) const
{
    if (!m_Running || !entity.IsValid()) return false;
    Entity current = entity;
    while (current.IsValid())
    {
        if (IsPawnLocallyControlled(scene, current))
            return true;
        current = scene.GetParent(current);
    }
    return false;
}

std::string Runtime::ResolveProjectPath(const std::string& path) const
{
    return m_ProjectManager ? m_ProjectManager->ResolveAssetPath(path) : path;
}
