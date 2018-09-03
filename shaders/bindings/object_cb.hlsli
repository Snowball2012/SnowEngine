#ifndef OBJECT_CB_HLSLI
#define OBJECT_CB_HLSLI

#ifdef PER_OBJECT_CB_BINDING

cbuffer cbPerObject : register( PER_OBJECT_CB_BINDING )
{
    float4x4 model_mat;
    float4x4 model_inv_transpose_mat;
}

#endif // PER_OBJECT_CB_BINDING
#endif // OBJECT_CB_HLSLI