#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Mesh.h"
#include "../Math/Mat4.h"

struct ImportedMaterial
{
    std::string name = "Default";
    float diffuse[3]{ 1.0f, 1.0f, 1.0f };
    float specular[3]{ 0.04f, 0.04f, 0.04f };
    float shininess = 0.0f;
    float opacity = 1.0f;
    std::string diffuseTexture;

    float Roughness() const;
    float Metallic() const;
};

struct MeshSection
{
    std::unique_ptr<Mesh> mesh;
    std::uint32_t materialIndex = 0;
    std::string name;
};

struct Bone
{
    std::string name;
    int parent = -1;
    std::array<float, 3> bindTranslation{ 0.0f, 0.0f, 0.0f };
    std::array<float, 4> bindRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    std::array<float, 3> bindScale{ 1.0f, 1.0f, 1.0f };
    std::array<float, 16> bindLocalMatrix{
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };
    std::array<float, 16> inverseBindMatrix{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
};

struct Skeleton
{
    std::vector<Bone> bones;
};

struct BoneWeight
{
    std::array<std::uint16_t, 4> joints{};
    std::array<float, 4> weights{};
};

struct AnimationKeyVec3
{
    float time = 0.0f;
    std::array<float, 3> value{};
};

struct AnimationKeyQuat
{
    float time = 0.0f;
    std::array<float, 4> value{ 0.0f, 0.0f, 0.0f, 1.0f };
};

struct AnimationChannel
{
    int bone = -1;
    std::vector<AnimationKeyVec3> translations;
    std::vector<AnimationKeyQuat> rotations;
    std::vector<AnimationKeyVec3> scales;
};

struct AnimationClip
{
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationChannel> channels;
};

enum class ModelAssetType
{
    Static,
    Skeletal
};

struct ModelAsset
{
    ModelAssetType type = ModelAssetType::Static;
    std::string sourcePath;
    std::vector<ImportedMaterial> materials;
    std::vector<MeshSection> sections;
    Skeleton skeleton;
    std::vector<std::vector<BoneWeight>> skinWeights;
    std::vector<AnimationClip> animations;

    bool IsSkeletal() const { return type == ModelAssetType::Skeletal; }
    bool Empty() const { return sections.empty(); }

    std::vector<Mat4> EvaluateAnimation(std::size_t clipIndex, float time, bool loop = true) const;
    std::vector<Mat4> BindPose() const;
};
