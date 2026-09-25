#pragma once



#include "../Systems/PlayerSystem.h"
#include "../Systems/CharacterControllerSystem.h"
#include "../Systems/CollisionSystem.h"
#include "../Systems/ScriptSystem.h"
#include "../Systems/LuaScriptSystem.h"
#include "../Systems/InteractionSystem.h"

#include "../../Scene/Scene.h"

#include "../../Math/Vec3.h"

#include <string>

class Renderer;
class Input;
class UICanvas;
class AudioEngine;

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
};