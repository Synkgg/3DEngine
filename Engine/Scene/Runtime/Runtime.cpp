#include "Runtime.h"

#include "../Scene.h"
#include "../SceneSerializer.h"
#include "../../Editor/HierarchyFolder.h"

#include "../Scripting/Scripts/RotatorScript.h"

#include "../../Graphics/Renderer.h"
#include "../../Platform/SDL/Input.h"
#include "../../UI/UICanvas.h"

#include "../../Core/Logger.h"

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

    m_LuaScriptSystem.Start(
        scene,
        input,
        renderer,
        uiCanvas,
        this
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

        if (m_Renderer && m_Input && m_UICanvas)
        {
            m_LuaScriptSystem.Start(
                scene,
                *m_Input,
                *m_Renderer,
                *m_UICanvas,
                this
            );

            m_ScriptSystem.Start(scene);
        }

        m_CurrentScenePath = nextScene;
        Logger::Info("Runtime scene switched to: " + nextScene);
        return;
    }

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
        m_LuaScriptSystem
    );

    m_CollisionSystem.Update(
        scene
    );
}

void Runtime::Stop(Scene& scene)
{
    if (!m_Running)
    {
        return;
    }

    m_LuaScriptSystem.Stop();
    m_ScriptSystem.Stop();

    if (m_HasSnapshot)
    {
        scene = m_Snapshot;
        m_HasSnapshot = false;
    }

    m_Running = false;

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
