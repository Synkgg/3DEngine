#include "ModelLoader.h"
#include "ModelAsset.h"
#include "Mesh.h"
#include "../Core/Logger.h"

#include <algorithm>
#include <cmath>\n#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

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

void LoadMTL(const std::filesystem::path& path,std::vector<ImportedMaterial>& out)
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
        else if(current&&tag=="Kd") ss>>current->diffuse[0]>>current->diffuse[1]>>current->diffuse[2];
        else if(current&&tag=="Ks") ss>>current->specular[0]>>current->specular[1]>>current->specular[2];
        else if(current&&tag=="Ns") ss>>current->shininess;
        else if(current&&tag=="d") ss>>current->opacity;
        else if(current&&tag=="Tr"){float tr=0.0f;ss>>tr;current->opacity=1.0f-tr;}
        else if(current&&tag=="map_Kd")
        {
            std::string texture;std::getline(ss,texture);
            texture=Trim(texture);
            if(!texture.empty()) current->diffuseTexture=(path.parent_path()/texture).lexically_normal().string();
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
    // Wavefront Ns is [0,1000]. Convert its specular exponent to a practical
    // PBR roughness approximation while keeping old matte assets sensible.
    const float ns=std::max(0.0f,shininess);
    return std::clamp(std::sqrt(2.0f/(ns+2.0f)),0.04f,1.0f);
}

float ImportedMaterial::Metallic() const
{
    const float maxSpec=std::max(specular[0],std::max(specular[1],specular[2]));
    return std::clamp((maxSpec-0.04f)/0.96f,0.0f,1.0f);
}

std::unique_ptr<ModelAsset> ModelLoader::LoadModel(const std::string& filepath,const ModelImportSettings& settings)
{
    const std::string ext=std::filesystem::path(filepath).extension().string();
    std::string lower=ext;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return (char)std::tolower(c);});
    if(lower==".obj") return LoadOBJModel(filepath,settings);
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

    auto getBuilder=[&]() -> SectionBuilder& {
        const std::string key=settings.mergeMaterialSections?currentMaterial:(currentMaterial+"#"+std::to_string(builders.size()));
        auto it=builderByMaterial.find(key);
        if(settings.mergeMaterialSections&&it!=builderByMaterial.end()) return builders[it->second];
        const size_t index=builders.size();builders.push_back({});builders.back().material=currentMaterial;
        if(settings.mergeMaterialSections) builderByMaterial[key]=index;
        return builders.back();
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
            if(!mtl.empty()) LoadMTL((std::filesystem::path(filepath).parent_path()/mtl).lexically_normal(),model->materials);
        }
        else if(tag=="usemtl")
        {
            std::string name;std::getline(ss,name);name=Trim(name);currentMaterial=name.empty()?"Default":name;
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
            for(size_t i=1;i+1<face.size();++i){b.indices.push_back(emit(face[0]));b.indices.push_back(emit(face[i]));b.indices.push_back(emit(face[i+1]));}
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
