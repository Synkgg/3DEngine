#include "ModelLoader.h"
#include "ModelAsset.h"
#include "Mesh.h"
#include "../Core/Logger.h"
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
std::array<float,16> M(const cgltf_float* m){std::array<float,16> o{};std::copy(m,m+16,o.begin());return o;}
std::array<float,16> I(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
int NI(const cgltf_data* d,const cgltf_node* n){return n?(int)(n-d->nodes):-1;}
std::string TexturePath(const std::string& file,const cgltf_texture_view& view){
 if(!view.texture||!view.texture->image||!view.texture->image->uri)return {};
 const std::string uri=view.texture->image->uri;if(uri.rfind("data:",0)==0)return {};
 return (std::filesystem::path(file).parent_path()/uri).lexically_normal().string();
}
AnimationInterpolation Interp(cgltf_interpolation_type t){
 if(t==cgltf_interpolation_type_step)return AnimationInterpolation::Step;
 if(t==cgltf_interpolation_type_cubic_spline)return AnimationInterpolation::CubicSpline;
 return AnimationInterpolation::Linear;
}
void TransformStaticVertices(const cgltf_node* node,std::vector<Vertex>& vertices){
 cgltf_float w[16]{};cgltf_node_transform_world(node,w);
 for(Vertex& v:vertices){
  const float x=v.position[0],y=v.position[1],z=v.position[2];
  v.position[0]=w[0]*x+w[4]*y+w[8]*z+w[12];v.position[1]=w[1]*x+w[5]*y+w[9]*z+w[13];v.position[2]=w[2]*x+w[6]*y+w[10]*z+w[14];
  const float nx=v.normal[0],ny=v.normal[1],nz=v.normal[2];
  float tx=w[0]*nx+w[4]*ny+w[8]*nz,ty=w[1]*nx+w[5]*ny+w[9]*nz,tz=w[2]*nx+w[6]*ny+w[10]*nz,l=std::sqrt(tx*tx+ty*ty+tz*tz);
  if(l>0.000001f){v.normal[0]=tx/l;v.normal[1]=ty/l;v.normal[2]=tz/l;}
 }
}
}

std::unique_ptr<ModelAsset> LoadGLTFAsset(const std::string& filepath,const ModelImportSettings& settings){
 cgltf_options options{};cgltf_data* data=nullptr;
 if(cgltf_parse_file(&options,filepath.c_str(),&data)!=cgltf_result_success){Logger::Error("Could not parse glTF/GLB: "+filepath);return nullptr;}
 struct G{cgltf_data* p;~G(){cgltf_free(p);}} guard{data};
 if(cgltf_load_buffers(&options,data,filepath.c_str())!=cgltf_result_success){Logger::Error("Could not load glTF buffers: "+filepath);return nullptr;}
 auto model=std::make_unique<ModelAsset>();model->sourcePath=filepath;model->type=data->skins_count?ModelAssetType::Skeletal:ModelAssetType::Static;
 ImportedMaterial fallback;fallback.name="Default";model->materials.push_back(fallback);
 if(settings.importMaterials)for(cgltf_size i=0;i<data->materials_count;++i){
  const cgltf_material& s=data->materials[i];ImportedMaterial m;m.name=s.name?s.name:("Material_"+std::to_string(i));
  if(s.has_pbr_metallic_roughness){const auto& p=s.pbr_metallic_roughness;m.diffuse[0]=p.base_color_factor[0];m.diffuse[1]=p.base_color_factor[1];m.diffuse[2]=p.base_color_factor[2];m.opacity=p.base_color_factor[3];m.metallicFactor=p.metallic_factor;m.roughnessFactor=p.roughness_factor;
   if(settings.importTextures){m.diffuseTexture=TexturePath(filepath,p.base_color_texture);m.metallicRoughnessTexture=TexturePath(filepath,p.metallic_roughness_texture);}}
  m.emissiveFactor[0]=s.emissive_factor[0];m.emissiveFactor[1]=s.emissive_factor[1];m.emissiveFactor[2]=s.emissive_factor[2];
  if(settings.importTextures){m.normalTexture=TexturePath(filepath,s.normal_texture);m.occlusionTexture=TexturePath(filepath,s.occlusion_texture);m.emissiveTexture=TexturePath(filepath,s.emissive_texture);}
  model->materials.push_back(std::move(m));
 }
 std::unordered_set<int> skeletonNodes;
 for(cgltf_size si=0;si<data->skins_count;++si)for(cgltf_size ji=0;ji<data->skins[si].joints_count;++ji)for(const cgltf_node* n=data->skins[si].joints[ji];n;n=n->parent)skeletonNodes.insert(NI(data,n));
 std::unordered_map<int,int> boneByNode;
 for(cgltf_size ni=0;ni<data->nodes_count;++ni)if(skeletonNodes.count((int)ni)){boneByNode[(int)ni]=(int)model->skeleton.bones.size();Bone b;b.name=data->nodes[ni].name?data->nodes[ni].name:("Bone_"+std::to_string(ni));cgltf_float local[16]{};cgltf_node_transform_local(&data->nodes[ni],local);b.bindLocalMatrix=M(local);if(data->nodes[ni].has_translation)std::copy(data->nodes[ni].translation,data->nodes[ni].translation+3,b.bindTranslation.begin());if(data->nodes[ni].has_rotation)std::copy(data->nodes[ni].rotation,data->nodes[ni].rotation+4,b.bindRotation.begin());if(data->nodes[ni].has_scale)std::copy(data->nodes[ni].scale,data->nodes[ni].scale+3,b.bindScale.begin());model->skeleton.bones.push_back(b);}
 for(const auto& x:boneByNode){const cgltf_node& n=data->nodes[x.first];auto p=boneByNode.find(NI(data,n.parent));model->skeleton.bones[x.second].parent=p==boneByNode.end()?-1:p->second;}
 for(cgltf_size si=0;si<data->skins_count;++si){const cgltf_skin& skin=data->skins[si];if(!skin.inverse_bind_matrices)continue;for(cgltf_size ji=0;ji<skin.joints_count;++ji){auto it=boneByNode.find(NI(data,skin.joints[ji]));if(it==boneByNode.end())continue;cgltf_float x[16]{};cgltf_accessor_read_float(skin.inverse_bind_matrices,ji,x,16);model->skeleton.bones[it->second].inverseBindMatrix=M(x);}}
 if(model->skeleton.bones.size()>128)Logger::Warning("glTF skeleton has "+std::to_string(model->skeleton.bones.size())+" bones; influences above the 128-bone GPU limit will be discarded.");

 for(cgltf_size ni=0;ni<data->nodes_count;++ni){const cgltf_node& node=data->nodes[ni];if(!node.mesh)continue;const cgltf_skin* skin=node.skin;
  for(cgltf_size pi=0;pi<node.mesh->primitives_count;++pi){const cgltf_primitive& prim=node.mesh->primitives[pi];if(prim.type!=cgltf_primitive_type_triangles)continue;
   const cgltf_accessor *pos=nullptr,*normal=nullptr,*uv=nullptr,*joints=nullptr,*weights=nullptr;
   for(cgltf_size ai=0;ai<prim.attributes_count;++ai){const auto& a=prim.attributes[ai];if(a.type==cgltf_attribute_type_position)pos=a.data;else if(a.type==cgltf_attribute_type_normal)normal=a.data;else if(a.type==cgltf_attribute_type_texcoord&&a.index==0)uv=a.data;else if(a.type==cgltf_attribute_type_joints&&a.index==0)joints=a.data;else if(a.type==cgltf_attribute_type_weights&&a.index==0)weights=a.data;}if(!pos)continue;
   std::vector<Vertex> vertices(pos->count);std::vector<BoneWeight> sw(pos->count);
   for(cgltf_size vi=0;vi<pos->count;++vi){cgltf_float v[4]{};cgltf_accessor_read_float(pos,vi,v,3);std::copy(v,v+3,vertices[vi].position);if(normal){cgltf_accessor_read_float(normal,vi,v,3);std::copy(v,v+3,vertices[vi].normal);}if(uv){cgltf_accessor_read_float(uv,vi,v,2);std::copy(v,v+2,vertices[vi].uv);}
    if(skin&&joints&&weights){cgltf_uint j[4]{};cgltf_accessor_read_uint(joints,vi,j,4);cgltf_accessor_read_float(weights,vi,v,4);float sum=0;for(int k=0;k<4;++k){int mapped=-1;if(j[k]<skin->joints_count){auto it=boneByNode.find(NI(data,skin->joints[j[k]]));if(it!=boneByNode.end()&&it->second<128)mapped=it->second;}sw[vi].joints[k]=(std::uint16_t)std::max(mapped,0);sw[vi].weights[k]=mapped>=0?std::max(v[k],0.0f):0.0f;sum+=sw[vi].weights[k];}if(sum>0.000001f)for(float& w:sw[vi].weights)w/=sum;else{sw[vi].joints[0]=0;sw[vi].weights[0]=1.0f;}}
   }
   if(!skin)TransformStaticVertices(&node,vertices);
   std::vector<std::uint32_t> indices;if(prim.indices){indices.resize(prim.indices->count);for(cgltf_size ii=0;ii<prim.indices->count;++ii)indices[ii]=(std::uint32_t)cgltf_accessor_read_index(prim.indices,ii);}else{indices.resize(pos->count);for(std::uint32_t ii=0;ii<indices.size();++ii)indices[ii]=ii;}
   MeshSection section;section.name=node.name?node.name:(node.mesh->name?node.mesh->name:("Mesh_"+std::to_string(ni)+"_"+std::to_string(pi)));section.materialIndex=settings.importMaterials&&prim.material?(std::uint32_t)((prim.material-data->materials)+1):0;section.mesh=std::make_unique<Mesh>(vertices,indices);if(skin&&joints&&weights)section.mesh->SetSkinWeights(sw);model->sections.push_back(std::move(section));model->skinWeights.push_back(skin&&joints&&weights?std::move(sw):std::vector<BoneWeight>{});
  }
 }
 for(cgltf_size ai=0;ai<data->animations_count;++ai){const cgltf_animation& src=data->animations[ai];AnimationClip clip;clip.name=src.name?src.name:("Animation_"+std::to_string(ai));
  for(cgltf_size ci=0;ci<src.channels_count;++ci){const auto& ch=src.channels[ci];if(!ch.target_node||!ch.sampler||!ch.sampler->input||!ch.sampler->output)continue;auto bi=boneByNode.find(NI(data,ch.target_node));if(bi==boneByNode.end())continue;AnimationChannel channel;channel.bone=bi->second;const AnimationInterpolation interpolation=Interp(ch.sampler->interpolation);const bool cubic=interpolation==AnimationInterpolation::CubicSpline;
   if(ch.target_path==cgltf_animation_path_type_translation)channel.translationInterpolation=interpolation;else if(ch.target_path==cgltf_animation_path_type_rotation)channel.rotationInterpolation=interpolation;else if(ch.target_path==cgltf_animation_path_type_scale)channel.scaleInterpolation=interpolation;else continue;
   for(cgltf_size ki=0;ki<ch.sampler->input->count;++ki){cgltf_float time=0;cgltf_accessor_read_float(ch.sampler->input,ki,&time,1);clip.duration=std::max(clip.duration,time);const cgltf_size valueIndex=cubic?ki*3+1:ki;cgltf_float value[4]{},inT[4]{},outT[4]{};const cgltf_size comps=ch.target_path==cgltf_animation_path_type_rotation?4:3;cgltf_accessor_read_float(ch.sampler->output,valueIndex,value,comps);if(cubic){cgltf_accessor_read_float(ch.sampler->output,ki*3,inT,comps);cgltf_accessor_read_float(ch.sampler->output,ki*3+2,outT,comps);}
    if(ch.target_path==cgltf_animation_path_type_rotation){AnimationKeyQuat k;k.time=time;std::copy(value,value+4,k.value.begin());std::copy(inT,inT+4,k.inTangent.begin());std::copy(outT,outT+4,k.outTangent.begin());channel.rotations.push_back(k);}else{AnimationKeyVec3 k;k.time=time;std::copy(value,value+3,k.value.begin());std::copy(inT,inT+3,k.inTangent.begin());std::copy(outT,outT+3,k.outTangent.begin());if(ch.target_path==cgltf_animation_path_type_translation)channel.translations.push_back(k);else channel.scales.push_back(k);}
   }clip.channels.push_back(std::move(channel));
  }model->animations.push_back(std::move(clip));
 }
 if(model->sections.empty()){Logger::Error("glTF contains no renderable triangle meshes: "+filepath);return nullptr;}Logger::Info("Loaded glTF model: "+filepath+" ("+std::to_string(model->sections.size())+" sections, "+std::to_string(model->skeleton.bones.size())+" bones, "+std::to_string(model->animations.size())+" animations)");return model;
}