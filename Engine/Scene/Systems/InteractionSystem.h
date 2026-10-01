#pragma once

#include <string>

struct Vec3;

class Scene;
class Renderer;
class Input;
class LuaScriptSystem;
class AudioEngine;

class InteractionSystem
{
public:
    void Update(
        Scene& scene,
        Renderer& renderer,
        Input& input,
        LuaScriptSystem& luaScriptSystem,
        AudioEngine* audio = nullptr
    );

    const std::string& GetPrompt() const
    {
        return m_CurrentPrompt;
    }

private:
    bool RayIntersectsAABB(
        const Vec3& rayOrigin,
        const Vec3& rayDirection,
        const Vec3& boxCenter,
        const Vec3& boxHalfExtents,
        float& distance
    ) const;

    std::string m_CurrentPrompt;
};