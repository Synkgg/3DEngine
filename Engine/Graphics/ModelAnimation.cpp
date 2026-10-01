#include "ModelAsset.h"
#include "../Math/Mat4.h"
#include "../Math/Vec3.h"
#include <algorithm>
#include <cmath>

namespace {
Mat4 FromArray(const std::array<float,16>& a){Mat4 m;for(int i=0;i<16;++i)m.elements[i]=a[i];return m;}
Mat4 QuatMatrix(std::array<float,4> q){
 float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);if(l<=0.000001f)return Mat4::Identity();for(float& v:q)v/=l;
 float x=q[0],y=q[1],z=q[2],w=q[3];Mat4 m=Mat4::Identity();
 m.elements[0]=1-2*y*y-2*z*z;m.elements[1]=2*x*y+2*w*z;m.elements[2]=2*x*z-2*w*y;
 m.elements[4]=2*x*y-2*w*z;m.elements[5]=1-2*x*x-2*z*z;m.elements[6]=2*y*z+2*w*x;
 m.elements[8]=2*x*z+2*w*y;m.elements[9]=2*y*z-2*w*x;m.elements[10]=1-2*x*x-2*y*y;return m;
}
template<class K> std::size_t Segment(const std::vector<K>& keys,float t){if(keys.size()<2)return 0;for(std::size_t i=0;i+1<keys.size();++i)if(t<keys[i+1].time)return i;return keys.size()-2;}
std::array<float,3> Sample3(const std::vector<AnimationKeyVec3>& k,float t,std::array<float,3> d){if(k.empty())return d;if(k.size()==1||t<=k.front().time)return k.front().value;if(t>=k.back().time)return k.back().value;auto i=Segment(k,t);float a=(t-k[i].time)/std::max(k[i+1].time-k[i].time,0.000001f);for(int n=0;n<3;++n)d[n]=k[i].value[n]+(k[i+1].value[n]-k[i].value[n])*a;return d;}
std::array<float,4> SampleQ(const std::vector<AnimationKeyQuat>& k,float t,std::array<float,4> d){if(k.empty())return d;if(k.size()==1||t<=k.front().time)return k.front().value;if(t>=k.back().time)return k.back().value;auto i=Segment(k,t);float a=(t-k[i].time)/std::max(k[i+1].time-k[i].time,0.000001f);auto p=k[i].value,q=k[i+1].value;float dot=0;for(int n=0;n<4;++n)dot+=p[n]*q[n];if(dot<0)for(float& v:q)v=-v;for(int n=0;n<4;++n)d[n]=p[n]+(q[n]-p[n])*a;float l=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]+d[3]*d[3]);if(l>0)for(float& v:d)v/=l;return d;}
Mat4 TRS(const std::array<float,3>& t,const std::array<float,4>& r,const std::array<float,3>& s){return Mat4::Translation(Vec3(t[0],t[1],t[2]))*QuatMatrix(r)*Mat4::Scale(Vec3(s[0],s[1],s[2]));}
}
std::vector<Mat4> ModelAsset::BindPose() const{
 std::vector<Mat4> global(skeleton.bones.size()),out(skeleton.bones.size());
 for(std::size_t i=0;i<skeleton.bones.size();++i){const Bone& b=skeleton.bones[i];Mat4 local=FromArray(b.bindLocalMatrix);global[i]=b.parent>=0?global[b.parent]*local:local;out[i]=global[i]*FromArray(b.inverseBindMatrix);}return out;
}
std::vector<Mat4> ModelAsset::EvaluateAnimation(std::size_t clipIndex,float time,bool loop) const{
 if(clipIndex>=animations.size())return BindPose();const AnimationClip& clip=animations[clipIndex];if(loop&&clip.duration>0)time=std::fmod(std::max(time,0.0f),clip.duration);else time=std::clamp(time,0.0f,clip.duration);
 struct Pose{std::array<float,3> t;std::array<float,4> r;std::array<float,3> s;bool animated=false;};
 std::vector<Pose> pose(skeleton.bones.size());std::vector<Mat4> local(skeleton.bones.size());
 for(std::size_t i=0;i<skeleton.bones.size();++i){const Bone& b=skeleton.bones[i];pose[i]={b.bindTranslation,b.bindRotation,b.bindScale,false};local[i]=FromArray(b.bindLocalMatrix);}
 for(const AnimationChannel& c:clip.channels){if(c.bone<0||c.bone>=static_cast<int>(skeleton.bones.size()))continue;Pose& p=pose[c.bone];if(!c.translations.empty())p.t=Sample3(c.translations,time,p.t);if(!c.rotations.empty())p.r=SampleQ(c.rotations,time,p.r);if(!c.scales.empty())p.s=Sample3(c.scales,time,p.s);p.animated=true;}
 for(std::size_t i=0;i<pose.size();++i)if(pose[i].animated)local[i]=TRS(pose[i].t,pose[i].r,pose[i].s);
 std::vector<Mat4> global(local.size()),out(local.size());for(std::size_t i=0;i<local.size();++i){const Bone& b=skeleton.bones[i];global[i]=b.parent>=0?global[b.parent]*local[i]:local[i];out[i]=global[i]*FromArray(b.inverseBindMatrix);}return out;
}
