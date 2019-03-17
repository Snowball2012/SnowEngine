#ifndef OBJECT_CB_HLSLI
#define OBJECT_CB_HLSLI

struct ObjectConstants
{
    float4x4 model_mat;
    float4x4 model_inv_transpose_mat;
};

#ifdef PER_OBJECT_CB_BINDING

cbuffer cbPerObject : register( PER_OBJECT_CB_BINDING )
{
    ObjectConstants renderitem;
}

#endif // PER_OBJECT_CB_BINDING
#endif // OBJECT_CB_HLSLI