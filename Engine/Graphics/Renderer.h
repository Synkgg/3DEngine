#pragma once

#include <memory>
#include <array>

#include <glad/gl.h>
#include <SDL3/SDL.h>

#include "Shader.h"
#include "Mesh.h"

#include "../Graphics/Camera.h"
#include "Framebuffer.h"

#include "Grid.h"
#include "PrimitiveType.h"
#include "../Math/Transform.h"
#include "../Math/Vec3.h"
#include "DebugRenderer.h"
#include "TextureManager.h"
#include "../UI/UIRenderer.h"

class Window;
class Texture2D;

struct PointLightData { Vec3 position; Vec3 color; float intensity=1.0f; float range=10.0f; };
struct SpotLightData { Vec3 position; Vec3 direction; Vec3 color; float intensity=1.0f; float range=15.0f; float innerCos=0.92f; float outerCos=0.82f; };

class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool Initialize(Window& window);
    void Shutdown();

    void BeginFrame();
    void EndScene();
    void EndFrame();

    void SetClearColor(float red, float green, float blue, float alpha);

    void DrawMesh(
        const Transform& transform,
        PrimitiveType primitive,
        float red,
        float green,
        float blue,
        float alpha,
        const Texture2D* texture = nullptr,
        float metallic = 0.0f,
        float roughness = 0.65f,
        float ambientOcclusion = 1.0f,
        float emissive = 0.0f
    );

    SDL_GLContext GetContext() const;
    unsigned int GetViewportTexture() const;

    void ResizeViewport(unsigned int width, unsigned int height);

    Vec3 GetCameraPosition() const;
    void SetCameraPosition(const Vec3& position);

    float GetCameraYaw() const;
    float GetCameraPitch() const;
    Vec3 GetCameraForward() const;
    Vec3 GetCameraRight() const;
    void SetCameraRotation(float yaw, float pitch);
    void RotateCamera(float yawDelta, float pitchDelta);
    void MoveCamera( float forward, float right, float up, float deltaTime);
    void ResetCamera();

    void DrawGrid();
    void DrawSky();

    Mat4 GetCameraViewMatrix() const;
    Mat4 GetCameraProjectionMatrix() const;
    Vec3 GetCameraRayDirection(float ndcX, float ndcY) const;

    void SetDirectionalLight(const Vec3& direction, const Vec3& color, float intensity);
    void ClearLocalLights();
    void AddPointLight(const PointLightData& light);
    void AddSpotLight(const SpotLightData& light);
    void DrawDirectionalLight(const Vec3& position, const Vec3& direction);
    void DrawCollider(const Transform& transform, float width, float height, float depth);

    Texture2D* LoadTexture(
        const std::string& filepath
    );

    UIRenderer& GetUIRenderer()
    {
        return m_UIRenderer;
    }

private:
    SDL_GLContext m_Context;
    Window* m_Window;

    float m_ClearColor[4];

    Shader m_Shader;
    Shader m_GridShader;
    Shader m_SkyShader;
    unsigned int m_SkyVAO = 0;
    unsigned int m_SkyVBO = 0;

    std::unique_ptr<Mesh> m_CubeMesh;
    std::unique_ptr<Mesh> m_PlaneMesh;
    std::unique_ptr<Mesh> m_SphereMesh;
    std::unique_ptr<Mesh> m_CylinderMesh;

    Camera m_Camera;
    Framebuffer m_Framebuffer;

    unsigned int m_ViewportWidth;
    unsigned int m_ViewportHeight;

    Grid m_Grid;

    Vec3 m_LightDirection;
    Vec3 m_LightColor;
    float m_LightIntensity;
    std::array<PointLightData, 8> m_PointLights{};
    std::array<SpotLightData, 4> m_SpotLights{};
    int m_PointLightCount = 0;
    int m_SpotLightCount = 0;

    DebugRenderer m_DebugRenderer;

    TextureManager m_TextureManager;
    UIRenderer m_UIRenderer;
};