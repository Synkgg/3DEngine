#pragma once
#include <memory>
#include <string>

#include "ModelAsset.h"

struct ModelImportSettings
{
    bool generateNormals = true;
    bool importMaterials = true;
    bool importTextures = true;
    bool mergeMaterialSections = true;
};

class ModelLoader
{
public:
    static std::unique_ptr<ModelAsset> LoadModel(
        const std::string& filepath,
        const ModelImportSettings& settings = {}
    );

    static std::unique_ptr<ModelAsset> LoadOBJModel(
        const std::string& filepath,
        const ModelImportSettings& settings = {}
    );

    static bool SaveImportedAsset(const std::string& filepath, const ModelAsset& asset, const ModelImportSettings& settings);
    static std::unique_ptr<ModelAsset> LoadImportedAsset(const std::string& filepath);

    static std::unique_ptr<ModelAsset> LoadGLTFModel(
        const std::string& filepath,
        const ModelImportSettings& settings = {}
    );

    // Compatibility path for callers that still require one merged mesh.
    static std::unique_ptr<Mesh> LoadOBJ(const std::string& filepath);
};
