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

Mesh* Renderer::GetModelMesh(const std::string& modelPath)
{
    if (modelPath.empty()) return nullptr;
    auto it = m_ModelCache.find(modelPath);
    if (it != m_ModelCache.end()) return it->second.get();

    std::unique_ptr<Mesh> loaded = ModelLoader::LoadOBJ(modelPath);
    if (!loaded) return nullptr;
    Mesh* result = loaded.get();
    m_ModelCache.emplace(modelPath, std::move(loaded));
    return result;
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
    Mesh* mesh=GetModelMesh(path);
    if(!mesh||mesh->GetVertices().empty()) return 0;

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

    const auto& v=mesh->GetVertices(); Vec3 mn(v[0].position[0],v[0].position[1],v[0].position[2]),mx=mn;
    for(const Vertex& x:v){mn.x=std::min(mn.x,x.position[0]);mn.y=std::min(mn.y,x.position[1]);mn.z=std::min(mn.z,x.position[2]);mx.x=std::max(mx.x,x.position[0]);mx.y=std::max(mx.y,x.position[1]);mx.z=std::max(mx.z,x.position[2]);}
    Vec3 center((mn.x+mx.x)*.5f,(mn.y+mx.y)*.5f,(mn.z+mx.z)*.5f);
    float radius=std::max(.1f,std::max(mx.x-mn.x,std::max(mx.y-mn.y,mx.z-mn.z))*.5f),dist=radius*3.1f;
    Mat4 view=Mat4::LookAt(Vec3(center.x+dist*.78f,center.y+dist*.58f,center.z+dist),center,Vec3(0,1,0));
    Mat4 proj=Mat4::Perspective(45.f*.0174532925f,(float)width/(float)height,.01f,dist+radius*4.f);
    Mat4 mvp=proj*view;

    GLint oldFbo=0,oldVp[4]{};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&oldFbo);glGetIntegerv(GL_VIEWPORT,oldVp);
    glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);glViewport(0,0,(GLsizei)width,(GLsizei)height);glEnable(GL_DEPTH_TEST);
    glClearColor(.075f,.082f,.095f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    m_ModelPreviewShader.Bind();m_ModelPreviewShader.SetMat4("u_MVP",mvp);mesh->Bind();
    glDrawElements(GL_TRIANGLES,(GLsizei)mesh->GetIndexCount(),GL_UNSIGNED_INT,nullptr);mesh->Unbind();m_ModelPreviewShader.Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)oldFbo);glViewport(oldVp[0],oldVp[1],oldVp[2],oldVp[3]);

    const unsigned int texture=target.texture;
    m_ModelPreviewCache.emplace(cacheKey,target);
    return texture;
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

void Renderer::DrawMeshInternal(
    Mesh* mesh, const Transform& transform,
    float red, float green, float blue, float alpha,
    const Texture2D* texture, float metallic, float roughness,
    float ambientOcclusion, float emissive, const Texture2D* normalMap,
    const Texture2D* metallicMap, const Texture2D* roughnessMap,
    const Texture2D* aoMap, const Texture2D* emissiveMap)
{
    if (!mesh) return;	Mat4 model =
		transform.GetMatrix();

	Mat4 view =
		m_Camera.GetViewMatrix();

	Mat4 projection =
		m_Camera.GetProjectionMatrix();

	Mat4 cameraTransform =
		projection * view * model;

	mesh->Bind();
	m_Shader.Bind();

	if (texture != nullptr &&
		texture->IsLoaded())
	{
		texture->Bind(0);

		m_Shader.SetInt(
			"u_Texture",
			0
		);

		m_Shader.SetInt(
			"u_UseTexture",
			1
		);
	}
	else
	{
		m_Shader.SetInt(
			"u_UseTexture",
			0
		);
	}

	m_Shader.SetMat4(
		"u_Transform",
		cameraTransform
	);

	m_Shader.SetVec4(
		"u_Color",
		red,
		green,
		blue,
		alpha
	);

	m_Shader.SetMat4(
		"u_Model",
		model
	);
    for (int i = 0; i < ShadowCascadeCount; ++i) {
        m_Shader.SetMat4(("u_LightSpaceMatrices[" + std::to_string(i) + "]").c_str(), m_LightSpaceMatrices[i]);
        m_Shader.SetFloat(("u_ShadowCascadeSplits[" + std::to_string(i) + "]").c_str(), m_ShadowCascadeSplits[i]);
        glActiveTexture(GL_TEXTURE1 + i); glBindTexture(GL_TEXTURE_2D, m_ShadowDepthTextures[i]);
        m_Shader.SetInt(("u_ShadowMaps[" + std::to_string(i) + "]").c_str(), 1 + i);
    }
    m_Shader.SetInt("u_ShadowsEnabled", (m_RenderSettings.shadows && m_ShadowMapReady) ? 1 : 0);
    m_Shader.SetInt("u_ShadowPCFRadius", std::clamp(m_RenderSettings.shadowQuality + 1, 1, 3));
    glActiveTexture(GL_TEXTURE0);

	m_Shader.SetVec3(
		"u_LightDirection",
		m_LightDirection.x,
		m_LightDirection.y,
		m_LightDirection.z
	);

	m_Shader.SetVec3(
		"u_LightColor",
		m_LightColor.x,
		m_LightColor.y,
		m_LightColor.z
	);

	m_Shader.SetFloat(
		"u_LightIntensity",
		m_LightIntensity
	);

	const Vec3 cameraPosition = m_Camera.GetPosition();
	m_Shader.SetVec3(
		"u_CameraPosition",
		cameraPosition.x,
		cameraPosition.y,
		cameraPosition.z
	);
	m_Shader.SetFloat("u_Metallic", metallic);
	m_Shader.SetFloat("u_Roughness", roughness);
	m_Shader.SetFloat("u_AO", ambientOcclusion);
	m_Shader.SetFloat("u_Emissive", emissive);
    m_Shader.SetFloat("u_IndirectLightStrength", m_RenderSettings.indirectLightStrength);
    m_Shader.SetFloat("u_ReflectionStrength", m_RenderSettings.reflectionStrength);
    m_Shader.SetFloat("u_ContactShadowStrength", m_RenderSettings.contactShadowStrength);
    m_Shader.SetFloat("u_SkyIntensity", m_RenderSettings.skyIntensity);
    const bool hasEnvironment = m_EnvironmentSystem.GetEnvironmentMap() != 0;
    m_Shader.SetInt("u_UseEnvironmentMap", hasEnvironment ? 1 : 0);
    if (hasEnvironment)
    {
        m_EnvironmentSystem.Bind(9); m_Shader.SetInt("u_EnvironmentMap", 9);
        m_EnvironmentSystem.BindIrradiance(10); m_Shader.SetInt("u_IrradianceMap", 10);
    }
    const Texture2D* maps[5] = { normalMap, metallicMap, roughnessMap, aoMap, emissiveMap };
    const char* samplers[5] = { "u_NormalMap", "u_MetallicMap", "u_RoughnessMap", "u_AOMap", "u_EmissiveMap" };
    const char* toggles[5] = { "u_UseNormalMap", "u_UseMetallicMap", "u_UseRoughnessMap", "u_UseAOMap", "u_UseEmissiveMap" };
    for (int mapIndex = 0; mapIndex < 5; ++mapIndex)
    {
        const bool valid = maps[mapIndex] && maps[mapIndex]->IsLoaded();
        m_Shader.SetInt(toggles[mapIndex], valid ? 1 : 0);
        if (valid)
        {
            maps[mapIndex]->Bind(4 + mapIndex);
            m_Shader.SetInt(samplers[mapIndex], 4 + mapIndex);
        }
    }
    glActiveTexture(GL_TEXTURE0);
	m_Shader.SetInt("u_FogEnabled", m_RenderSettings.fog ? 1 : 0);
	m_Shader.SetFloat("u_FogDensity", m_RenderSettings.fogDensity);
    m_Shader.SetFloat("u_ViewDistance", m_RenderSettings.viewDistance);
	m_Shader.SetInt("u_PointLightCount", m_PointLightCount);
	m_Shader.SetInt("u_SpotLightCount", m_SpotLightCount);
	for(int i=0;i<m_PointLightCount;i++) {
		std::string b="u_PointLights["+std::to_string(i)+"]";
		m_Shader.SetVec3((b+".position").c_str(),m_PointLights[i].position.x,m_PointLights[i].position.y,m_PointLights[i].position.z);
		m_Shader.SetVec3((b+".color").c_str(),m_PointLights[i].color.x,m_PointLights[i].color.y,m_PointLights[i].color.z);
		m_Shader.SetFloat((b+".intensity").c_str(),m_PointLights[i].intensity); m_Shader.SetFloat((b+".range").c_str(),m_PointLights[i].range);
	}
	for(int i=0;i<m_SpotLightCount;i++) {
		std::string b="u_SpotLights["+std::to_string(i)+"]";
		m_Shader.SetVec3((b+".position").c_str(),m_SpotLights[i].position.x,m_SpotLights[i].position.y,m_SpotLights[i].position.z);
		m_Shader.SetVec3((b+".direction").c_str(),m_SpotLights[i].direction.x,m_SpotLights[i].direction.y,m_SpotLights[i].direction.z);
		m_Shader.SetVec3((b+".color").c_str(),m_SpotLights[i].color.x,m_SpotLights[i].color.y,m_SpotLights[i].color.z);
		m_Shader.SetFloat((b+".intensity").c_str(),m_SpotLights[i].intensity);m_Shader.SetFloat((b+".range").c_str(),m_SpotLights[i].range);
		m_Shader.SetFloat((b+".innerCos").c_str(),m_SpotLights[i].innerCos);m_Shader.SetFloat((b+".outerCos").c_str(),m_SpotLights[i].outerCos);
	}

	glDrawElements(
		GL_TRIANGLES,
		static_cast<GLsizei>(
			mesh->GetIndexCount()
			),
		GL_UNSIGNED_INT,
		nullptr
	);

	if (texture != nullptr &&
		texture->IsLoaded())
	{
		texture->Unbind();
	}

	m_Shader.Unbind();
	mesh->Unbind();

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
        normalMap, metallicMap, roughnessMap, aoMap, emissiveMap);
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
    DrawMeshInternal(GetModelMesh(modelPath), transform, red, green, blue, alpha,
        texture, metallic, roughness, ambientOcclusion, emissive,
        normalMap, metallicMap, roughnessMap, aoMap, emissiveMap);
}

Texture2D* Renderer::LoadTexture(
	const std::string& filepath)
{
	return m_TextureManager.Load(
		filepath
	);
}
