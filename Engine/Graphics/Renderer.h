#pragma once

#include <memory>
#include <array>
#include <unordered_map>
#include <string>
#include <cstdint>

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
#include "Environment/EnvironmentSystem.h"
#include "../UI/UIRenderer.h"

class Window;
class Texture2D;

#include "Lighting/LightTypes.h"
#include "Rendering/RenderSettings.h"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool Initialize(Window& window);
    void Shutdown();

    static constexpr int ShadowCascadeCount = 3;
    void BeginShadowPass(int cascadeIndex = 0);
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
        float emissive = 0.0f,
        const Texture2D* normalMap = nullptr,
        const Texture2D* metallicMap = nullptr,
        const Texture2D* roughnessMap = nullptr,
        const Texture2D* aoMap = nullptr,
        const Texture2D* emissiveMap = nullptr
    );

    void DrawModel(
        const Transform& transform,
        const std::string& modelPath,
        float red, float green, float blue, float alpha,
        const Texture2D* texture = nullptr,
        float metallic = 0.0f, float roughness = 0.65f,
        float ambientOcclusion = 1.0f, float emissive = 0.0f,
        const Texture2D* normalMap = nullptr, const Texture2D* metallicMap = nullptr,
        const Texture2D* roughnessMap = nullptr, const Texture2D* aoMap = nullptr,
        const Texture2D* emissiveMap = nullptr
    );
    void DrawShadowModel(const Transform& transform, const std::string& modelPath);
    unsigned int RenderModelPreview(const std::string& modelPath, unsigned int width = 256, unsigned int height = 256);

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
    void SetCameraFov(float degrees) { m_Camera.SetFovDegrees(degrees); }
    float GetCameraFov() const { return m_Camera.GetFovDegrees(); }

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
    Shader m_TAAShader;
    unsigned int m_SkyVAO = 0;
    unsigned int m_SkyVBO = 0;
    unsigned int m_PostVAO = 0;
    unsigned int m_PostVBO = 0;
    std::array<unsigned int, ShadowCascadeCount> m_ShadowFramebuffers{};
    std::array<unsigned int, ShadowCascadeCount> m_ShadowDepthTextures{};
    std::array<unsigned int, ShadowCascadeCount> m_ShadowMapSizes{ 2048u, 2048u, 1024u };
    std::array<Mat4, ShadowCascadeCount> m_LightSpaceMatrices{ Mat4::Identity(), Mat4::Identity(), Mat4::Identity() };
    std::array<float, ShadowCascadeCount> m_ShadowCascadeSplits{ 12.0f, 32.0f, 80.0f };
    int m_ActiveShadowCascade = 0;
    bool m_ShadowMapReady = false;

    unsigned int m_PostFramebuffer = 0;
    unsigned int m_PostColorTexture = 0;
    unsigned int m_HistoryFramebuffer = 0;
    unsigned int m_HistoryTexture = 0;
    bool m_HistoryValid = false;
    Vec3 m_PreviousCameraPosition{};
    Vec3 m_PreviousCameraForward{};
    Vec3 m_PreviousCameraRight{};
    Vec3 m_PreviousCameraUp{};
    float m_PreviousTanHalfFov = 0.0f;
    float m_PreviousAspect = 1.0f;
    unsigned int m_BloomFramebuffer[2]{ 0, 0 };
    unsigned int m_BloomTexture[2]{ 0, 0 };
    unsigned int m_ModelPreviewFramebuffer = 0;
    unsigned int m_ModelPreviewTexture = 0;
    unsigned int m_ModelPreviewDepth = 0;
    unsigned int m_ModelPreviewWidth = 0;
    unsigned int m_ModelPreviewHeight = 0;
    Shader m_ModelPreviewShader;

    struct ModelPreviewTexture
    {
        unsigned int framebuffer = 0;
        unsigned int texture = 0;
        unsigned int depth = 0;
        unsigned int width = 0;
        unsigned int height = 0;
    };
    std::unordered_map<std::string, ModelPreviewTexture> m_ModelPreviewCache;

    std::unique_ptr<Mesh> m_CubeMesh;
    std::unique_ptr<Mesh> m_PlaneMesh;
    std::unique_ptr<Mesh> m_SphereMesh;
    std::unique_ptr<Mesh> m_CylinderMesh;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> m_ModelCache;

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
    EnvironmentSystem m_EnvironmentSystem;
    UIRenderer m_UIRenderer;

    Mesh* GetPrimitiveMesh(PrimitiveType primitive);
    Mesh* GetModelMesh(const std::string& modelPath);
    void DrawMeshInternal(Mesh* mesh, const Transform& transform, float red, float green, float blue, float alpha, const Texture2D* texture, float metallic, float roughness, float ambientOcclusion, float emissive, const Texture2D* normalMap, const Texture2D* metallicMap, const Texture2D* roughnessMap, const Texture2D* aoMap, const Texture2D* emissiveMap);

    bool CreateShadowTarget();
    void DestroyShadowTarget();
    void UpdateLightSpaceMatrices();

    bool CreatePostProcessTarget();
    void DestroyPostProcessTarget();
    void RenderPostProcess();
    unsigned int RenderBloom();
    void ResolveTAA();
    bool EnsureModelPreviewTarget(unsigned int width, unsigned int height);
    void DestroyModelPreviewTarget();
    void DestroyModelPreviewCache();
};