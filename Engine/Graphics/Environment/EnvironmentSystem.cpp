#include "EnvironmentSystem.h"
#include "../RHI/RHI.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <bit>
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

namespace {
uint16_t half(float v) {
    uint32_t bits=std::bit_cast<uint32_t>(v), sign=(bits>>16)&0x8000;
    int exp=int((bits>>23)&255)-127+15;
    if(exp<=0) return uint16_t(sign);
    if(exp>=31) return uint16_t(sign|0x7bff);
    return uint16_t(sign|(uint32_t(exp)<<10)|((bits>>13)&1023));
}
}
bool EnvironmentSystem::Initialize()
{
    Shutdown(); auto* device=Velcryn::RHI::GetDevice(); if(!device)return false;
    auto create=[&](bool irradiance) {
        const int size=irradiance?16:64, mips=irradiance?1:7;
        std::vector<uint16_t> data;
        for(int f=0;f<6;++f) {
            std::vector<V> level(size*size);
            for(int y=0;y<size;++y)for(int x=0;x<size;++x) {
                V n=faceDir(f,2.f*(x+.5f)/size-1,2.f*(y+.5f)/size-1), c=sky(n);
                if(irradiance) {
                    V helper=std::abs(n.y)<.99f?V{0,1,0}:V{1,0,0};
                    V t=norm({helper.y*n.z-helper.z*n.y,helper.z*n.x-helper.x*n.z,helper.x*n.y-helper.y*n.x});
                    V b{n.y*t.z-n.z*t.y,n.z*t.x-n.x*t.z,n.x*t.y-n.y*t.x};
                    c={0,0,0};float total=0;
                    for(int sy=0;sy<8;++sy)for(int sx=0;sx<16;++sx) {
                        float phi=6.2831853f*(sx+.5f)/16,ct=(sy+.5f)/8,st=std::sqrt(1-ct*ct);
                        V d=norm({t.x*std::cos(phi)*st+b.x*std::sin(phi)*st+n.x*ct,t.y*std::cos(phi)*st+b.y*std::sin(phi)*st+n.y*ct,t.z*std::cos(phi)*st+b.z*std::sin(phi)*st+n.z*ct});
                        V s=sky(d); c.x+=s.x*ct;c.y+=s.y*ct;c.z+=s.z*ct;total+=ct;
                    }
                    c={c.x/total,c.y/total,c.z/total};
                }
                level[y*size+x]=c;
            }
            int width=size;
            for(int mip=0;mip<mips;++mip) {
                for(V c:level) {data.push_back(half(c.x));data.push_back(half(c.y));data.push_back(half(c.z));data.push_back(half(1));}
                if(width==1)break;
                const int next=width/2;std::vector<V> reduced(next*next);
                for(int y=0;y<next;++y)for(int x=0;x<next;++x) {
                    V c{};for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){V q=level[(y*2+dy)*width+x*2+dx];c.x+=q.x*.25f;c.y+=q.y*.25f;c.z+=q.z*.25f;}
                    reduced[y*next+x]=c;
                }
                level=std::move(reduced);width=next;
            }
        }
        Velcryn::RHI::TextureDesc desc{};desc.width=desc.height=size;desc.arrayLayers=6;desc.mipLevels=mips;
        desc.format=Velcryn::RHI::TextureFormat::RGBA16_Float;
        desc.usage=Velcryn::RHI::TextureUsage::Sampled|Velcryn::RHI::TextureUsage::TransferDestination;
        desc.debugName=irradiance?"DiffuseIrradiance":"EnvironmentRadiance";
        return device->CreateTexture(desc,data.data(),data.size()*sizeof(uint16_t));
    };
    m_EnvironmentMap=create(false);m_IrradianceMap=create(true);
    if(!m_EnvironmentMap||!m_IrradianceMap){Shutdown();return false;}return true;
}
void EnvironmentSystem::Shutdown(){if(auto* d=Velcryn::RHI::GetDevice()){if(m_EnvironmentMap)d->DestroyTexture(m_EnvironmentMap);if(m_IrradianceMap)d->DestroyTexture(m_IrradianceMap);}m_EnvironmentMap={};m_IrradianceMap={};}
