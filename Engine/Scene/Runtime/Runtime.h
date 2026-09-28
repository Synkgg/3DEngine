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
class ProjectManager;

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
    void SetProjectManager(ProjectManager* projects) { m_ProjectManager = projects; }
    std::string ResolveProjectPath(const std::string& path) const;
    NetworkManager& GetNetwork() { return m_Network; }
    bool IsLocalPlayerEntityOrChild(const Scene& scene, Entity entity) const;
    void SetStateNumber(const std::string& key, double value) { m_StateNumbers[key] = value; }
    double GetStateNumber(const std::string& key, double fallback = 0.0) const
    {
        const auto it = m_StateNumbers.find(key);
        return it != m_StateNumbers.end() ? it->second : fallback;
    }
    void SetStateBool(const std::string& key, bool value) { m_StateBools[key] = value; }
    bool GetStateBool(const std::string& key, bool fallback = false) const
    {
        const auto it = m_StateBools.find(key);
        return it != m_StateBools.end() ? it->second : fallback;
    }

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
    ProjectManager* m_ProjectManager = nullptr;
    NetworkManager m_Network;
    std::unordered_map<std::string, double> m_StateNumbers;
    std::unordered_map<std::string, bool> m_StateBools;
};