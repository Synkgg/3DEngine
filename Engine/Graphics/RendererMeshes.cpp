#include "Renderer.h"
#include "RHI/RHI.h"
#include "MeshDrawData.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "ModelLoader.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"
#include "../Core/Logger.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>

Mesh* Renderer::GetPrimitiveMesh(PrimitiveType primitive)
{
    switch (primitive)
    {
    case PrimitiveType::Cube: return m_CubeMesh.get();
    case PrimitiveType::Plane: return m_PlaneMesh.get();
    case PrimitiveType::Sphere: return m_SphereMesh.get();
    case PrimitiveType::Cylinder: return m_CylinderMesh.get();
    default: return nullptr;
    }
}

std::string Renderer::ResolveAssetPath(const std::string& path) const
{
    if (path.empty()) return path;
    const std::filesystem::path input(path);
    if (input.is_absolute() || m_ProjectRoot.empty())
        return input.lexically_normal().string();
    return (m_ProjectRoot / input).lexically_normal().string();
}

ModelAsset* Renderer::GetModelAsset(const std::string& modelPath)
{
    if (modelPath.empty()) return nullptr;
    std::string resolvedPath = ResolveAssetPath(modelPath);
    const std::string sourceKey = resolvedPath;
    const auto now = std::chrono::steady_clock::now();
    auto previous = m_ModelSourceChecks.find(sourceKey);
    if (previous != m_ModelSourceChecks.end() && now < previous->second.nextCheck) {
        auto cached = m_ModelCache.find(previous->second.assetPath);
        return cached == m_ModelCache.end() ? nullptr : cached->second.get();
    }
    const std::filesystem::path sourcePath(resolvedPath);
    const std::string extension = sourcePath.extension().string();
    if (extension == ".obj" || extension == ".gltf" || extension == ".glb")
    {
        std::filesystem::path importedPath = sourcePath;
        importedPath.replace_extension(".modelasset");
        std::error_code ec;
        const bool importedExists = std::filesystem::exists(importedPath, ec) && !ec;
        bool needsImport = !importedExists || (importedExists && !ModelLoader::IsImportedAssetCurrent(importedPath.string()));
        if(importedExists && ModelLoader::SourceDependenciesNewer(sourcePath.string(),importedPath.string())) needsImport=true;

        if (needsImport)
        {
            ModelImportSettings settings;
            if (importedExists)
            {
                ModelImportSettings storedSettings;
                if (ModelLoader::ReadImportedAssetSettings(importedPath.string(), storedSettings))
                    settings = storedSettings;
                else
                    Logger::Warning("Could not read import settings from " + importedPath.filename().string() + "; using defaults.");
            }
            std::unique_ptr<ModelAsset> imported = ModelLoader::LoadModel(sourcePath.string(), settings);
            if (imported && ModelLoader::SaveImportedAsset(importedPath.string(), *imported, settings))
            {
                Logger::Info(std::string(importedExists ? "Auto-reimported model: " : "Auto-imported model: ") +
                    sourcePath.filename().string() + " -> " + importedPath.filename().string());
                m_ModelCache.erase(importedPath.lexically_normal().string());
                for(auto& [key,preview]:m_ModelPreviewCache)
                    if(preview.path==sourceKey || preview.path==importedPath.lexically_normal().string())preview.pending=true;
            }
            else if (!importedExists)
            {
                Logger::Warning("Automatic model import failed; using source asset: " + sourcePath.string());
            }
        }

        ec.clear();
        if (std::filesystem::exists(importedPath, ec) && !ec)
            resolvedPath = importedPath.lexically_normal().string();
    }
    // Dependency scans parse source files; do not repeat for every instance.
    m_ModelSourceChecks[sourceKey] = {now + std::chrono::seconds(2), resolvedPath};
    auto it = m_ModelCache.find(resolvedPath);
    if (it != m_ModelCache.end()) return it->second.get();

    std::unique_ptr<ModelAsset> loaded = ModelLoader::LoadModel(resolvedPath);
    if (!loaded) return nullptr;
    ModelAsset* result = loaded.get();
    m_ModelCache.emplace(resolvedPath, std::move(loaded));
    return result;
}

void Renderer::InvalidateModelAsset(const std::string& modelPath)
{
    const auto source = m_ModelSourceChecks.find(ResolveAssetPath(modelPath));
    if (source != m_ModelSourceChecks.end()) m_ModelCache.erase(source->second.assetPath);
    m_ModelSourceChecks.clear();
    const std::string resolvedPath=ResolveAssetPath(modelPath);m_ModelCache.erase(resolvedPath);for(auto it=m_ModelPreviewCache.begin();it!=m_ModelPreviewCache.end();){if(it->first==resolvedPath||it->first.rfind(resolvedPath+"#",0)==0){it->second.pending=true;++it;}else ++it;}
}

Mesh* Renderer::GetModelMesh(const std::string& modelPath)
{
    ModelAsset* model = GetModelAsset(modelPath);
    if (!model || model->sections.empty()) return nullptr;
    return model->sections.front().mesh.get();
}

std::uint64_t Renderer::RenderModelPreview(const std::string& path, unsigned int width, unsigned int height)
{
    auto* device = Velcryn::RHI::GetDevice();
    if (!device || path.empty() || !width || !height) return 0;
    width = std::min(width, 2048u); height = std::min(height, 2048u);
    const std::string resolved = ResolveAssetPath(path);
    const std::string key = resolved + "#" + std::to_string(width) + "x" + std::to_string(height);
    auto [it, inserted] = m_ModelPreviewCache.try_emplace(key);
    auto& preview = it->second;
    if (inserted) {
        using namespace Velcryn::RHI;
        TextureDesc desc{}; desc.width=width; desc.height=height;
        desc.format=TextureFormat::RGBA16_Float;
        desc.usage=TextureUsage::RenderTarget|TextureUsage::Sampled|TextureUsage::TransferSource;
        desc.debugName="ModelPreviewColor"; preview.color=device->CreateTexture(desc);
        desc.format=TextureFormat::D32_Float; desc.usage=TextureUsage::DepthStencil|TextureUsage::Sampled;
        desc.debugName="ModelPreviewDepth"; preview.depth=device->CreateTexture(desc);
        preview.width=width; preview.height=height; preview.path=resolved;
        if (!preview.color || !preview.depth) {
            device->DestroyTexture(preview.color); device->DestroyTexture(preview.depth);
            m_ModelPreviewCache.erase(it); return 0;
        }
    }
    return device->GetImGuiTextureID(preview.color);
}

std::uint64_t Renderer::RenderAnimatedModelPreview(const std::string& path, std::size_t clip, float time, unsigned int width, unsigned int height)
{
    const auto id = RenderModelPreview(path,width,height);
    const std::string key=ResolveAssetPath(path)+"#"+std::to_string(std::min(width,2048u))+"x"+std::to_string(std::min(height,2048u));
    if (auto it=m_ModelPreviewCache.find(key); it!=m_ModelPreviewCache.end()) {
        it->second.animated=true; it->second.clip=clip; it->second.time=time; it->second.pending=true;
    }
    return id;
}

void Renderer::RenderPendingPreviews()
{
    auto* device=Velcryn::RHI::GetDevice();
    const auto savedVP=m_FrameViewProjection;
    m_InPreviewPass=true;
    for (auto& [key, preview] : m_ModelPreviewCache) {
        if (!preview.pending) continue;
        auto* model=GetModelAsset(preview.path);
        const float clear[]={.055f,.065f,.085f,1};
        device->BeginRendering(preview.color,preview.depth,clear);
        if (model) {
            Vec3 lower(1e30f,1e30f,1e30f), upper(-1e30f,-1e30f,-1e30f);
            bool any=false;
            for (const auto& section:model->sections) if(section.mesh) for(const auto& vertex:section.mesh->GetVertices()) {
                any=true;
                lower.x=std::min(lower.x,vertex.position[0]); lower.y=std::min(lower.y,vertex.position[1]); lower.z=std::min(lower.z,vertex.position[2]);
                upper.x=std::max(upper.x,vertex.position[0]); upper.y=std::max(upper.y,vertex.position[1]); upper.z=std::max(upper.z,vertex.position[2]);
            }
            if(any) {
                const Vec3 center=(lower+upper)*.5f;
                const float radius=std::max({upper.x-lower.x,upper.y-lower.y,upper.z-lower.z,.01f})*.5f;
                const float distance=radius*3.1f;
                m_FrameViewProjection=Mat4::Perspective(45.f*3.14159265f/180.f,float(preview.width)/preview.height,.001f,distance+radius*8)*Mat4::LookAt(center+Vec3(.78f,.58f,1)*distance,center,Vec3(0,1,0));
                if(preview.animated) DrawAnimatedModel(Transform{},preview.path,preview.clip,preview.time,true);
                else DrawModel(Transform{},preview.path,1,1,1,1);
            }
        }
        device->EndRendering();
        preview.pending=false;
    }
    m_InPreviewPass=false;
    m_FrameViewProjection=savedVP;
}

void Renderer::DestroyModelPreviewCache()
{
    if(auto* device=Velcryn::RHI::GetDevice()) for(auto& [key,preview]:m_ModelPreviewCache) {
        device->DestroyTexture(preview.color); device->DestroyTexture(preview.depth);
    }
    m_ModelPreviewCache.clear();
}


void Renderer::DrawMeshInternal(Mesh* mesh, const Transform& transform, float red, float green, float blue, float alpha,
    const Texture2D* texture, float metallic, float roughness, float ambientOcclusion, float emissive, const Texture2D* normalMap, const Texture2D* metallicMap,
    const Texture2D* roughnessMap, const Texture2D* aoMap, const Texture2D* emissiveMap, const std::vector<Mat4>* bones)
{
    if (!mesh) return;
    struct Constants { Mat4 mvp; float color[4]; float normalColumns[3][4]; };
    const Mat4 model = transform.GetMatrix();
    Constants constants{(m_InShadowPass ? m_LightSpaceMatrices[m_ActiveShadowCascade] : m_FrameViewProjection) * model, {red, green, blue, alpha}, {}};
    // Transform consists of rotation and scale: divide each basis column by
    // its squared length to obtain the inverse transpose for surface normals.
    for (int column = 0; column < 3; ++column)
    {
        float squaredLength = 0;
        for (int row = 0; row < 3; ++row) squaredLength += model.elements[column * 4 + row] * model.elements[column * 4 + row];
        if (squaredLength > 1e-12f)
            for (int row = 0; row < 3; ++row) constants.normalColumns[column][row] = model.elements[column * 4 + row] / squaredLength;
    }
    Vec3 lightDirection = m_LightDirection.Length() > 0.001f
        ? m_LightDirection.Normalized() : Vec3(-0.4f, -0.8f, -0.6f).Normalized();
    constants.normalColumns[0][3] = lightDirection.x;
    constants.normalColumns[1][3] = lightDirection.y;
    constants.normalColumns[2][3] = lightDirection.z;
    static_assert(sizeof(Constants) == 128);
    const auto sampledTexture = texture && texture->IsLoaded() ? texture->GetHandle() : m_WhiteTexture;
    MeshDrawData data{};
    data.model = model;
    data.lightColorIntensity[0] = m_LightColor.x; data.lightColorIntensity[1] = m_LightColor.y;
    data.lightColorIntensity[2] = m_LightColor.z; data.lightColorIntensity[3] = m_LightIntensity;
    const auto camera = m_Camera.GetPosition();
    data.cameraAmbient[0] = camera.x; data.cameraAmbient[1] = camera.y; data.cameraAmbient[2] = camera.z;
    data.cameraAmbient[3] = 0.15f * m_RenderSettings.indirectLightStrength;
    data.material[0] = metallic; data.material[1] = roughness; data.material[2] = ambientOcclusion; data.material[3] = emissive;
    data.settings[0] = m_RenderSettings.exposure; data.settings[1] = float(m_PointLightCount); data.settings[2] = float(m_SpotLightCount);
    for (int i = 0; i < m_PointLightCount; ++i) {
        const auto& light = m_PointLights[i]; auto& gpu = data.points[i];
        gpu = {{light.position.x, light.position.y, light.position.z, light.range}, {light.color.x, light.color.y, light.color.z, light.intensity}};
    }
    for (int i = 0; i < m_SpotLightCount; ++i) {
        const auto& light = m_SpotLights[i]; auto& gpu = data.spots[i];
        gpu = {{light.position.x, light.position.y, light.position.z, light.range}, {light.color.x, light.color.y, light.color.z, light.intensity},
            {light.direction.x, light.direction.y, light.direction.z, light.innerCos}, {light.outerCos, 0, 0, 0}};
    }
    data.environment[0]=m_RenderSettings.indirectLightStrength; data.environment[1]=m_RenderSettings.environmentReflectionStrength;
    data.environment[2]=m_RenderSettings.reflectionStrength; data.environment[3]=m_RenderSettings.skyIntensity;
    data.fog[0]=m_RenderSettings.fog?1.f:0.f;data.fog[1]=m_RenderSettings.fogDensity;data.fog[2]=m_RenderSettings.viewDistance;data.fog[3]=m_RenderSettings.contactShadowStrength;
    const Texture2D* maps[]={normalMap,metallicMap,roughnessMap,aoMap,emissiveMap};
    std::array<Velcryn::RHI::TextureHandle,10> textures{};
    uint32_t flags=0;
    for(int i=0;i<5;++i){const bool loaded=maps[i]&&maps[i]->IsLoaded();textures[i]=loaded?maps[i]->GetHandle():m_WhiteTexture;if(loaded)flags|=1u<<i;}
    data.mapFlags[0]=float(flags);data.mapFlags[1]=metallicMap&&metallicMap==roughnessMap?1.f:0.f;
    data.mapFlags[2]=m_ShadowMapReady&&m_RenderSettings.shadows?1.f:0.f;data.mapFlags[3]=float(std::clamp(m_RenderSettings.shadowQuality,1,3));
    for(int i=0;i<3;++i){textures[5+i]=m_ShadowMapReady?m_ShadowDepthTextures[i]:m_WhiteTexture;data.lightMatrices[i]=m_LightSpaceMatrices[i];data.cascadeSplits[i]=m_ShadowCascadeSplits[i];}
    textures[8]=m_EnvironmentSystem.GetEnvironmentMap();textures[9]=m_EnvironmentSystem.GetIrradianceMap();
    if(bones&&!bones->empty()){
        data.settings[3]=1;for(int i=0;i<128;++i)data.bones[i]=i<bones->size()?(*bones)[i]:Mat4::Identity();
    }
    if (auto* device = Velcryn::RHI::GetDevice())
        device->DrawIndexed(m_InShadowPass?m_ShadowPipeline:(m_InPreviewPass?m_PreviewPipeline:m_MeshPipeline), mesh->GetVertexBuffer(), mesh->GetIndexBuffer(),
            mesh->GetIndexCount(), &constants, sizeof(constants), 1, sampledTexture, std::as_bytes(std::span{&data, 1}),
            (m_InShadowPass||m_InPreviewPass)?std::span<const Velcryn::RHI::TextureHandle>{}:std::span<const Velcryn::RHI::TextureHandle>{textures});

}

void Renderer::DrawMesh(
	const Transform& transform,
	PrimitiveType primitive,
	float red,
	float green,
	float blue,
	float alpha,
	const Texture2D* texture,
	float metallic,
	float roughness,
	float ambientOcclusion,
	float emissive,
    const Texture2D* normalMap, const Texture2D* metallicMap,
    const Texture2D* roughnessMap, const Texture2D* aoMap,
    const Texture2D* emissiveMap)
{
    DrawMeshInternal(GetPrimitiveMesh(primitive), transform, red, green, blue, alpha,
        texture, metallic, roughness, ambientOcclusion, emissive,
        normalMap, metallicMap, roughnessMap, aoMap, emissiveMap, nullptr);
}

void Renderer::DrawModel(
    const Transform& transform, const std::string& modelPath,
    float red, float green, float blue, float alpha,
    const Texture2D* texture, float metallic, float roughness,
    float ambientOcclusion, float emissive,
    const Texture2D* normalMap, const Texture2D* metallicMap,
    const Texture2D* roughnessMap, const Texture2D* aoMap,
    const Texture2D* emissiveMap, bool overrideMaterial, const std::vector<Mat4>* pose)
{
    ModelAsset* model=GetModelAsset(modelPath);
    if(!model) return;
    std::vector<Mat4> bindBones;
    if(model->IsSkeletal() && !pose) bindBones=model->BindPose();
    for(const MeshSection& section:model->sections)
    {
        if(!section.mesh) continue;
        const ImportedMaterial* material=section.materialIndex<model->materials.size()?&model->materials[section.materialIndex]:nullptr;
        const Texture2D* sectionTexture=texture;
        if(!sectionTexture && material && !material->diffuseTexture.empty())
            sectionTexture=LoadTexture(material->diffuseTexture);

        const float sectionRed=material?red*material->diffuse[0]:red;
        const float sectionGreen=material?green*material->diffuse[1]:green;
        const float sectionBlue=material?blue*material->diffuse[2]:blue;
        const float sectionAlpha=material?alpha*material->opacity:alpha;
        const float sectionMetallic=material && !overrideMaterial ? material->Metallic() : metallic;
        const float sectionRoughness=material && !overrideMaterial ? material->Roughness() : roughness;

        const Texture2D* sectionNormal=normalMap;const Texture2D* sectionMR=nullptr;const Texture2D* sectionAO=aoMap;const Texture2D* sectionEmissive=emissiveMap;
        if(material){if(!sectionNormal&&!material->normalTexture.empty())sectionNormal=LoadTexture(material->normalTexture);if(!material->metallicRoughnessTexture.empty())sectionMR=LoadTexture(material->metallicRoughnessTexture);if(!sectionAO&&!material->occlusionTexture.empty())sectionAO=LoadTexture(material->occlusionTexture);if(!sectionEmissive&&!material->emissiveTexture.empty())sectionEmissive=LoadTexture(material->emissiveTexture);}
        const float sectionEmissiveAmount=material?std::max({emissive,material->emissiveFactor[0],material->emissiveFactor[1],material->emissiveFactor[2]}):emissive;
        DrawMeshInternal(section.mesh.get(),transform,sectionRed,sectionGreen,sectionBlue,sectionAlpha,
            sectionTexture,sectionMetallic,sectionRoughness,ambientOcclusion,sectionEmissiveAmount,
            sectionNormal,metallicMap?metallicMap:sectionMR,roughnessMap?roughnessMap:sectionMR,sectionAO,sectionEmissive,pose ? pose : (model->IsSkeletal()?&bindBones:nullptr));
    }
}

void Renderer::DrawAnimatedModel(const Transform& transform, const std::string& modelPath,
    std::size_t clipIndex, float animationTime, bool loop, float red, float green, float blue, float alpha,
    const Texture2D* texture, float metallic, float roughness, float ambientOcclusion, float emissive,
    const Texture2D* normalMap, const Texture2D* metallicMap, const Texture2D* roughnessMap,
    const Texture2D* aoMap, const Texture2D* emissiveMap, bool overrideMaterial)
{
    ModelAsset* model = GetModelAsset(modelPath);
    if (!model) return;
    const auto bones = model->IsSkeletal() ? model->EvaluateAnimation(clipIndex, animationTime, loop) : std::vector<Mat4>{};
    DrawModel(transform, modelPath, red, green, blue, alpha, texture, metallic, roughness,
        ambientOcclusion, emissive, normalMap, metallicMap, roughnessMap, aoMap, emissiveMap,
        overrideMaterial, bones.empty() ? nullptr : &bones);
}

Texture2D* Renderer::LoadTexture(
	const std::string& filepath)
{
	return m_TextureManager.Load(
		ResolveAssetPath(filepath)
	);
}
