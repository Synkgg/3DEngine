#pragma once



#include "../Systems/PlayerSystem.h"
#include "../Systems/CharacterControllerSystem.h"
#include "../Systems/CollisionSystem.h"
#include "../Systems/ScriptSystem.h"
#include "../Systems/LuaScriptSystem.h"
#include "../Systems/InteractionSystem.h"

#include "../../Scene/Scene.h"

#include "../../Math/Vec3.h"
#include "../../Network/NetworkManager.h"

#include <string>
#include <unordered_map>

class Renderer;
class Input;
class UICanvas;
class AudioEngine;
class ProjectSettings;

class Runtime
{
public:
    void Start(
        Scene& scene,
        Renderer& renderer,
        Input& input,
        UICanvas& uiCanvas
    );

    void Update(
        Scene& scene,
        Renderer& renderer,
        Input& input,
        float deltaTime
    );

    void Stop(Scene& scene);

    bool IsRunning() const;

    void SaveCameraState(const Renderer& renderer);
    void RestoreCameraState(Renderer& renderer);

    const std::string& GetInteractionPrompt() const;

    bool RequestSceneLoad(const std::string& path);
    const std::string& GetCurrentScenePath() const;
    bool WantsCursor() const;
    void SetWantsCursor(bool wantsCursor);
    bool IsPaused() const;
    void SetPaused(bool paused);
    void SetAudioEngine(AudioEngine* audio) { m_Audio = audio; }
    AudioEngine* GetAudioEngine() const { return m_Audio; }
    void SetProjectSettings(ProjectSettings* settings) { m_ProjectSettings = settings; }
    NetworkManager& GetNetwork() { return m_Network; }
    bool IsLocalPlayerEntityOrChild(const Scene& scene, Entity entity) const;
    void SetMouseSensitivity(float value) { m_MouseSensitivity = value; }
    float GetMouseSensitivity() const { return m_MouseSensitivity; }
    void SetInvertY(bool value) { m_InvertY = value; }
    bool GetInvertY() const { return m_InvertY; }
    void SetSprintToggle(bool value) { m_SprintToggle = value; }
    bool GetSprintToggle() const { return m_SprintToggle; }
    void SetCameraBob(bool value) { m_CameraBob = value; }
    bool GetCameraBob() const { return m_CameraBob; }
    void SetShowFPS(bool value) { m_ShowFPS = value; }
    bool GetShowFPS() const { return m_ShowFPS; }

private:
    bool m_Running = false;

    PlayerSystem m_PlayerSystem;
    CharacterControllerSystem m_CharacterControllerSystem;
    CollisionSystem m_CollisionSystem;
    ScriptSystem m_ScriptSystem;
    LuaScriptSystem m_LuaScriptSystem;
    InteractionSystem m_InteractionSystem;

    Scene m_Snapshot;
    bool m_HasSnapshot = false;

    Vec3 m_SnapshotCameraPosition{};
    float m_SnapshotCameraYaw = 0.0f;
    float m_SnapshotCameraPitch = 0.0f;

    std::string m_PendingScenePath;
    std::string m_CurrentScenePath;
    bool m_WantsCursor = false;
    bool m_Paused = false;
    Renderer* m_Renderer = nullptr;
    Input* m_Input = nullptr;
    UICanvas* m_UICanvas = nullptr;
    AudioEngine* m_Audio = nullptr;
    ProjectSettings* m_ProjectSettings = nullptr;
    NetworkManager m_Network;
    std::unordered_map<std::uint32_t, std::uint32_t> m_RemotePlayerEntities;
    float m_NetworkTransformSendTimer = 0.0f;
    float m_MouseSensitivity = 0.01f;
    bool m_InvertY = false;
    bool m_SprintToggle = false;
    bool m_CameraBob = true;
    bool m_ShowFPS = false;
    void UpdateNetworkPlayers(Scene& scene, float deltaTime);
};