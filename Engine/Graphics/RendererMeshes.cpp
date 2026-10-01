#include "Renderer.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "ModelLoader.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"
#include "../Core/Logger.h"
#include <glad/gl.h>
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
    const std::string resolvedPath = ResolveAssetPath(modelPath);
    auto it = m_ModelCache.find(resolvedPath);
    if (it != m_ModelCache.end()) return it->second.get();

    std::unique_ptr<ModelAsset> loaded = ModelLoader::LoadModel(resolvedPath);
    if (!loaded) return nullptr;
    ModelAsset* result = loaded.get();
    m_ModelCache.emplace(resolvedPath, std::move(loaded));
    return result;
}

Mesh* Renderer::GetModelMesh(const std::string& modelPath)
{
    ModelAsset* model = GetModelAsset(modelPath);
    if (!model || model->sections.empty()) return nullptr;
    return model->sections.front().mesh.get();
}

bool Renderer::EnsureModelPreviewTarget(unsigned int width, unsigned int height)
{
    width=std::max(1u,width); height=std::max(1u,height);
    if(m_ModelPreviewFramebuffer && m_ModelPreviewWidth==width && m_ModelPreviewHeight==height) return true;
    DestroyModelPreviewTarget();
    glGenFramebuffers(1,&m_ModelPreviewFramebuffer); glBindFramebuffer(GL_FRAMEBUFFER,m_ModelPreviewFramebuffer);
    glGenTextures(1,&m_ModelPreviewTexture); glBindTexture(GL_TEXTURE_2D,m_ModelPreviewTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,(GLsizei)width,(GLsizei)height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m_ModelPreviewTexture,0);
    glGenRenderbuffers(1,&m_ModelPreviewDepth); glBindRenderbuffer(GL_RENDERBUFFER,m_ModelPreviewDepth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,(GLsizei)width,(GLsizei)height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,m_ModelPreviewDepth);
    bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE; glBindFramebuffer(GL_FRAMEBUFFER,0);
    if(!ok){DestroyModelPreviewTarget();return false;} m_ModelPreviewWidth=width;m_ModelPreviewHeight=height;return true;
}

void Renderer::DestroyModelPreviewTarget()
{
    if(m_ModelPreviewDepth)glDeleteRenderbuffers(1,&m_ModelPreviewDepth);
    if(m_ModelPreviewTexture)glDeleteTextures(1,&m_ModelPreviewTexture);
    if(m_ModelPreviewFramebuffer)glDeleteFramebuffers(1,&m_ModelPreviewFramebuffer);
    m_ModelPreviewDepth=m_ModelPreviewTexture=m_ModelPreviewFramebuffer=0;m_ModelPreviewWidth=m_ModelPreviewHeight=0;
}

unsigned int Renderer::RenderModelPreview(const std::string& path,unsigned int width,unsigned int height)
{
    ModelAsset* model=GetModelAsset(path);
    if(!model||model->sections.empty()) return 0;

    const Vertex* firstVertex=nullptr;
    for(const auto& section:model->sections)
    {
        if(section.mesh && !section.mesh->GetVertices().empty()){firstVertex=&section.mesh->GetVertices().front();break;}
    }
    if(!firstVertex) return 0;

    width=std::max(1u,width); height=std::max(1u,height);
    const std::string cacheKey=path+"#"+std::to_string(width)+"x"+std::to_string(height);
    auto cached=m_ModelPreviewCache.find(cacheKey);
    if(cached!=m_ModelPreviewCache.end()) return cached->second.texture;

    ModelPreviewTexture target;
    target.width=width; target.height=height;
    glGenFramebuffers(1,&target.framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);
    glGenTextures(1,&target.texture); glBindTexture(GL_TEXTURE_2D,target.texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,(GLsizei)width,(GLsizei)height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target.texture,0);
    glGenRenderbuffers(1,&target.depth); glBindRenderbuffer(GL_RENDERBUFFER,target.depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,(GLsizei)width,(GLsizei)height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,target.depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        if(target.depth)glDeleteRenderbuffers(1,&target.depth);
        if(target.texture)glDeleteTextures(1,&target.texture);
        if(target.framebuffer)glDeleteFramebuffers(1,&target.framebuffer);
        return 0;
    }

    Vec3 mn(firstVertex->position[0],firstVertex->position[1],firstVertex->position[2]),mx=mn;
    for(const auto& section:model->sections)
    {
        if(!section.mesh) continue;
        for(const Vertex& x:section.mesh->GetVertices())
        {
            mn.x=std::min(mn.x,x.position[0]);mn.y=std::min(mn.y,x.position[1]);mn.z=std::min(mn.z,x.position[2]);
            mx.x=std::max(mx.x,x.position[0]);mx.y=std::max(mx.y,x.position[1]);mx.z=std::max(mx.z,x.position[2]);
        }
    }
    Vec3 center((mn.x+mx.x)*.5f,(mn.y+mx.y)*.5f,(mn.z+mx.z)*.5f);
    float radius=std::max(.1f,std::max(mx.x-mn.x,std::max(mx.y-mn.y,mx.z-mn.z))*.5f),dist=radius*3.1f;
    Mat4 view=Mat4::LookAt(Vec3(center.x+dist*.78f,center.y+dist*.58f,center.z+dist),center,Vec3(0,1,0));
    Mat4 proj=Mat4::Perspective(45.f*.0174532925f,(float)width/(float)height,.01f,dist+radius*4.f);
    Mat4 mvp=proj*view;

    GLint oldFbo=0,oldVp[4]{};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&oldFbo);glGetIntegerv(GL_VIEWPORT,oldVp);
    glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);glViewport(0,0,(GLsizei)width,(GLsizei)height);glEnable(GL_DEPTH_TEST);
    glClearColor(.075f,.082f,.095f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    m_ModelPreviewShader.Bind();m_ModelPreviewShader.SetMat4("u_MVP",mvp);m_ModelPreviewShader.SetInt("u_Skinned",0);
    for(const auto& section:model->sections)
    {
        if(!section.mesh) continue;
        section.mesh->Bind();
        glDrawElements(GL_TRIANGLES,(GLsizei)section.mesh->GetIndexCount(),GL_UNSIGNED_INT,nullptr);
        section.mesh->Unbind();
    }
    m_ModelPreviewShader.Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)oldFbo);glViewport(oldVp[0],oldVp[1],oldVp[2],oldVp[3]);

    const unsigned int texture=target.texture;
    m_ModelPreviewCache.emplace(cacheKey,target);
    return texture;
}

unsigned int Renderer::RenderAnimatedModelPreview(const std::string& path,std::size_t clipIndex,float animationTime,unsigned int width,unsigned int height)
{
    ModelAsset* model=GetModelAsset(path);if(!model||model->sections.empty())return 0;
    if(!model->IsSkeletal()||model->animations.empty())return RenderModelPreview(path,width,height);
    if(!EnsureModelPreviewTarget(width,height))return 0;
    const Vertex* first=nullptr;for(const auto& s:model->sections)if(s.mesh&&!s.mesh->GetVertices().empty()){first=&s.mesh->GetVertices().front();break;}if(!first)return 0;
    Vec3 mn(first->position[0],first->position[1],first->position[2]),mx=mn;for(const auto& s:model->sections)if(s.mesh)for(const Vertex& v:s.mesh->GetVertices()){mn.x=std::min(mn.x,v.position[0]);mn.y=std::min(mn.y,v.position[1]);mn.z=std::min(mn.z,v.position[2]);mx.x=std::max(mx.x,v.position[0]);mx.y=std::max(mx.y,v.position[1]);mx.z=std::max(mx.z,v.position[2]);}
    Vec3 center((mn.x+mx.x)*.5f,(mn.y+mx.y)*.5f,(mn.z+mx.z)*.5f);float radius=std::max(.1f,std::max(mx.x-mn.x,std::max(mx.y-mn.y,mx.z-mn.z))*.5f),dist=radius*3.1f;Mat4 mvp=Mat4::Perspective(45.f*.0174532925f,(float)width/(float)height,.01f,dist+radius*4.f)*Mat4::LookAt(Vec3(center.x+dist*.78f,center.y+dist*.58f,center.z+dist),center,Vec3(0,1,0));
    const std::vector<Mat4> bones=model->EvaluateAnimation(clipIndex,animationTime,true);GLint oldFbo=0,oldVp[4]{};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&oldFbo);glGetIntegerv(GL_VIEWPORT,oldVp);glBindFramebuffer(GL_FRAMEBUFFER,m_ModelPreviewFramebuffer);glViewport(0,0,(GLsizei)width,(GLsizei)height);glEnable(GL_DEPTH_TEST);glClearColor(.075f,.082f,.095f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    m_ModelPreviewShader.Bind();m_ModelPreviewShader.SetMat4("u_MVP",mvp);m_ModelPreviewShader.SetInt("u_Skinned",1);for(std::size_t i=0;i<std::min<std::size_t>(bones.size(),128);++i){const std::string n="u_Bones["+std::to_string(i)+"]";m_ModelPreviewShader.SetMat4(n.c_str(),bones[i]);}for(const auto& s:model->sections){if(!s.mesh)continue;s.mesh->Bind();glDrawElements(GL_TRIANGLES,(GLsizei)s.mesh->GetIndexCount(),GL_UNSIGNED_INT,nullptr);s.mesh->Unbind();}m_ModelPreviewShader.Unbind();glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)oldFbo);glViewport(oldVp[0],oldVp[1],oldVp[2],oldVp[3]);return m_ModelPreviewTexture;
}

void Renderer::DestroyModelPreviewCache()
{
    for(auto& pair:m_ModelPreviewCache)
    {
        ModelPreviewTexture& target=pair.second;
        if(target.depth)glDeleteRenderbuffers(1,&target.depth);
        if(target.texture)glDeleteTextures(1,&target.texture);
        if(target.framebuffer)glDeleteFramebuffers(1,&target.framebuffer);
    }
    m_ModelPreviewCache.clear();
}

void Renderer::UploadFrameShaderState()
{
    m_Shader.Bind();
    for (int i = 0; i < ShadowCascadeCount; ++i)
    {
        static const char* matrixNames[ShadowCascadeCount] = { "u_LightSpaceMatrices[0]", "u_LightSpaceMatrices[1]", "u_LightSpaceMatrices[2]" };
        static const char* splitNames[ShadowCascadeCount] = { "u_ShadowCascadeSplits[0]", "u_ShadowCascadeSplits[1]", "u_ShadowCascadeSplits[2]" };
        static const char* samplerNames[ShadowCascadeCount] = { "u_ShadowMaps[0]", "u_ShadowMaps[1]", "u_ShadowMaps[2]" };
        m_Shader.SetMat4(matrixNames[i], m_LightSpaceMatrices[i]);
        m_Shader.SetFloat(splitNames[i], m_ShadowCascadeSplits[i]);
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, m_ShadowDepthTextures[i]);
        m_Shader.SetInt(samplerNames[i], 1 + i);
    }
    m_Shader.SetInt("u_ShadowsEnabled", (m_RenderSettings.shadows && m_ShadowMapReady) ? 1 : 0);
    m_Shader.SetInt("u_ShadowPCFRadius", std::clamp(m_RenderSettings.shadowQuality + 1, 1, 3));
    m_Shader.SetVec3("u_LightDirection", m_LightDirection.x, m_LightDirection.y, m_LightDirection.z);
    m_Shader.SetVec3("u_LightColor", m_LightColor.x, m_LightColor.y, m_LightColor.z);
    m_Shader.SetFloat("u_LightIntensity", m_LightIntensity);
    const Vec3 cameraPosition = m_Camera.GetPosition();
    m_Shader.SetVec3("u_CameraPosition", cameraPosition.x, cameraPosition.y, cameraPosition.z);
    m_Shader.SetFloat("u_IndirectLightStrength", m_RenderSettings.indirectLightStrength);
    m_Shader.SetFloat("u_EnvironmentReflectionStrength", m_RenderSettings.environmentReflectionStrength);
    m_Shader.SetFloat("u_ReflectionStrength", m_RenderSettings.reflectionStrength);
    m_Shader.SetFloat("u_ContactShadowStrength", m_RenderSettings.contactShadowStrength);
    m_Shader.SetFloat("u_SkyIntensity", m_RenderSettings.skyIntensity);
    m_Shader.SetInt("u_FogEnabled", m_RenderSettings.fog ? 1 : 0);
    m_Shader.SetFloat("u_FogDensity", m_RenderSettings.fogDensity);
    m_Shader.SetFloat("u_ViewDistance", m_RenderSettings.viewDistance);
    m_Shader.SetInt("u_PointLightCount", m_PointLightCount);
    m_Shader.SetInt("u_SpotLightCount", m_SpotLightCount);
    static const char* pointPosition[8]={"u_PointLights[0].position","u_PointLights[1].position","u_PointLights[2].position","u_PointLights[3].position","u_PointLights[4].position","u_PointLights[5].position","u_PointLights[6].position","u_PointLights[7].position"};
    static const char* pointColor[8]={"u_PointLights[0].color","u_PointLights[1].color","u_PointLights[2].color","u_PointLights[3].color","u_PointLights[4].color","u_PointLights[5].color","u_PointLights[6].color","u_PointLights[7].color"};
    static const char* pointIntensity[8]={"u_PointLights[0].intensity","u_PointLights[1].intensity","u_PointLights[2].intensity","u_PointLights[3].intensity","u_PointLights[4].intensity","u_PointLights[5].intensity","u_PointLights[6].intensity","u_PointLights[7].intensity"};
    static const char* pointRange[8]={"u_PointLights[0].range","u_PointLights[1].range","u_PointLights[2].range","u_PointLights[3].range","u_PointLights[4].range","u_PointLights[5].range","u_PointLights[6].range","u_PointLights[7].range"};
    for(int i=0;i<m_PointLightCount;i++){const auto& l=m_PointLights[i];m_Shader.SetVec3(pointPosition[i],l.position.x,l.position.y,l.position.z);m_Shader.SetVec3(pointColor[i],l.color.x,l.color.y,l.color.z);m_Shader.SetFloat(pointIntensity[i],l.intensity);m_Shader.SetFloat(pointRange[i],l.range);}
    static const char* spotPosition[4]={"u_SpotLights[0].position","u_SpotLights[1].position","u_SpotLights[2].position","u_SpotLights[3].position"};
    static const char* spotDirection[4]={"u_SpotLights[0].direction","u_SpotLights[1].direction","u_SpotLights[2].direction","u_SpotLights[3].direction"};
    static const char* spotColor[4]={"u_SpotLights[0].color","u_SpotLights[1].color","u_SpotLights[2].color","u_SpotLights[3].color"};
    static const char* spotIntensity[4]={"u_SpotLights[0].intensity","u_SpotLights[1].intensity","u_SpotLights[2].intensity","u_SpotLights[3].intensity"};
    static const char* spotRange[4]={"u_SpotLights[0].range","u_SpotLights[1].range","u_SpotLights[2].range","u_SpotLights[3].range"};
    static const char* spotInner[4]={"u_SpotLights[0].innerCos","u_SpotLights[1].innerCos","u_SpotLights[2].innerCos","u_SpotLights[3].innerCos"};
    static const char* spotOuter[4]={"u_SpotLights[0].outerCos","u_SpotLights[1].outerCos","u_SpotLights[2].outerCos","u_SpotLights[3].outerCos"};
    for(int i=0;i<m_SpotLightCount;i++){const auto& l=m_SpotLights[i];m_Shader.SetVec3(spotPosition[i],l.position.x,l.position.y,l.position.z);m_Shader.SetVec3(spotDirection[i],l.direction.x,l.direction.y,l.direction.z);m_Shader.SetVec3(spotColor[i],l.color.x,l.color.y,l.color.z);m_Shader.SetFloat(spotIntensity[i],l.intensity);m_Shader.SetFloat(spotRange[i],l.range);m_Shader.SetFloat(spotInner[i],l.innerCos);m_Shader.SetFloat(spotOuter[i],l.outerCos);}
    const bool hasEnvironment=m_EnvironmentSystem.GetEnvironmentMap()!=0;
    m_Shader.SetInt("u_UseEnvironmentMap",hasEnvironment?1:0);
    if(hasEnvironment){m_EnvironmentSystem.Bind(9);m_Shader.SetInt("u_EnvironmentMap",9);m_EnvironmentSystem.BindIrradiance(10);m_Shader.SetInt("u_IrradianceMap",10);}
    glActiveTexture(GL_TEXTURE0);
    m_Shader.Unbind();
    m_FrameShaderStateReady=true;
}

void Renderer::DrawMeshInternal(
    Mesh* mesh, const Transform& transform,
    float red, float green, float blue, float alpha,
    const Texture2D* texture, float metallic, float roughness,
    float ambientOcclusion, float emissive, const Texture2D* normalMap,
    const Texture2D* metallicMap, const Texture2D* roughnessMap,
    const Texture2D* aoMap, const Texture2D* emissiveMap, const std::vector<Mat4>* bones)
{
    if (!mesh) return;
    const Mat4 model = transform.GetMatrix();
    const Mat4 cameraTransform = m_FrameViewProjection * model;
    mesh->Bind();
    m_Shader.Bind();
    if (texture && texture->IsLoaded()){texture->Bind(0);m_Shader.SetInt("u_Texture",0);m_Shader.SetInt("u_UseTexture",1);}else m_Shader.SetInt("u_UseTexture",0);
    m_Shader.SetMat4("u_Transform",cameraTransform);
    m_Shader.SetVec4("u_Color",red,green,blue,alpha);
    m_Shader.SetMat4("u_Model",model);
    const bool skinned=bones&&!bones->empty();m_Shader.SetInt("u_Skinned",skinned?1:0);
    if(skinned){const std::size_t count=std::min<std::size_t>(bones->size(),128);for(std::size_t i=0;i<count;++i){const std::string n="u_Bones["+std::to_string(i)+"]";m_Shader.SetMat4(n.c_str(),(*bones)[i]);}}
    m_Shader.SetFloat("u_Metallic",metallic);m_Shader.SetFloat("u_Roughness",roughness);m_Shader.SetFloat("u_AO",ambientOcclusion);m_Shader.SetFloat("u_Emissive",emissive);
    const Texture2D* maps[5]={normalMap,metallicMap,roughnessMap,aoMap,emissiveMap};
    static const char* samplers[5]={"u_NormalMap","u_MetallicMap","u_RoughnessMap","u_AOMap","u_EmissiveMap"};
    static const char* toggles[5]={"u_UseNormalMap","u_UseMetallicMap","u_UseRoughnessMap","u_UseAOMap","u_UseEmissiveMap"};
    for(int i=0;i<5;i++){const bool valid=maps[i]&&maps[i]->IsLoaded();m_Shader.SetInt(toggles[i],valid?1:0);if(valid){maps[i]->Bind(4+i);m_Shader.SetInt(samplers[i],4+i);}}
    glActiveTexture(GL_TEXTURE0);
    glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(mesh->GetIndexCount()),GL_UNSIGNED_INT,nullptr);
    if(texture&&texture->IsLoaded())texture->Unbind();
    m_Shader.Unbind();mesh->Unbind();
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

        DrawMeshInternal(section.mesh.get(),transform,sectionRed,sectionGreen,sectionBlue,sectionAlpha,
            sectionTexture,sectionMetallic,sectionRoughness,ambientOcclusion,emissive,
            normalMap,metallicMap,roughnessMap,aoMap,emissiveMap,model->IsSkeletal()?&bindBones:nullptr);
    }
}

void Renderer::DrawAnimatedModel(const Transform& transform,const std::string& modelPath,std::size_t clipIndex,float animationTime,bool loop,float red,float green,float blue,float alpha)
{
    ModelAsset* model=GetModelAsset(modelPath);if(!model)return;
    if(!model->IsSkeletal()||model->animations.empty()){DrawModel(transform,modelPath,red,green,blue,alpha);return;}
    const std::vector<Mat4> bones=model->EvaluateAnimation(clipIndex,animationTime,loop);
    for(const MeshSection& section:model->sections){if(!section.mesh)continue;const ImportedMaterial* material=section.materialIndex<model->materials.size()?&model->materials[section.materialIndex]:nullptr;const Texture2D* tex=nullptr;if(material&&!material->diffuseTexture.empty())tex=LoadTexture(material->diffuseTexture);DrawMeshInternal(section.mesh.get(),transform,material?red*material->diffuse[0]:red,material?green*material->diffuse[1]:green,material?blue*material->diffuse[2]:blue,material?alpha*material->opacity:alpha,tex,material?material->Metallic():0.0f,material?material->Roughness():0.65f,1.0f,0.0f,nullptr,nullptr,nullptr,nullptr,nullptr,&bones);}
}

Texture2D* Renderer::LoadTexture(
	const std::string& filepath)
{
	return m_TextureManager.Load(
		ResolveAssetPath(filepath)
	);
}
