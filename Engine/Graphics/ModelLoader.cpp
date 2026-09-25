#include "ModelLoader.h"
#include "Mesh.h"
#include "../Core/Logger.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>

namespace {
struct V3 { float x=0,y=0,z=0; };
struct V2 { float x=0,y=0; };
struct Ref { int p=0,t=0,n=0; };
Ref ParseRef(const std::string& s) {
    Ref r; std::stringstream ss(s); std::string x;
    if (std::getline(ss,x,'/')) r.p=x.empty()?0:std::stoi(x);
    if (std::getline(ss,x,'/')) r.t=x.empty()?0:std::stoi(x);
    if (std::getline(ss,x,'/')) r.n=x.empty()?0:std::stoi(x);
    return r;
}
int Resolve(int i,int size){ return i>0?i-1:(i<0?size+i:-1); }
}

std::unique_ptr<Mesh> ModelLoader::LoadOBJ(const std::string& filepath)
{
    std::ifstream f(filepath);
    if(!f){ Logger::Error("Could not open OBJ: "+filepath); return nullptr; }
    std::vector<V3> p,n; std::vector<V2> uv;
    std::vector<Vertex> vertices; std::vector<std::uint32_t> indices;
    std::unordered_map<std::string,std::uint32_t> cache;
    std::string line;
    auto emit=[&](const std::string& key)->std::uint32_t{
        auto it=cache.find(key); if(it!=cache.end()) return it->second;
        Ref r=ParseRef(key); Vertex v{};
        int pi=Resolve(r.p,(int)p.size()), ti=Resolve(r.t,(int)uv.size()), ni=Resolve(r.n,(int)n.size());
        if(pi>=0){v.position[0]=p[pi].x;v.position[1]=p[pi].y;v.position[2]=p[pi].z;}
        if(ti>=0){v.uv[0]=uv[ti].x;v.uv[1]=uv[ti].y;}
        if(ni>=0){v.normal[0]=n[ni].x;v.normal[1]=n[ni].y;v.normal[2]=n[ni].z;}
        auto id=(std::uint32_t)vertices.size(); vertices.push_back(v); cache[key]=id; return id;
    };
    while(std::getline(f,line)){
        std::stringstream ss(line); std::string tag; ss>>tag;
        if(tag=="v"){V3 x;ss>>x.x>>x.y>>x.z;p.push_back(x);}
        else if(tag=="vt"){V2 x;ss>>x.x>>x.y;uv.push_back(x);}
        else if(tag=="vn"){V3 x;ss>>x.x>>x.y>>x.z;n.push_back(x);}
        else if(tag=="f"){std::vector<std::string> face;std::string x;while(ss>>x)face.push_back(x);
            for(size_t i=1;i+1<face.size();++i){indices.push_back(emit(face[0]));indices.push_back(emit(face[i]));indices.push_back(emit(face[i+1]));}}
    }
    if(vertices.empty()||indices.empty()){Logger::Error("OBJ contains no renderable faces: "+filepath);return nullptr;}

    // OBJ files commonly omit normals. Generate smooth vertex normals so
    // imported meshes still participate correctly in the material/light pass.
    bool hasUsableNormals = false;
    for (const Vertex& v : vertices)
    {
        const float lengthSq = v.normal[0]*v.normal[0] + v.normal[1]*v.normal[1] + v.normal[2]*v.normal[2];
        if (lengthSq > 0.000001f) { hasUsableNormals = true; break; }
    }
    if (!hasUsableNormals)
    {
        for (Vertex& v : vertices) { v.normal[0]=0.0f; v.normal[1]=0.0f; v.normal[2]=0.0f; }
        for (size_t i=0; i+2<indices.size(); i+=3)
        {
            Vertex& a=vertices[indices[i]]; Vertex& b=vertices[indices[i+1]]; Vertex& d=vertices[indices[i+2]];
            const float abx=b.position[0]-a.position[0], aby=b.position[1]-a.position[1], abz=b.position[2]-a.position[2];
            const float acx=d.position[0]-a.position[0], acy=d.position[1]-a.position[1], acz=d.position[2]-a.position[2];
            const float nx=aby*acz-abz*acy, ny=abz*acx-abx*acz, nz=abx*acy-aby*acx;
            for (Vertex* v : {&a,&b,&d}) { v->normal[0]+=nx; v->normal[1]+=ny; v->normal[2]+=nz; }
        }
        for (Vertex& v : vertices)
        {
            const float len=std::sqrt(v.normal[0]*v.normal[0]+v.normal[1]*v.normal[1]+v.normal[2]*v.normal[2]);
            if (len>0.000001f) { v.normal[0]/=len; v.normal[1]/=len; v.normal[2]/=len; }
            else v.normal[1]=1.0f;
        }
    }
    Logger::Info("Loaded OBJ model: "+filepath+" ("+std::to_string(vertices.size())+" vertices, "+std::to_string(indices.size()/3)+" triangles)");
    return std::make_unique<Mesh>(vertices,indices);
}
