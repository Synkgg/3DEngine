#pragma once

#include <memory>
#include <array>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "Mesh.h"
#include "ModelAsset.h"

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

enum class RenderDebugView
{
    Lit = 0,
    Normals,
    Roughness,
    Depth,
    AmbientOcclusion,
    Reflections
};

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
    void DrawAnimatedModel(
        const Transform& transform, const std::string& modelPath,
        std::size_t clipIndex, float animationTime, bool loop = true,
        float red = 1.0f, float green = 1.0f, float blue = 1.0f, float alpha = 1.0f
    );
    void DrawShadowModel(const Transform& transform, const std::string& modelPath);
    void DrawAnimatedShadowModel(const Transform& transform, const std::string& modelPath, std::size_t clipIndex, float animationTime, bool loop = true);
    unsigned int RenderModelPreview(const std::string& modelPath, unsigned int width = 256, unsigned int height = 256);
    unsigned int RenderAnimatedModelPreview(const std::string& modelPath, std::size_t clipIndex, float animationTime, unsigned int width = 256, unsigned int height = 256);
    ModelAsset* GetModelAsset(const std::string& modelPath);
    void InvalidateModelAsset(const std::string& modelPath);

    std::uint64_t GetViewportTexture() const;
    Velcryn::RHI::TextureHandle GetSceneColorTexture() const { return m_Framebuffer.GetColorTexture(); }
    unsigned int GetViewportWidth() const { return m_ViewportWidth; }
    unsigned int GetViewportHeight() const { return m_ViewportHeight; }

    void ResizeViewport(unsigned int width, unsigned int height);

    Vec3 GetCameraPosition() const;
    void SetCameraPosition(const Vec3& position);

    float GetCameraYaw() const;
    float GetCameraPitch() const;
    Vec3 GetCameraForward() const;
    Vec3 GetCameraRight() const;
    void SetCameraRotation(float yaw, float pitch);
    void RotateCamera(float yawDelta, float pitchDelta);
    void SetCameraMoveSpeed(float speed);
    void MoveCamera(float forward, float right, float up, float deltaTime);
    void ResetCamera();
    void SetCameraFov(float degrees) { m_Camera.SetFovDegrees(degrees); }
    float GetCameraFov() const { return m_Camera.GetFovDegrees(); }
    void SetCameraNearPlane(float value) { m_Camera.SetNearPlane(value); }
    void SetCameraFarPlane(float value) { m_Camera.SetFarPlane(value); }

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
    void SetDebugView(RenderDebugView view) { m_DebugView = view; m_HistoryValid = false; }
    RenderDebugView GetDebugView() const { return m_DebugView; }
    void InvalidateTemporalHistory() { m_HistoryValid = false; }
    void DrawDirectionalLight(const Vec3& position, const Vec3& direction);
    void DrawCollider(const Transform& transform, float width, float height, float depth);
    void AddDebugLine(const Vec3& start, const Vec3& end, const Vec3& color = Vec3(1.0f, 0.2f, 0.2f), float duration = 0.0f);
    void DrawDebugLines(float deltaTime);

    Texture2D* LoadTexture(
        const std::string& filepath
    );

    void SetProjectRoot(const std::filesystem::path& root) { m_ProjectRoot = root.lexically_normal(); }
    std::string ResolveAssetPath(const std::string& path) const;

    UIRenderer& GetUIRenderer()
    {
        return m_UIRenderer;
    }

private:
    Velcryn::RHI::PipelineHandle m_MeshPipeline{};
    Velcryn::RHI::TextureHandle m_WhiteTexture{};
    Window* m_Window;

    float m_ClearColor[4];

    std::array<Velcryn::RHI::TextureHandle, ShadowCascadeCount> m_ShadowDepthTextures{};
    std::array<unsigned int, ShadowCascadeCount> m_ShadowMapSizes{ 2048u, 2048u, 1024u };
    std::array<Mat4, ShadowCascadeCount> m_LightSpaceMatrices{ Mat4::Identity(), Mat4::Identity(), Mat4::Identity() };
    std::array<float, ShadowCascadeCount> m_ShadowCascadeSplits{ 12.0f, 32.0f, 80.0f };
    int m_ActiveShadowCascade = 0;
    bool m_ShadowMapReady = false;

    Velcryn::RHI::TextureHandle m_PostColorTexture{};
    Velcryn::RHI::TextureHandle m_BloomTexture[2]{};
    Velcryn::RHI::TextureHandle m_HistoryTexture[2]{};
    int m_HistoryReadIndex = 0;
    bool m_HistoryValid = false;
    unsigned int m_ModelPreviewWidth = 0;
    unsigned int m_ModelPreviewHeight = 0;

    struct ModelPreviewTexture
    {
        Velcryn::RHI::TextureHandle color{};
        Velcryn::RHI::TextureHandle depth{};
        unsigned int width = 0;
        unsigned int height = 0;
    };
    std::unordered_map<std::string, ModelPreviewTexture> m_ModelPreviewCache;

    std::unique_ptr<Mesh> m_CubeMesh;
    std::unique_ptr<Mesh> m_PlaneMesh;
    std::unique_ptr<Mesh> m_SphereMesh;
    std::unique_ptr<Mesh> m_CylinderMesh;
    std::unordered_map<std::string, std::unique_ptr<ModelAsset>> m_ModelCache;
    std::filesystem::path m_ProjectRoot;

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
    RenderDebugView m_DebugView = RenderDebugView::Lit;
    Mat4 m_FrameViewProjection = Mat4::Identity();
    bool m_FrameShaderStateReady = false;

    struct DebugLine { Vec3 start; Vec3 end; Vec3 color; float remaining = 0.0f; };
    DebugRenderer m_DebugRenderer;
    std::vector<DebugLine> m_DebugLines;

    TextureManager m_TextureManager;
    EnvironmentSystem m_EnvironmentSystem;
    UIRenderer m_UIRenderer;

    Mesh* GetPrimitiveMesh(PrimitiveType primitive);
    Mesh* GetModelMesh(const std::string& modelPath);
    void DrawMeshInternal(Mesh* mesh, const Transform& transform, float red, float green, float blue, float alpha, const Texture2D* texture, float metallic, float roughness, float ambientOcclusion, float emissive, const Texture2D* normalMap, const Texture2D* metallicMap, const Texture2D* roughnessMap, const Texture2D* aoMap, const Texture2D* emissiveMap, const std::vector<Mat4>* bones = nullptr);
    void UploadFrameShaderState();

    bool CreateShadowTarget();
    void DestroyShadowTarget();
    void UpdateLightSpaceMatrices();

    bool CreatePostProcessTarget();
    void DestroyPostProcessTarget();
    void RenderPostProcess();
    Velcryn::RHI::TextureHandle RenderBloom();
    void ResolveTAA();
    bool EnsureModelPreviewTarget(unsigned int width, unsigned int height);
    void DestroyModelPreviewTarget();
    void DestroyModelPreviewCache();
};
