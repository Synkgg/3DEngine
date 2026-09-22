#include "Runtime.h"

#include "../Scene.h"

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

    m_LuaScriptSystem.Start(
        scene,
        input,
        renderer,
        uiCanvas
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