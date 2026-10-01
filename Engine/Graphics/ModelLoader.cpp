#include "ModelLoader.h"
#include "ModelAsset.h"
#include "Mesh.h"
#include "../Core/Logger.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <iomanip>

namespace
{
struct V3 { float x=0,y=0,z=0; };
struct V2 { float x=0,y=0; };
struct Ref { int p=0,t=0,n=0; };

Ref ParseRef(const std::string& s)
{
    Ref r; std::stringstream ss(s); std::string x;
    if (std::getline(ss,x,'/')) r.p=x.empty()?0:std::stoi(x);
    if (std::getline(ss,x,'/')) r.t=x.empty()?0:std::stoi(x);
    if (std::getline(ss,x,'/')) r.n=x.empty()?0:std::stoi(x);
    return r;
}

int Resolve(int i,int size) { return i>0?i-1:(i<0?size+i:-1); }

float LegacyMTLColor(float value)
{
    // These Blender-era MTL files store very dark linear-looking Kd values.
    // The engine's base-color path expects display-space values, so convert
    // them before lighting rather than rendering the asset nearly black.
    return std::pow(std::clamp(value,0.0f,1.0f),1.0f/2.2f);
}

struct P2 { float x=0.0f,y=0.0f; };

float Cross2(const P2& a,const P2& b,const P2& c)
{
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}

bool PointInTriangle(const P2& p,const P2& a,const P2& b,const P2& c,float winding)
{
    const float e0=Cross2(a,b,p)*winding;
    const float e1=Cross2(b,c,p)*winding;
    const float e2=Cross2(c,a,p)*winding;
    constexpr float eps=-0.000001f;
    return e0>=eps&&e1>=eps&&e2>=eps;
}

std::vector<std::uint32_t> TriangulateFace(const std::vector<std::string>& face,const std::vector<V3>& positions)
{
    std::vector<std::uint32_t> result;
    if(face.size()<3) return result;
    if(face.size()==3) return {0,1,2};

    std::vector<V3> points;
    points.reserve(face.size());
    for(const std::string& token:face)
    {
        const Ref r=ParseRef(token);
        const int pi=Resolve(r.p,(int)positions.size());
        if(pi<0||pi>=(int)positions.size()) return {};
        points.push_back(positions[pi]);
    }

    // Newell normal gives a stable projection axis for arbitrary OBJ n-gons.
    V3 n{};
    for(size_t i=0;i<points.size();++i)
    {
        const V3& a=points[i];
        const V3& c=points[(i+1)%points.size()];
        n.x+=(a.y-c.y)*(a.z+c.z);
        n.y+=(a.z-c.z)*(a.x+c.x);
        n.z+=(a.x-c.x)*(a.y+c.y);
    }
    const float ax=std::abs(n.x),ay=std::abs(n.y),az=std::abs(n.z);
    int drop=2;
    if(ax>=ay&&ax>=az) drop=0;
    else if(ay>=az) drop=1;

    std::vector<P2> projected(points.size());
    for(size_t i=0;i<points.size();++i)
    {
        if(drop==0) projected[i]={points[i].y,points[i].z};
        else if(drop==1) projected[i]={points[i].x,points[i].z};
        else projected[i]={points[i].x,points[i].y};
    }

    float area=0.0f;
    for(size_t i=0;i<projected.size();++i)
    {
        const P2& a=projected[i];const P2& c=projected[(i+1)%projected.size()];
        area+=a.x*c.y-c.x*a.y;
    }
    const float winding=area>=0.0f?1.0f:-1.0f;

    std::vector<std::uint32_t> remaining(face.size());
    for(std::uint32_t i=0;i<(std::uint32_t)face.size();++i) remaining[i]=i;

    size_t guard=0;
    while(remaining.size()>3&&guard++<face.size()*face.size())
    {
        bool clipped=false;
        for(size_t i=0;i<remaining.size();++i)
        {
            const std::uint32_t ia=remaining[(i+remaining.size()-1)%remaining.size()];
            const std::uint32_t ib=remaining[i];
            const std::uint32_t ic=remaining[(i+1)%remaining.size()];
            const P2& a=projected[ia];const P2& bb=projected[ib];const P2& c=projected[ic];
            if(Cross2(a,bb,c)*winding<=0.000001f) continue;

            bool contains=false;
            for(std::uint32_t p:remaining)
            {
                if(p==ia||p==ib||p==ic) continue;
                if(PointInTriangle(projected[p],a,bb,c,winding)){contains=true;break;}
            }
            if(contains) continue;

            result.push_back(ia);result.push_back(ib);result.push_back(ic);
            remaining.erase(remaining.begin()+i);
            clipped=true;
            break;
        }
        if(!clipped) break;
    }

    if(remaining.size()==3)
    {
        result.push_back(remaining[0]);result.push_back(remaining[1]);result.push_back(remaining[2]);
        return result;
    }

    // Degenerate/non-planar fallback: retain the old behavior instead of dropping a face.
    result.clear();
    for(std::uint32_t i=1;i+1<(std::uint32_t)face.size();++i)
    {
        result.push_back(0);result.push_back(i);result.push_back(i+1);
    }
    return result;
}

std::string Trim(const std::string& value)
{
    const auto first=value.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return {};
    const auto last=value.find_last_not_of(" \t\r\n");
    return value.substr(first,last-first+1);
}

void GenerateNormals(std::vector<Vertex>& vertices,const std::vector<std::uint32_t>& indices)
{
    bool hasNormals=false;
    for(const Vertex& v:vertices)
    {
        const float l=v.normal[0]*v.normal[0]+v.normal[1]*v.normal[1]+v.normal[2]*v.normal[2];
        if(l>0.000001f){hasNormals=true;break;}
    }
    if(hasNormals) return;

    for(Vertex& v:vertices){v.normal[0]=v.normal[1]=v.normal[2]=0.0f;}
    for(size_t i=0;i+2<indices.size();i+=3)
    {
        Vertex& a=vertices[indices[i]]; Vertex& b=vertices[indices[i+1]]; Vertex& c=vertices[indices[i+2]];
        const float abx=b.position[0]-a.position[0],aby=b.position[1]-a.position[1],abz=b.position[2]-a.position[2];
        const float acx=c.position[0]-a.position[0],acy=c.position[1]-a.position[1],acz=c.position[2]-a.position[2];
        const float nx=aby*acz-abz*acy,ny=abz*acx-abx*acz,nz=abx*acy-aby*acx;
        for(Vertex* v:{&a,&b,&c}){v->normal[0]+=nx;v->normal[1]+=ny;v->normal[2]+=nz;}
    }
    for(Vertex& v:vertices)
    {
        const float l=std::sqrt(v.normal[0]*v.normal[0]+v.normal[1]*v.normal[1]+v.normal[2]*v.normal[2]);
        if(l>0.000001f){v.normal[0]/=l;v.normal[1]/=l;v.normal[2]/=l;} else v.normal[1]=1.0f;
    }
}

void LoadMTL(const std::filesystem::path& path,std::vector<ImportedMaterial>& out,bool importTextures)
{
    std::ifstream file(path);
    if(!file){Logger::Warning("Could not open MTL: "+path.string());return;}

    ImportedMaterial* current=nullptr;
    std::string line;
    while(std::getline(file,line))
    {
        std::stringstream ss(line); std::string tag; ss>>tag;
        if(tag.empty()||tag[0]=='#') continue;
        if(tag=="newmtl")
        {
            std::string name;std::getline(ss,name);
            out.emplace_back();out.back().name=Trim(name);current=&out.back();
        }
        else if(current&&tag=="Kd")
        {
            float r=1.0f,g=1.0f,b=1.0f;ss>>r>>g>>b;
            current->diffuse[0]=LegacyMTLColor(r);
            current->diffuse[1]=LegacyMTLColor(g);
            current->diffuse[2]=LegacyMTLColor(b);
        }
        else if(current&&tag=="Ks") ss>>current->specular[0]>>current->specular[1]>>current->specular[2];
        else if(current&&tag=="Ns") ss>>current->shininess;
        else if(current&&tag=="d") ss>>current->opacity;
        else if(current&&tag=="Tr"){float tr=0.0f;ss>>tr;current->opacity=1.0f-tr;}
        else if(current&&tag=="map_Kd")
        {
            std::string texture;std::getline(ss,texture);
            texture=Trim(texture);
            if(importTextures && !texture.empty()) current->diffuseTexture=(path.parent_path()/texture).lexically_normal().string();
        }
    }
}

struct SectionBuilder
{
    std::string material;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::unordered_map<std::string,std::uint32_t> cache;
};
}

float ImportedMaterial::Roughness() const
{
    // Legacy MTL Ns describes specular highlight size, not PBR metalness.
    // Keep the conversion deliberately broad so old Blender materials retain
    // their diffuse colour instead of collapsing into chrome-like surfaces.
    if(roughnessFactor>=0.0f) return std::clamp(roughnessFactor,0.04f,1.0f);
    const float ns=std::clamp(shininess,0.0f,1000.0f);
    return std::clamp(0.92f-0.55f*std::sqrt(ns/1000.0f),0.28f,0.92f);
}

float ImportedMaterial::Metallic() const
{
    if(metallicFactor>=0.0f) return std::clamp(metallicFactor,0.0f,1.0f);
    // Wavefront Ks is specular reflectance and cannot reliably identify
    // whether a surface is a metal. Treat legacy OBJ/MTL materials as
    // dielectric by default; explicit PBR assets/maps can provide metalness.
    return 0.0f;
}

std::unique_ptr<ModelAsset> ModelLoader::LoadModel(const std::string& filepath,const ModelImportSettings& settings)
{
    const std::string ext=std::filesystem::path(filepath).extension().string();
    std::string lower=ext;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return (char)std::tolower(c);});
    if(lower==".modelasset") return LoadImportedAsset(filepath);
    if(lower==".obj") return LoadOBJModel(filepath,settings);
    if(lower==".gltf"||lower==".glb") return LoadGLTFModel(filepath,settings);
    Logger::Error("Unsupported model format: "+filepath+" (OBJ is enabled; glTF/GLB skeletal data structures are ready for the next importer backend)");
    return nullptr;
}

std::unique_ptr<ModelAsset> ModelLoader::LoadOBJModel(const std::string& filepath,const ModelImportSettings& settings)
{
    std::ifstream f(filepath);
    if(!f){Logger::Error("Could not open OBJ: "+filepath);return nullptr;}

    auto model=std::make_unique<ModelAsset>();
    model->sourcePath=filepath;

    ImportedMaterial fallback;fallback.name="Default";
    model->materials.push_back(fallback);

    std::vector<V3> positions,normals;std::vector<V2> uvs;
    std::vector<SectionBuilder> builders;
    std::unordered_map<std::string,size_t> builderByMaterial;
    std::string currentMaterial="Default";

    SectionBuilder* activeBuilder=nullptr;
    auto getBuilder=[&]() -> SectionBuilder& {
        if(settings.mergeMaterialSections)
        {
            auto it=builderByMaterial.find(currentMaterial);
            if(it!=builderByMaterial.end()) return builders[it->second];
            const size_t index=builders.size();
            builders.push_back({});
            builders.back().material=currentMaterial;
            builderByMaterial[currentMaterial]=index;
            return builders.back();
        }
        if(!activeBuilder)
        {
            builders.push_back({});
            builders.back().material=currentMaterial;
            activeBuilder=&builders.back();
        }
        return *activeBuilder;
    };

    std::string line;
    while(std::getline(f,line))
    {
        std::stringstream ss(line);std::string tag;ss>>tag;
        if(tag.empty()||tag[0]=='#') continue;
        if(tag=="v"){V3 x;ss>>x.x>>x.y>>x.z;positions.push_back(x);}
        else if(tag=="vt"){V2 x;ss>>x.x>>x.y;uvs.push_back(x);}
        else if(tag=="vn"){V3 x;ss>>x.x>>x.y>>x.z;normals.push_back(x);}
        else if(tag=="mtllib"&&settings.importMaterials)
        {
            std::string mtl;std::getline(ss,mtl);mtl=Trim(mtl);
            if(!mtl.empty()) LoadMTL((std::filesystem::path(filepath).parent_path()/mtl).lexically_normal(),model->materials,settings.importTextures);
        }
        else if(tag=="usemtl")
        {
            std::string name;std::getline(ss,name);name=Trim(name);currentMaterial=name.empty()?"Default":name;
            activeBuilder=nullptr;
        }
        else if(tag=="f")
        {
            std::vector<std::string> face;std::string x;while(ss>>x)face.push_back(x);
            if(face.size()<3) continue;
            SectionBuilder& b=getBuilder();
            auto emit=[&](const std::string& key)->std::uint32_t{
                auto it=b.cache.find(key);if(it!=b.cache.end())return it->second;
                Ref r=ParseRef(key);Vertex v{};
                int pi=Resolve(r.p,(int)positions.size()),ti=Resolve(r.t,(int)uvs.size()),ni=Resolve(r.n,(int)normals.size());
                if(pi>=0){v.position[0]=positions[pi].x;v.position[1]=positions[pi].y;v.position[2]=positions[pi].z;}
                if(ti>=0){v.uv[0]=uvs[ti].x;v.uv[1]=uvs[ti].y;}
                if(ni>=0){v.normal[0]=normals[ni].x;v.normal[1]=normals[ni].y;v.normal[2]=normals[ni].z;}
                const auto id=(std::uint32_t)b.vertices.size();b.vertices.push_back(v);b.cache[key]=id;return id;
            };
            const std::vector<std::uint32_t> triangles=TriangulateFace(face,positions);
            for(std::uint32_t corner:triangles) b.indices.push_back(emit(face[corner]));
        }
    }

    std::unordered_map<std::string,std::uint32_t> materialIndices;
    for(std::uint32_t i=0;i<(std::uint32_t)model->materials.size();++i) materialIndices[model->materials[i].name]=i;

    size_t triangleCount=0;
    for(SectionBuilder& b:builders)
    {
        if(b.vertices.empty()||b.indices.empty()) continue;
        if(settings.generateNormals) GenerateNormals(b.vertices,b.indices);
        auto materialIt=materialIndices.find(b.material);
        std::uint32_t materialIndex=materialIt==materialIndices.end()?0u:materialIt->second;
        triangleCount+=b.indices.size()/3;
        MeshSection section;section.name=b.material;section.materialIndex=materialIndex;
        section.mesh=std::make_unique<Mesh>(b.vertices,b.indices);
        model->sections.push_back(std::move(section));
    }

    if(model->sections.empty()){Logger::Error("OBJ contains no renderable faces: "+filepath);return nullptr;}
    Logger::Info("Loaded OBJ model: "+filepath+" ("+std::to_string(model->sections.size())+" sections, "+std::to_string(model->materials.size())+" materials, "+std::to_string(triangleCount)+" triangles)");
    return model;
}

std::unique_ptr<ModelAsset> ModelLoader::LoadGLTFModel(const std::string& filepath,const ModelImportSettings& settings)
{
    extern std::unique_ptr<ModelAsset> LoadGLTFAsset(const std::string&,const ModelImportSettings&);
    return LoadGLTFAsset(filepath,settings);
}

std::unique_ptr<Mesh> ModelLoader::LoadOBJ(const std::string& filepath)
{
    auto model=LoadOBJModel(filepath,{});
    if(!model||model->sections.empty()) return nullptr;
    if(model->sections.size()==1) return std::move(model->sections.front().mesh);

    std::vector<Vertex> vertices;std::vector<std::uint32_t> indices;
    for(auto& section:model->sections)
    {
        const std::uint32_t base=(std::uint32_t)vertices.size();
        const auto& sv=section.mesh->GetVertices();const auto& si=section.mesh->GetIndices();
        vertices.insert(vertices.end(),sv.begin(),sv.end());
        for(std::uint32_t index:si) indices.push_back(base+index);
    }
    return std::make_unique<Mesh>(vertices,indices);
}
namespace {
template<class T> bool WritePod(std::ofstream& out,const T& v){out.write(reinterpret_cast<const char*>(&v),sizeof(T));return (bool)out;}
template<class T> bool ReadPod(std::ifstream& in,T& v){in.read(reinterpret_cast<char*>(&v),sizeof(T));return (bool)in;}
bool WriteString(std::ofstream& out,const std::string& s){std::uint32_t n=(std::uint32_t)s.size();return WritePod(out,n)&&(out.write(s.data(),n),(bool)out);}
bool ReadString(std::ifstream& in,std::string& s){std::uint32_t n=0;if(!ReadPod(in,n)||n>16*1024*1024)return false;s.resize(n);in.read(s.data(),n);return (bool)in;}
}
bool ModelLoader::SaveImportedAsset(const std::string& filepath,const ModelAsset& asset,const ModelImportSettings& settings)
{
    std::ofstream out(filepath,std::ios::binary);if(!out){Logger::Error("Could not create imported model asset: "+filepath);return false;}
    const char magic[8]={'S','3','D','M','D','L','1','\0'};out.write(magic,8);std::uint32_t version=2;WritePod(out,version);WriteString(out,asset.sourcePath);
    std::uint8_t flags=(settings.generateNormals?1:0)|(settings.importMaterials?2:0)|(settings.importTextures?4:0)|(settings.mergeMaterialSections?8:0);WritePod(out,flags);std::uint8_t type=(std::uint8_t)asset.type;WritePod(out,type);
    std::uint32_t mc=(std::uint32_t)asset.materials.size();WritePod(out,mc);for(const auto& m:asset.materials){WriteString(out,m.name);out.write((const char*)m.diffuse,sizeof(m.diffuse));out.write((const char*)m.specular,sizeof(m.specular));WritePod(out,m.shininess);WritePod(out,m.opacity);WriteString(out,m.diffuseTexture);WritePod(out,m.metallicFactor);WritePod(out,m.roughnessFactor);WriteString(out,m.normalTexture);WriteString(out,m.metallicRoughnessTexture);WriteString(out,m.occlusionTexture);WriteString(out,m.emissiveTexture);out.write((const char*)m.emissiveFactor,sizeof(m.emissiveFactor));}
    std::uint32_t sc=(std::uint32_t)asset.sections.size();WritePod(out,sc);for(std::uint32_t i=0;i<sc;++i){const auto& s=asset.sections[i];WriteString(out,s.name);WritePod(out,s.materialIndex);const auto& v=s.mesh->GetVertices();const auto& ix=s.mesh->GetIndices();std::uint32_t vc=(std::uint32_t)v.size(),ic=(std::uint32_t)ix.size();WritePod(out,vc);WritePod(out,ic);out.write((const char*)v.data(),v.size()*sizeof(Vertex));out.write((const char*)ix.data(),ix.size()*sizeof(std::uint32_t));const auto& sw=i<asset.skinWeights.size()?asset.skinWeights[i]:std::vector<BoneWeight>{};std::uint32_t wc=(std::uint32_t)sw.size();WritePod(out,wc);out.write((const char*)sw.data(),sw.size()*sizeof(BoneWeight));}
    std::uint32_t bc=(std::uint32_t)asset.skeleton.bones.size();WritePod(out,bc);for(const auto& bone:asset.skeleton.bones){WriteString(out,bone.name);WritePod(out,bone.parent);out.write((const char*)bone.bindTranslation.data(),sizeof(float)*3);out.write((const char*)bone.bindRotation.data(),sizeof(float)*4);out.write((const char*)bone.bindScale.data(),sizeof(float)*3);out.write((const char*)bone.bindLocalMatrix.data(),sizeof(float)*16);out.write((const char*)bone.inverseBindMatrix.data(),sizeof(float)*16);}
    std::uint32_t ac=(std::uint32_t)asset.animations.size();WritePod(out,ac);for(const auto& clip:asset.animations){WriteString(out,clip.name);WritePod(out,clip.duration);std::uint32_t cc=(std::uint32_t)clip.channels.size();WritePod(out,cc);for(const auto& ch:clip.channels){WritePod(out,ch.bone);WritePod(out,ch.translationInterpolation);WritePod(out,ch.rotationInterpolation);WritePod(out,ch.scaleInterpolation);auto w3=[&](const auto& keys){std::uint32_t n=(std::uint32_t)keys.size();WritePod(out,n);for(const auto& k:keys){WritePod(out,k.time);out.write((const char*)k.value.data(),sizeof(k.value));out.write((const char*)k.inTangent.data(),sizeof(k.inTangent));out.write((const char*)k.outTangent.data(),sizeof(k.outTangent));}};w3(ch.translations);w3(ch.rotations);w3(ch.scales);}}
    if(!out){Logger::Error("Failed writing imported model asset: "+filepath);return false;}Logger::Info("Saved imported model asset: "+filepath);return true;
}
bool ModelLoader::IsImportedAssetCurrent(const std::string& filepath)
{
    std::ifstream in(filepath,std::ios::binary);if(!in)return false;char magic[8]{};in.read(magic,8);std::uint32_t version=0;return std::string(magic,7)=="S3DMDL1"&&ReadPod(in,version)&&version==2;
}
bool ModelLoader::ReadImportedAssetSettings(const std::string& filepath,ModelImportSettings& settings,std::string* sourcePath)
{
    std::ifstream in(filepath,std::ios::binary);if(!in)return false;char magic[8]{};in.read(magic,8);std::uint32_t version=0;if(std::string(magic,7)!="S3DMDL1"||!ReadPod(in,version)||(version!=1&&version!=2))return false;
    std::string storedSource;if(!ReadString(in,storedSource))return false;std::uint8_t flags=0;if(!ReadPod(in,flags))return false;
    settings.generateNormals=(flags&1)!=0;settings.importMaterials=(flags&2)!=0;settings.importTextures=(flags&4)!=0;settings.mergeMaterialSections=(flags&8)!=0;if(sourcePath)*sourcePath=std::move(storedSource);return true;
}
std::unique_ptr<ModelAsset> ModelLoader::LoadImportedAsset(const std::string& filepath)
{
    std::ifstream in(filepath,std::ios::binary);if(!in)return nullptr;char magic[8]{};in.read(magic,8);std::uint32_t version=0;if(std::string(magic,7)!="S3DMDL1"||!ReadPod(in,version)||(version!=1&&version!=2)){Logger::Error("Invalid imported model asset: "+filepath);return nullptr;}
    auto a=std::make_unique<ModelAsset>();ReadString(in,a->sourcePath);std::uint8_t flags=0,type=0;ReadPod(in,flags);ReadPod(in,type);a->type=(ModelAssetType)type;
    std::uint32_t mc=0;ReadPod(in,mc);a->materials.resize(mc);for(auto& m:a->materials){ReadString(in,m.name);in.read((char*)m.diffuse,sizeof(m.diffuse));in.read((char*)m.specular,sizeof(m.specular));ReadPod(in,m.shininess);ReadPod(in,m.opacity);ReadString(in,m.diffuseTexture);if(version>=2){ReadPod(in,m.metallicFactor);ReadPod(in,m.roughnessFactor);ReadString(in,m.normalTexture);ReadString(in,m.metallicRoughnessTexture);ReadString(in,m.occlusionTexture);ReadString(in,m.emissiveTexture);in.read((char*)m.emissiveFactor,sizeof(m.emissiveFactor));}}
    std::uint32_t sc=0;ReadPod(in,sc);a->skinWeights.resize(sc);for(std::uint32_t i=0;i<sc;++i){MeshSection s;ReadString(in,s.name);ReadPod(in,s.materialIndex);std::uint32_t vc=0,ic=0;ReadPod(in,vc);ReadPod(in,ic);std::vector<Vertex> v(vc);std::vector<std::uint32_t> ix(ic);in.read((char*)v.data(),v.size()*sizeof(Vertex));in.read((char*)ix.data(),ix.size()*sizeof(std::uint32_t));std::uint32_t wc=0;ReadPod(in,wc);a->skinWeights[i].resize(wc);in.read((char*)a->skinWeights[i].data(),wc*sizeof(BoneWeight));s.mesh=std::make_unique<Mesh>(v,ix);if(wc==vc&&wc)s.mesh->SetSkinWeights(a->skinWeights[i]);a->sections.push_back(std::move(s));}
    std::uint32_t bc=0;ReadPod(in,bc);a->skeleton.bones.resize(bc);for(auto& bone:a->skeleton.bones){ReadString(in,bone.name);ReadPod(in,bone.parent);in.read((char*)bone.bindTranslation.data(),sizeof(float)*3);in.read((char*)bone.bindRotation.data(),sizeof(float)*4);in.read((char*)bone.bindScale.data(),sizeof(float)*3);in.read((char*)bone.bindLocalMatrix.data(),sizeof(float)*16);in.read((char*)bone.inverseBindMatrix.data(),sizeof(float)*16);}
    std::uint32_t ac=0;ReadPod(in,ac);a->animations.resize(ac);for(auto& clip:a->animations){ReadString(in,clip.name);ReadPod(in,clip.duration);std::uint32_t cc=0;ReadPod(in,cc);clip.channels.resize(cc);for(auto& ch:clip.channels){ReadPod(in,ch.bone);if(version>=2){ReadPod(in,ch.translationInterpolation);ReadPod(in,ch.rotationInterpolation);ReadPod(in,ch.scaleInterpolation);}auto r=[&](auto& keys){std::uint32_t n=0;ReadPod(in,n);keys.resize(n);for(auto& k:keys){ReadPod(in,k.time);in.read((char*)k.value.data(),sizeof(k.value));if(version>=2){in.read((char*)k.inTangent.data(),sizeof(k.inTangent));in.read((char*)k.outTangent.data(),sizeof(k.outTangent));}}};r(ch.translations);r(ch.rotations);r(ch.scales);}}
    if(!in){Logger::Error("Corrupt imported model asset: "+filepath);return nullptr;}return a;
}
