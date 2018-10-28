#ifndef MATERIAL_CB_HLSLI
#define MATERIAL_CB_HLSLI

#ifdef PER_MATERIAL_CB_BINDING

struct MaterialConstants
{
    float4x4 transform;
	float3 diffuse_fresnel;
};

cbuffer cbPerMaterial : register( PER_MATERIAL_CB_BINDING )
{
	MaterialConstants material;
}

#endif // PER_MATERIAL_CB_BINDING

#endif // MATERIAL_CB_HLSLI