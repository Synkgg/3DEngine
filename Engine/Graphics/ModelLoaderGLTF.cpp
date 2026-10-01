#include "ModelLoader.h"
#include "ModelAsset.h"
#include "Mesh.h"
#include "../Core/Logger.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
std::array<float,16> MatrixToArray(const cgltf_float* m)
{
    std::array<float,16> out{};
    std::copy(m,m+16,out.begin());
    return out;
}

std::array<float,16> IdentityMatrix()
{
    return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
}

int NodeIndex(const cgltf_data* data,const cgltf_node* node)
{
    return node ? static_cast<int>(node-data->nodes) : -1;
}

int FindBone(const std::unordered_map<int,int>& boneByNode,int node)
{
    const auto it=boneByNode.find(node);
    return it==boneByNode.end()?-1:it->second;
}
}

std::unique_ptr<ModelAsset> LoadGLTFAsset(const std::string& filepath,const ModelImportSettings& settings)
{
    cgltf_options options{};
    cgltf_data* data=nullptr;
    if(cgltf_parse_file(&options,filepath.c_str(),&data)!=cgltf_result_success)
    {
        Logger::Error("Could not parse glTF/GLB: "+filepath);
        return nullptr;
    }
    struct Guard{cgltf_data* p;~Guard(){cgltf_free(p);}} guard{data};
    if(cgltf_load_buffers(&options,data,filepath.c_str())!=cgltf_result_success)
    {
        Logger::Error("Could not load glTF buffers: "+filepath);
        return nullptr;
    }

    auto model=std::make_unique<ModelAsset>();
    model->sourcePath=filepath;
    model->type=data->skins_count>0?ModelAssetType::Skeletal:ModelAssetType::Static;

    ImportedMaterial fallback; fallback.name="Default";
    model->materials.push_back(fallback);
    for(cgltf_size i=0;i<data->materials_count;++i)
    {
        const cgltf_material& src=data->materials[i];
        ImportedMaterial m;
        m.name=src.name?src.name:("Material_"+std::to_string(i));
        if(src.has_pbr_metallic_roughness)
        {
            const auto& p=src.pbr_metallic_roughness;
            m.diffuse[0]=p.base_color_factor[0];m.diffuse[1]=p.base_color_factor[1];m.diffuse[2]=p.base_color_factor[2];
            m.opacity=p.base_color_factor[3];
            // Encode glTF's explicit PBR values into the legacy material fields.
            m.specular[0]=m.specular[1]=m.specular[2]=p.metallic_factor>0.5f?1.0f:0.04f;
            const float r=std::clamp(p.roughness_factor,0.04f,1.0f);
            m.shininess=std::clamp(2.0f/(r*r)-2.0f,0.0f,1000.0f);
            if(settings.importTextures && p.base_color_texture.texture && p.base_color_texture.texture->image && p.base_color_texture.texture->image->uri)
                m.diffuseTexture=(std::filesystem::path(filepath).parent_path()/p.base_color_texture.texture->image->uri).lexically_normal().string();
        }
        model->materials.push_back(std::move(m));
    }

    std::unordered_map<int,int> boneByNode;
    if(data->skins_count>0)
    {
        const cgltf_skin& skin=data->skins[0];
        model->skeleton.bones.resize(skin.joints_count);
        for(cgltf_size i=0;i<skin.joints_count;++i)
        {
            const int ni=NodeIndex(data,skin.joints[i]);
            boneByNode[ni]=static_cast<int>(i);
            model->skeleton.bones[i].name=skin.joints[i]->name?skin.joints[i]->name:("Bone_"+std::to_string(i));
            model->skeleton.bones[i].inverseBindMatrix=IdentityMatrix();
        }
        for(cgltf_size i=0;i<skin.joints_count;++i)
        {
            const cgltf_node* parent=skin.joints[i]->parent;
            model->skeleton.bones[i].parent=FindBone(boneByNode,NodeIndex(data,parent));
        }
        if(skin.inverse_bind_matrices)
        {
            for(cgltf_size i=0;i<skin.joints_count;++i)
            {
                cgltf_float m[16]{};
                cgltf_accessor_read_float(skin.inverse_bind_matrices,i,m,16);
                model->skeleton.bones[i].inverseBindMatrix=MatrixToArray(m);
            }
        }
    }

    for(cgltf_size mi=0;mi<data->meshes_count;++mi)
    {
        const cgltf_mesh& mesh=data->meshes[mi];
        for(cgltf_size pi=0;pi<mesh.primitives_count;++pi)
        {
            const cgltf_primitive& prim=mesh.primitives[pi];
            if(prim.type!=cgltf_primitive_type_triangles) continue;
            const cgltf_accessor* pos=nullptr;const cgltf_accessor* normal=nullptr;const cgltf_accessor* uv=nullptr;
            const cgltf_accessor* joints=nullptr;const cgltf_accessor* weights=nullptr;
            for(cgltf_size ai=0;ai<prim.attributes_count;++ai)
            {
                const cgltf_attribute& a=prim.attributes[ai];
                if(a.type==cgltf_attribute_type_position)pos=a.data;
                else if(a.type==cgltf_attribute_type_normal)normal=a.data;
                else if(a.type==cgltf_attribute_type_texcoord&&a.index==0)uv=a.data;
                else if(a.type==cgltf_attribute_type_joints&&a.index==0)joints=a.data;
                else if(a.type==cgltf_attribute_type_weights&&a.index==0)weights=a.data;
            }
            if(!pos) continue;
            std::vector<Vertex> vertices(pos->count);
            std::vector<BoneWeight> skinWeights(pos->count);
            for(cgltf_size vi=0;vi<pos->count;++vi)
            {
                cgltf_float v[4]{};
                cgltf_accessor_read_float(pos,vi,v,3);vertices[vi].position[0]=v[0];vertices[vi].position[1]=v[1];vertices[vi].position[2]=v[2];
                if(normal){cgltf_accessor_read_float(normal,vi,v,3);vertices[vi].normal[0]=v[0];vertices[vi].normal[1]=v[1];vertices[vi].normal[2]=v[2];}
                if(uv){cgltf_accessor_read_float(uv,vi,v,2);vertices[vi].uv[0]=v[0];vertices[vi].uv[1]=v[1];}
                if(joints){cgltf_uint j[4]{};cgltf_accessor_read_uint(joints,vi,j,4);for(int k=0;k<4;++k)skinWeights[vi].joints[k]=static_cast<std::uint16_t>(j[k]);}
                if(weights){cgltf_accessor_read_float(weights,vi,v,4);for(int k=0;k<4;++k)skinWeights[vi].weights[k]=v[k];}
            }
            std::vector<std::uint32_t> indices;
            if(prim.indices){indices.resize(prim.indices->count);for(cgltf_size ii=0;ii<prim.indices->count;++ii)indices[ii]=static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices,ii));}
            else{indices.resize(pos->count);for(std::uint32_t ii=0;ii<indices.size();++ii)indices[ii]=ii;}
            MeshSection section;
            section.name=mesh.name?mesh.name:("Mesh_"+std::to_string(mi)+"_"+std::to_string(pi));
            section.materialIndex=prim.material?static_cast<std::uint32_t>((prim.material-data->materials)+1):0u;
            section.mesh=std::make_unique<Mesh>(vertices,indices);
            model->sections.push_back(std::move(section));
            model->skinWeights.push_back(std::move(skinWeights));
        }
    }

    for(cgltf_size ai=0;ai<data->animations_count;++ai)
    {
        const cgltf_animation& src=data->animations[ai];
        AnimationClip clip;clip.name=src.name?src.name:("Animation_"+std::to_string(ai));
        for(cgltf_size ci=0;ci<src.channels_count;++ci)
        {
            const cgltf_animation_channel& ch=src.channels[ci];
            if(!ch.target_node||!ch.sampler||!ch.sampler->input||!ch.sampler->output) continue;
            const int bone=FindBone(boneByNode,NodeIndex(data,ch.target_node));
            if(bone<0) continue;
            AnimationChannel channel;channel.bone=bone;
            for(cgltf_size ki=0;ki<ch.sampler->input->count;++ki)
            {
                cgltf_float time=0;cgltf_accessor_read_float(ch.sampler->input,ki,&time,1);clip.duration=std::max(clip.duration,time);
                cgltf_float value[4]{};
                if(ch.target_path==cgltf_animation_path_type_rotation)
                {
                    cgltf_accessor_read_float(ch.sampler->output,ki,value,4);
                    channel.rotations.push_back({time,{value[0],value[1],value[2],value[3]}});
                }
                else if(ch.target_path==cgltf_animation_path_type_translation||ch.target_path==cgltf_animation_path_type_scale)
                {
                    cgltf_accessor_read_float(ch.sampler->output,ki,value,3);
                    AnimationKeyVec3 key{time,{value[0],value[1],value[2]}};
                    if(ch.target_path==cgltf_animation_path_type_translation)channel.translations.push_back(key);else channel.scales.push_back(key);
                }
            }
            clip.channels.push_back(std::move(channel));
        }
        model->animations.push_back(std::move(clip));
    }

    if(model->sections.empty()){Logger::Error("glTF contains no renderable triangle meshes: "+filepath);return nullptr;}
    Logger::Info("Loaded glTF model: "+filepath+" ("+std::to_string(model->sections.size())+" sections, "+std::to_string(model->skeleton.bones.size())+" bones, "+std::to_string(model->animations.size())+" animations)");
    return model;
}
