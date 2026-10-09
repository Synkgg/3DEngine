struct PointLight { float4 positionRange; float4 colorIntensity; };
struct SpotLight { float4 positionRange; float4 colorIntensity; float4 directionInner; float4 outer; };
cbuffer MeshData : register(b32, space1)
{
    column_major float4x4 model;
    float4 lightColorIntensity;
    float4 cameraAmbient;
    float4 material;
    float4 settings;
    PointLight points[8];
    SpotLight spots[4];
    float4 environment;
    float4 fog;
    float4 mapFlags;
    float4 cascadeSplits;
    column_major float4x4 lightMatrices[3];
    column_major float4x4 bones[128];
};
