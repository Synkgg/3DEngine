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
struct RenderSettings
{
    bool antiAliasing = true;
    int antiAliasingSamples = 4;
    bool shadows = true;
    // Optional post effects stay off until they have dedicated post-process passes.
    bool fog = false;
    bool bloom = true;
    float viewDistance = 1000.0f;
    float exposure = 1.0f;
    float fogDensity = 0.003f;
    float bloomStrength = 0.32f;
    int shadowQuality = 2;
    float shadowDistance = 80.0f;
};

struct SpotLightData { Vec3 position; Vec3 direction; Vec3 color; float intensity=1.0f; float range=15.0f; float innerCos=0.92f; float outerCos=0.82f; };

class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool Initialize(Window& window);
    void Shutdown();

    void BeginShadowPass();
    void DrawShadowMesh(const Transform& transform, PrimitiveType primitive);
    void EndShadowPass();

    void BeginFrame();
    void EndScene();
    void BeginOverlay();
    void EndOverlay();
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
    void SetRenderSettings(const RenderSettings& settings);
    const RenderSettings& GetRenderSettings() const;
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
    Shader m_ShadowShader;
    Shader m_SkyShader;
    Shader m_PostShader;
    Shader m_BloomExtractShader;
    Shader m_BloomBlurShader;
    unsigned int m_SkyVAO = 0;
    unsigned int m_SkyVBO = 0;
    unsigned int m_PostVAO = 0;
    unsigned int m_PostVBO = 0;
    unsigned int m_ShadowFramebuffer = 0;
    unsigned int m_ShadowDepthTexture = 0;
    unsigned int m_ShadowMapSize = 2048;
    Mat4 m_LightSpaceMatrix = Mat4::Identity();
    bool m_ShadowMapReady = false;

    unsigned int m_PostFramebuffer = 0;
    unsigned int m_PostColorTexture = 0;
    unsigned int m_BloomFramebuffer[2]{ 0, 0 };
    unsigned int m_BloomTexture[2]{ 0, 0 };

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
    RenderSettings m_RenderSettings{};

    DebugRenderer m_DebugRenderer;

    TextureManager m_TextureManager;
    UIRenderer m_UIRenderer;

    bool CreateShadowTarget();
    void DestroyShadowTarget();
    void UpdateLightSpaceMatrix();

    bool CreatePostProcessTarget();
    void DestroyPostProcessTarget();
    void RenderPostProcess();
    unsigned int RenderBloom();
};