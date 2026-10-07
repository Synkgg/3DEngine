#include "Renderer.h"
#include "RHI/RHI.h"
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
    const std::string resolvedPath=ResolveAssetPath(modelPath);m_ModelCache.erase(resolvedPath);for(auto it=m_ModelPreviewCache.begin();it!=m_ModelPreviewCache.end();){if(it->first==resolvedPath||it->first.rfind(resolvedPath+"#",0)==0)it=m_ModelPreviewCache.erase(it);else ++it;}
}

Mesh* Renderer::GetModelMesh(const std::string& modelPath)
{
    ModelAsset* model = GetModelAsset(modelPath);
    if (!model || model->sections.empty()) return nullptr;
    return model->sections.front().mesh.get();
}

bool Renderer::EnsureModelPreviewTarget(unsigned int, unsigned int)
{
    // Model previews are rendered by the Vulkan scene path; no GL FBO is created.
    return true;
}

void Renderer::DestroyModelPreviewTarget()
{
    m_ModelPreviewWidth = m_ModelPreviewHeight = 0;
}

unsigned int Renderer::RenderModelPreview(const std::string&, unsigned int, unsigned int)
{
    return 0;
}

unsigned int Renderer::RenderAnimatedModelPreview(const std::string&, std::size_t, float, unsigned int, unsigned int)
{
    return 0;
}

void Renderer::DestroyModelPreviewCache()
{
    m_ModelPreviewCache.clear();
}


void Renderer::UploadFrameShaderState()
{
    m_FrameShaderStateReady = true;
}

void Renderer::DrawMeshInternal(Mesh* mesh, const Transform& transform, float red, float green, float blue, float alpha,
    const Texture2D*, float, float, float, float, const Texture2D*, const Texture2D*,
    const Texture2D*, const Texture2D*, const Texture2D*, const std::vector<Mat4>*)
{
    if (!mesh) return;
    struct Constants { Mat4 mvp; float color[4]; float normalColumns[3][4]; };
    const Mat4 model = transform.GetMatrix();
    Constants constants{m_FrameViewProjection * model, {red, green, blue, alpha}, {}};
    // Transform consists of rotation and scale: divide each basis column by
    // its squared length to obtain the inverse transpose for surface normals.
    for (int column = 0; column < 3; ++column)
    {
        float squaredLength = 0;
        for (int row = 0; row < 3; ++row) squaredLength += model.elements[column * 4 + row] * model.elements[column * 4 + row];
        if (squaredLength > 1e-12f)
            for (int row = 0; row < 3; ++row) constants.normalColumns[column][row] = model.elements[column * 4 + row] / squaredLength;
    }
    static_assert(sizeof(Constants) == 128);
    if (auto* device = Velcryn::RHI::GetDevice())
        device->DrawIndexed(m_MeshPipeline, mesh->GetVertexBuffer(), mesh->GetIndexBuffer(),
            mesh->GetIndexCount(), &constants, sizeof(constants));
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
    const Texture2D* emissiveMap)
{
    ModelAsset* model=GetModelAsset(modelPath);
    if(!model) return;
    std::vector<Mat4> bindBones;
    if(model->IsSkeletal()) bindBones=model->BindPose();
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
        const float sectionMetallic=material?std::max(metallic,material->Metallic()):metallic;
        const float sectionRoughness=material?material->Roughness():roughness;

        const Texture2D* sectionNormal=normalMap;const Texture2D* sectionMR=nullptr;const Texture2D* sectionAO=aoMap;const Texture2D* sectionEmissive=emissiveMap;
        if(material){if(!sectionNormal&&!material->normalTexture.empty())sectionNormal=LoadTexture(material->normalTexture);if(!material->metallicRoughnessTexture.empty())sectionMR=LoadTexture(material->metallicRoughnessTexture);if(!sectionAO&&!material->occlusionTexture.empty())sectionAO=LoadTexture(material->occlusionTexture);if(!sectionEmissive&&!material->emissiveTexture.empty())sectionEmissive=LoadTexture(material->emissiveTexture);}
        const float sectionEmissiveAmount=material?std::max({emissive,material->emissiveFactor[0],material->emissiveFactor[1],material->emissiveFactor[2]}):emissive;
        DrawMeshInternal(section.mesh.get(),transform,sectionRed,sectionGreen,sectionBlue,sectionAlpha,
            sectionTexture,sectionMetallic,sectionRoughness,ambientOcclusion,sectionEmissiveAmount,
            sectionNormal,sectionMR?sectionMR:metallicMap,sectionMR?sectionMR:roughnessMap,sectionAO,sectionEmissive,model->IsSkeletal()?&bindBones:nullptr);
    }
}

void Renderer::DrawAnimatedModel(const Transform& transform,const std::string& modelPath,std::size_t clipIndex,float animationTime,bool loop,float red,float green,float blue,float alpha)
{
    ModelAsset* model=GetModelAsset(modelPath);if(!model)return;
    if(!model->IsSkeletal()||model->animations.empty()){DrawModel(transform,modelPath,red,green,blue,alpha);return;}
    const std::vector<Mat4> bones=model->EvaluateAnimation(clipIndex,animationTime,loop);
    for(const MeshSection& section:model->sections){if(!section.mesh)continue;const ImportedMaterial* material=section.materialIndex<model->materials.size()?&model->materials[section.materialIndex]:nullptr;const Texture2D* tex=nullptr;const Texture2D* normal=nullptr;const Texture2D* mr=nullptr;const Texture2D* ao=nullptr;const Texture2D* em=nullptr;float emAmount=0.0f;if(material){if(!material->diffuseTexture.empty())tex=LoadTexture(material->diffuseTexture);if(!material->normalTexture.empty())normal=LoadTexture(material->normalTexture);if(!material->metallicRoughnessTexture.empty())mr=LoadTexture(material->metallicRoughnessTexture);if(!material->occlusionTexture.empty())ao=LoadTexture(material->occlusionTexture);if(!material->emissiveTexture.empty())em=LoadTexture(material->emissiveTexture);emAmount=std::max({material->emissiveFactor[0],material->emissiveFactor[1],material->emissiveFactor[2]});}DrawMeshInternal(section.mesh.get(),transform,material?red*material->diffuse[0]:red,material?green*material->diffuse[1]:green,material?blue*material->diffuse[2]:blue,material?alpha*material->opacity:alpha,tex,material?material->Metallic():0.0f,material?material->Roughness():0.65f,1.0f,emAmount,normal,mr,mr,ao,em,&bones);}
}

Texture2D* Renderer::LoadTexture(
	const std::string& filepath)
{
	return m_TextureManager.Load(
		ResolveAssetPath(filepath)
	);
}
