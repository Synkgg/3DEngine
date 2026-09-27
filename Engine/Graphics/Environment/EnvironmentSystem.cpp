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
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    // Build a low-frequency diffuse irradiance cubemap on the CPU. This keeps
    // the environment system self-contained while giving diffuse PBR lighting
    // a genuinely convolved hemisphere instead of an arbitrary high mip.
    constexpr int I=16; std::vector<float> irr(I*I*3);
    glGenTextures(1,&m_IrradianceMap); glBindTexture(GL_TEXTURE_CUBE_MAP,m_IrradianceMap);
    const V tangentUp{0,1,0};
    for(int f=0;f<6;++f){for(int y=0;y<I;++y)for(int x=0;x<I;++x){
        float u=(2.f*(x+.5f)/I)-1.f,v=(2.f*(y+.5f)/I)-1.f; V n=faceDir(f,u,v); V sum{0,0,0}; float wsum=0;
        for(int sy=0;sy<8;++sy)for(int sx=0;sx<16;++sx){float phi=6.2831853f*(sx+.5f)/16.f;float ct=(sy+.5f)/8.f;float st=std::sqrt(std::max(0.f,1.f-ct*ct));
            V helper=std::abs(n.y)<.99f?tangentUp:V{1,0,0}; V t=norm({helper.y*n.z-helper.z*n.y,helper.z*n.x-helper.x*n.z,helper.x*n.y-helper.y*n.x}); V b={n.y*t.z-n.z*t.y,n.z*t.x-n.x*t.z,n.x*t.y-n.y*t.x};
            V d=norm({t.x*std::cos(phi)*st+b.x*std::sin(phi)*st+n.x*ct,t.y*std::cos(phi)*st+b.y*std::sin(phi)*st+n.y*ct,t.z*std::cos(phi)*st+b.z*std::sin(phi)*st+n.z*ct}); V s=sky(d); float w=ct;sum.x+=s.x*w;sum.y+=s.y*w;sum.z+=s.z*w;wsum+=w;}
        int i=(y*I+x)*3;irr[i]=sum.x/wsum;irr[i+1]=sum.y/wsum;irr[i+2]=sum.z/wsum;}
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f,0,GL_RGB16F,I,I,0,GL_RGB,GL_FLOAT,irr.data());}
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP,0);return m_EnvironmentMap!=0&&m_IrradianceMap!=0;
}
void EnvironmentSystem::Shutdown(){if(m_EnvironmentMap)glDeleteTextures(1,&m_EnvironmentMap);if(m_IrradianceMap)glDeleteTextures(1,&m_IrradianceMap);m_EnvironmentMap=m_IrradianceMap=0;}
void EnvironmentSystem::Bind(unsigned int slot)const{glActiveTexture(GL_TEXTURE0+slot);glBindTexture(GL_TEXTURE_CUBE_MAP,m_EnvironmentMap);}

void EnvironmentSystem::BindIrradiance(unsigned int slot)const{glActiveTexture(GL_TEXTURE0+slot);glBindTexture(GL_TEXTURE_CUBE_MAP,m_IrradianceMap);}
