#ifndef MATERIAL_CB_HLSLI
#define MATERIAL_CB_HLSLI

#ifdef PER_MATERIAL_CB_BINDING

cbuffer cbPerMaterial : register( PER_MATERIAL_CB_BINDING )
{
	float4x4 transform;
	float3 diffuse_fresnel;
}

#endif // PER_MATERIAL_CB_BINDING

#endif // MATERIAL_CB_HLSLI