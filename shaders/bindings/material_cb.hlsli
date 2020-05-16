#ifndef MATERIAL_CB_HLSLI
#define MATERIAL_CB_HLSLI

struct MaterialConstants
{
    float4x4 transform;
    float4 albedo_color;
    float3 diffuse_fresnel;
};

#ifdef PER_MATERIAL_CB_BINDING

cbuffer cbPerMaterial : register( PER_MATERIAL_CB_BINDING )
{
    MaterialConstants material;
}

#endif // PER_MATERIAL_CB_BINDING

#endif // MATERIAL_CB_HLSLI