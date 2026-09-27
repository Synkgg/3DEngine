#include "EnvironmentSystem.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
struct V { float x,y,z; };
V norm(V v){float l=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return {v.x/l,v.y/l,v.z/l};}
V faceDir(int face,float u,float v){
    switch(face){case 0:return norm({1,-v,-u});case 1:return norm({-1,-v,u});
    case 2:return norm({u,1,v});case 3:return norm({u,-1,-v});
    case 4:return norm({u,-v,1});default:return norm({-u,-v,-1});}
}
V sky(V d){
    float h=std::clamp(d.y*.5f+.5f,0.f,1.f), horizon=std::exp(-std::abs(d.y)*6.5f);
    V ground{.050f,.043f,.038f}, hc{.38f,.47f,.58f}, mid{.13f,.28f,.52f}, zen{.025f,.085f,.22f};
    auto mix=[](V a,V b,float t){return V{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};};
    auto smooth=[](float a,float b,float x){x=std::clamp((x-a)/(b-a),0.f,1.f);return x*x*(3.f-2.f*x);};
    V r=mix(ground,hc,smooth(.08f,.50f,h)); r=mix(r,mid,smooth(.46f,.72f,h)); r=mix(r,zen,smooth(.72f,1.f,h));
    r.x+=.13f*horizon;r.y+=.085f*horizon;r.z+=.045f*horizon; return r;
}}
bool EnvironmentSystem::Initialize(){
    Shutdown(); constexpr int S=64; glGenTextures(1,&m_EnvironmentMap); glBindTexture(GL_TEXTURE_CUBE_MAP,m_EnvironmentMap);
    std::vector<float> px(S*S*3);
    for(int f=0;f<6;++f){for(int y=0;y<S;++y)for(int x=0;x<S;++x){
        float u=(2.f*(x+.5f)/S)-1.f,v=(2.f*(y+.5f)/S)-1.f; V c=sky(faceDir(f,u,v)); int i=(y*S+x)*3;px[i]=c.x;px[i+1]=c.y;px[i+2]=c.z;}
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f,0,GL_RGB16F,S,S,0,GL_RGB,GL_FLOAT,px.data());}
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);glBindTexture(GL_TEXTURE_CUBE_MAP,0);return m_EnvironmentMap!=0;
}
void EnvironmentSystem::Shutdown(){if(m_EnvironmentMap)glDeleteTextures(1,&m_EnvironmentMap);m_EnvironmentMap=0;}
void EnvironmentSystem::Bind(unsigned int slot)const{glActiveTexture(GL_TEXTURE0+slot);glBindTexture(GL_TEXTURE_CUBE_MAP,m_EnvironmentMap);}
