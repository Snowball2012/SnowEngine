#include "Utils.hlsli"

#include "SceneViewParams.hlsli"

[[vk::binding( 0, 0 )]] RWTexture2D<float4> output;
[[vk::binding( 1, 0 )]] RWTexture2D<uint> level_object_ids;

#define USE_IMPORTANCE_SAMPLING
#define USE_GGX_SAMPLING

// pixel_shift is in range [0,1]. 0.5 means middle of the pixel
float3 PixelPositionToWorld( uint2 pixel_position, float2 pixel_shift, float ndc_depth, uint2 viewport_size, float4x4 view_proj_inverse )
{
    float4 ndc = float4( ( float2( pixel_position ) + pixel_shift ) / float2( viewport_size ) * 2.0f - float2( 1.0f, 1.0f ), ndc_depth, 1.0f );
    ndc.y = -ndc.y;

    float4 world_pos = mul( view_proj_inverse, ndc );

    world_pos /= world_pos.w;

    return world_pos.xyz;
}

static const uint INVALID_PRIMITIVE = -1;
static const uint INVALID_INSTANCE_INDEX = -1;
static const uint INVALID_GEOM_INDEX = -1;
static const uint INVALID_MATERIAL_INDEX = -1;

struct Payload
{
    float2 barycentrics;
    uint instance_index;
    uint primitive_index;
    float t;
};

float3 ReconstructBarycentrics( float2 payload_barycentrics )
{
    return float3( payload_barycentrics, saturate( 1.0f - payload_barycentrics.x - payload_barycentrics.y ) );
}

Payload PayloadInit( float max_t )
{
    Payload ret;
    ret.barycentrics = float2( 0, 0 );
    ret.instance_index = INVALID_INSTANCE_INDEX;
    ret.primitive_index = INVALID_INSTANCE_INDEX;
    ret.t = max_t;
    
    return ret;
}

struct HitData
{
    Payload ray_payload;
    uint geom_index;
    uint material_index;
    int picking_id;
    float3x4 obj_to_world;
};

HitData GetHitData( Payload ray_payload )
{
    HitData data;
    data.ray_payload = ray_payload;
    data.geom_index = INVALID_GEOM_INDEX;
    data.material_index = INVALID_MATERIAL_INDEX;
    data.picking_id = -1;
    if ( ray_payload.instance_index != INVALID_INSTANCE_INDEX )
    {
        TLASItemParams item_params = tlas_items[ ray_payload.instance_index ];
        
        data.geom_index = item_params.geom_buf_index;
        data.obj_to_world = item_params.object_to_world_mat;
        data.picking_id = item_params.picking_id;
        data.material_index = item_params.material_index;
    }
    
    return data;
}

float3 GetHitNormal( HitData hit_data, out bool hit_valid )
{    
    hit_valid = false;
    float3 out_normal = _float3( 0 );
    if ( hit_data.geom_index != INVALID_GEOM_INDEX )
    {
        hit_valid = true;
        uint base_index = hit_data.ray_payload.primitive_index * 3;
        uint16_t i0 = geom_indices[hit_data.geom_index][base_index + 0];
        uint16_t i1 = geom_indices[hit_data.geom_index][base_index + 1];
        uint16_t i2 = geom_indices[hit_data.geom_index][base_index + 2];
        
        MeshVertex v0 = geom_vertices[hit_data.geom_index][i0];
        MeshVertex v1 = geom_vertices[hit_data.geom_index][i1];
        MeshVertex v2 = geom_vertices[hit_data.geom_index][i2];
        
        float3 triangle_normal_ls = normalize( cross( v1.position - v0.position, v2.position - v0.position ) );
        
        float3 triangle_normal_ws =
            normalize( mul(
                float3x3(
                    hit_data.obj_to_world[0].xyz,
                    hit_data.obj_to_world[1].xyz,
                    hit_data.obj_to_world[2].xyz ),
                triangle_normal_ls ) );
        
        //if ( hit_data.geom_index != 0 )
        //    triangle_normal_ws = -triangle_normal_ws;
        out_normal = triangle_normal_ws;
    }
    
    return out_normal;
}

float3 SampleSphereUniform( float2 e )
{
	float phi = 2.0f * M_PI * e.x;
	float cos_theta = 1.0f - 2.0f * e.y;
	float sin_theta = sqrt( 1.0f - sqr( cos_theta ) );

    return float3( sin_theta * cos( phi ), sin_theta * sin( phi ), cos_theta );
}

float3 SampleHemisphereCosineWeighted( float2 e, float3 halfplane_normal )
{
    float3 h = SampleSphereUniform( e ).xyz;
	return normalize( halfplane_normal * 1.001f + h ); // slightly biased, but does not hit NaN    
}

// result is local space ( Y up )
float3 SampleHemisphereUniform( float2 e )
{
    float3 uniform_hemisphere_sample = SampleSphereUniform( e );
    uniform_hemisphere_sample.y = abs( uniform_hemisphere_sample.y );
    return uniform_hemisphere_sample;
}

// pdf in .w
float4 SampleHemisphereCosineWeighted( float2 e )
{
    float3 h = SampleSphereUniform( e ).xyz;
    h.y += 1.0001f;
    
    h = normalize( h );
    
    float pdf = h.y / M_PI;
	return float4( normalize( h ), pdf ); // slightly biased, but does not hit NaN    
}

float VisibleGGXPDF(float3 V, float3 H, float a2)
{
	float NoV = V.z;
	float NoH = H.z;
	float VoH = saturate( dot(V, H) );

	float d = (NoH * a2 - NoH) * NoH + 1;
	float D = a2 / ( M_PI*d*d );

	float PDF = 2 * VoH * D / (NoV + sqrt(NoV * (NoV - NoV * a2) + a2));
	return PDF;
}

float VisibleGGXPDF(float NoV, float NoH, float VoH, float a2)
{
	float d = (NoH * a2 - NoH) * NoH + 1;
	float D = a2 / ( M_PI*d*d );

	float PDF = 2 * VoH * D / (NoV + sqrt(NoV * (NoV - NoV * a2) + a2));
	return PDF;
}

float2 UniformSampleDisk( float2 E )
{
	float Theta = 2 * M_PI * E.x;
	float Radius = sqrt( E.y );
	return Radius * float2( cos( Theta ), sin( Theta ) );
}

float4 ImportanceSampleVisibleGGX( float2 E, float a2, float3 V )
{
    float2 DiskE = UniformSampleDisk( E );

	// NOTE: See below for anisotropic version that avoids this sqrt
	float a = sqrt(a2);

	// stretch
	float3 Vh = normalize( float3( a * V.xy, V.z ) );

	// Stable tangent basis based on V
	// Tangent0 is orthogonal to N
	float LenSq = Vh.x * Vh.x + Vh.y * Vh.y;
	float3 Tangent0 = LenSq > 0 ? float3(-Vh.y, Vh.x, 0) * rsqrt(LenSq) : float3(1, 0, 0);
	float3 Tangent1 = cross(Vh, Tangent0);

	float2 p = DiskE;
	float s = 0.5 + 0.5 * Vh.z;
	p.y = (1 - s) * sqrt( 1 - p.x * p.x ) + s * p.y;

	float3 H;
	H  = p.x * Tangent0;
	H += p.y * Tangent1;
	H += sqrt( saturate( 1 - dot( p, p ) ) ) * Vh;

	// unstretch
	H = normalize( float3( a * H.xy, max(0.0, H.z) ) );

    float3 L = reflect( -V, H );
	return float4(L, VisibleGGXPDF(V, H, a2));
}

struct BSDFInputs
{
    float3 albedo;
    float roughness;
    float3 normal;
    float3 f0;
    float3x3 tnb; // Tangent frame. N is _second_ vector to work nice with Y-up local space
};

// Opaque. Does not contain NoL term ( it's not a part of BSDF, it's just a rendering equation term and should be applied outside
float3 EvaluateBSDF( BSDFInputs inputs, float3 l, float3 v )
{
    // simple lambertian bsdf + GGX mixed on Fresnel
    
    float3 n = inputs.normal.xyz;
    float3 f0 = inputs.f0;
    float3 albedo = inputs.albedo;
    float roughness = inputs.roughness;

    float3 h = normalize( l + v );
    
    float NoL = ( dot( l, n ) );
    float NoH = ( dot( n, h ) );
    float NoV = ( dot( n, v ) );
    float VoH = ( dot( v, h ) );
    
    if ( ( NoL <= FLT_EPSILON ) || ( NoH <= FLT_EPSILON ) || ( NoV <= FLT_EPSILON ) || ( VoH <= FLT_EPSILON ) )
        return float3( 0, 0, 0 );
        
    NoL = saturate( NoL );
    NoH = saturate( NoH );
    NoV = saturate( NoV );
    VoH = saturate( VoH );
    
    float3 lambertian_diffuse = albedo / M_PI;
    
    float a = sqr( roughness );
    float a2 = sqr( a );
    
    float3 ggx_spec = _float3( VisibleGGXPDF( NoV, NoH, VoH, a2 ) );
    
    float3 fresnel = f0 + ( _float3( 1.0f ) - f0 ) * pow( 1.0f - NoL, 5 );

    return lerp( lambertian_diffuse, ggx_spec, fresnel );
}

// dir_i is incoming ray direction (view)
// e - 3 uniformly distributed random variables
struct BSDFSampleOutput
{
    float3 integration_term; // bsdf * dot( n, l ) / pdf
    float3 dir_o;
};

BSDFSampleOutput SampleBSDFMonteCarlo( BSDFInputs inputs, float3 dir_i, float3 e )
{
    #if defined USE_IMPORTANCE_SAMPLING
        float4 dir_o_ts = float4( 0, 0, 0, 0 );
        float3 v_ts = mul( inputs.tnb, -dir_i );
        float a2 = pow( inputs.roughness, 4 );
        if ( e.z > 0.5f/* inputs.roughness*/ )
        {
            float4 dir_o_ts_spec = float4( 0, 0, 0, 0 );
            dir_o_ts_spec = ImportanceSampleVisibleGGX( e.xy, a2, v_ts.xzy );
            dir_o_ts_spec.xyz = dir_o_ts_spec.xzy;
            
            dir_o_ts.xyz = dir_o_ts_spec.xyz;
            
            float difuse_pdf = abs( dir_o_ts.y ) / M_PI; 
            dir_o_ts.w = lerp( dir_o_ts_spec.w, difuse_pdf, 0.5f/*inputs.roughness*/ );
        }
        else
        {
            float4 dir_o_ts_diffuse = SampleHemisphereCosineWeighted( e.xy );
            
            dir_o_ts.xyz = dir_o_ts_diffuse.xyz;
            
            float3 h = normalize( dir_o_ts.xyz + v_ts );
            
            float spec_pdf = VisibleGGXPDF( v_ts.xzy, h.xzy, a2 ); 
            dir_o_ts.w = lerp( spec_pdf, dir_o_ts_diffuse.w, 0.5f/*inputs.roughness*/ );
        }
        float pdf = dir_o_ts.w;
    #else
        float3 dir_o_ts = SampleHemisphereUniform( e.xy );
        float pdf = 0.5f / M_PI;
    #endif
    
    float3 dir_o_ws = normalize( mul( dir_o_ts.xyz, inputs.tnb ) );
    
    float3 bsdf = EvaluateBSDF( inputs, dir_o_ws, -dir_i );
    
    BSDFSampleOutput output;
    output.integration_term = bsdf * saturate( dot( inputs.normal, dir_o_ws ) ) / pdf;
    
    if ( dir_o_ts.y <= 0 || any( isnan( output.integration_term ) ) || any( isinf( output.integration_term ) ) )
        output.integration_term = _float3( 0 );
    
    output.dir_o = dir_o_ws;
    
    return output;
}

BSDFInputs SampleHitMaterial( HitData hit_data, float3 hit_direction_ws, out bool hit_valid )
{
    hit_valid = false;
    BSDFInputs inputs;
    
    float3 hit_normal_ws = GetHitNormal( hit_data, hit_valid );
    if ( dot( hit_normal_ws, hit_direction_ws ) > 0 )
    {
        hit_valid = false;
        hit_normal_ws = -hit_normal_ws;
    }
    
    if ( hit_valid == false )
        return inputs;
    
    Material material;
    if ( hit_data.material_index != INVALID_MATERIAL_INDEX )
    {
        material = materials[ hit_data.material_index ];
    }
    
    inputs.albedo = material.albedo;
    inputs.roughness = material.roughness;
    inputs.f0 = material.f0;
    
    inputs.normal = hit_normal_ws;
    
    float3 tan_unnormalized = cross( float3( 0, 1, 0 ), hit_normal_ws );
    if ( dot( tan_unnormalized, tan_unnormalized ) < 1.e-6f )
    {
        tan_unnormalized = cross( float3( 0, 0, 1 ), hit_normal_ws );
    }
    float3 tan = normalize( tan_unnormalized );
    float3 bitan = normalize( cross( tan, hit_normal_ws ) );
                
    float3x3 tnb = float3x3( tan, hit_normal_ws, bitan );
    
    inputs.tnb = tnb;
    
    return inputs;
}

struct PathData
{
    float3 bsdf_acc;
    float3 current_direction_ws;
    float3 current_position_ws;
    bool terminated;
};

void TracePath( HitData hit, float3 e, inout PathData path )
{
    bool is_hit_valid = false;
    BSDFInputs bsdf_inputs = SampleHitMaterial( hit, path.current_direction_ws, is_hit_valid );
    if ( ! is_hit_valid )
    {
        path.terminated = true;
        return;
    }
    
    float3 current_position_ws = path.current_position_ws + path.current_direction_ws * hit.ray_payload.t + bsdf_inputs.normal * 1.e-4f;
    path.current_position_ws = current_position_ws;
    
    BSDFSampleOutput bsdf_sample = SampleBSDFMonteCarlo( bsdf_inputs, path.current_direction_ws, e );
    
    path.current_direction_ws = bsdf_sample.dir_o;
    path.bsdf_acc *= bsdf_sample.integration_term;
}

PathData PathInit( float3 ray_start_pos_ws, float3 ray_direction_ws )
{
    PathData data;
    data.bsdf_acc = _float3( 1.0f );
    data.terminated = false;
    data.current_position_ws = ray_start_pos_ws;
    data.current_direction_ws = ray_direction_ws;
    
    return data;
}

float3 GetMissRadiance()
{
    return _float3( 10.0f );
}

// PDF = D * NoH / (4 * VoH)
float4 ImportanceSampleGGX( float2 E, float a2 )
{
	float Phi = 2 * M_PI * E.x;
	float CosTheta = sqrt( (1 - E.y) / ( 1 + (a2 - 1) * E.y ) );
	float SinTheta = sqrt( 1 - CosTheta * CosTheta );

	float3 H;
	H.x = SinTheta * cos( Phi );
	H.y = SinTheta * sin( Phi );
	H.z = CosTheta;
	
	float d = ( CosTheta * a2 - CosTheta ) * CosTheta + 1;
	float D = a2 / ( M_PI*d*d );
	float PDF = D * CosTheta;

	return float4( H, PDF );
}

float3x3 GetTangentBasis( float3 TangentZ )
{
	const float Sign = TangentZ.z >= 0 ? 1 : -1;
	const float a = -rcp( Sign + TangentZ.z );
	const float b = TangentZ.x * TangentZ.y * a;
	
	float3 TangentX = { 1 + Sign * a * sqr( TangentZ.x ), Sign * b, -Sign * TangentZ.x };
	float3 TangentY = { b,  Sign + a * sqr( TangentZ.y ), -TangentZ.y };

	return float3x3( TangentX, TangentY, TangentZ );
}


[shader( "raygeneration" )]
void VisibilityRGS()
{
    uint2 pixel_id = DispatchRaysIndex().xy;
    
    float2 pixel_shift = GetUnitRandomUniform( pixel_id );
    
    float3 camera_ray_origin = PixelPositionToWorld( pixel_id, pixel_shift, 0, view_data.viewport_size_px, view_data.view_proj_inv_mat );

    float3 camera_ray_destination = PixelPositionToWorld( pixel_id, pixel_shift, 1.0f, view_data.viewport_size_px, view_data.view_proj_inv_mat );

    float3 camera_ray_direction = camera_ray_destination - camera_ray_origin;

    float max_t = length( camera_ray_direction );
    camera_ray_direction = normalize( camera_ray_direction );

    RayDesc camera_ray = { camera_ray_origin, 0, camera_ray_direction, max_t };

    Payload payload = PayloadInit( max_t );

    TraceRay( scene_tlas,
        ( RAY_FLAG_NONE ),
        0xFF, 0, 1, 0, camera_ray, payload );
    
    HitData initial_hit = GetHitData( payload );
    
    if ( initial_hit.geom_index == INVALID_GEOM_INDEX )
    {
        if ( view_data.use_accumulation )
        {
            float4 cur_value = output[pixel_id];
            output[pixel_id] = float4( cur_value.xyz + GetMissRadiance(), asfloat( asuint( cur_value.w ) + 1 ) );
        }
        else
        {
            output[pixel_id] = float4( GetMissRadiance(), asfloat( uint( 1 ) ) );
        }
        
        level_object_ids[pixel_id] = -1;
        
        return;
    }
    
    level_object_ids[pixel_id] = initial_hit.picking_id;
    
    if ( all( pixel_id == view_data.cursor_position_px ) )
    {
        view_frame_readback_data[0].picking_id_under_cursor = initial_hit.picking_id;
    }
    
    const uint n_samples = 1;
    float3 radiance_accum = _float3( 0 );
    
    for ( uint sample_i = 0; sample_i < n_samples; ++sample_i )
    {
        PathData path = PathInit( camera_ray_origin, camera_ray_direction );
        
        const uint n_bounces = 16;
        HitData current_hit = initial_hit;
        
        for ( uint bounce_i = 0; bounce_i < n_bounces; ++bounce_i )
        {
            float3 e3d = GetUnitRandomUniform( sample_i * n_bounces + bounce_i, pixel_id );
            
            // @todo - third random variable
            
            TracePath( current_hit, e3d, path );
            
            if ( all( path.bsdf_acc < FLT_EPSILON ) )
            {
                break;
            }
            
            if ( path.terminated )
            {
                radiance_accum += float3( 1, 0, 1 ) * path.bsdf_acc;
                break;
            }
            
            RayDesc ray_bounce = { path.current_position_ws, 0, path.current_direction_ws, max_t };
            
            Payload payload_bounce = PayloadInit( max_t );
        
            TraceRay( scene_tlas,
                ( RAY_FLAG_NONE ),
                0xFF, 0, 1, 0, ray_bounce, payload_bounce );
                
            //if ( all( pixel_id == view_data.cursor_position_px ) )
            //{
            //    AddDebugLine( MakeDebugVector( ray_bounce.Origin, ray_bounce.Direction * payload_bounce.t, COLOR_RED, COLOR_GREEN ) );
            //}
                
            current_hit = GetHitData( payload_bounce );
            
            if ( current_hit.geom_index == INVALID_GEOM_INDEX )
            {
                radiance_accum += GetMissRadiance() * path.bsdf_acc;
                break;
            }
        }
    }
    
    float3 output_color = radiance_accum / _float3( n_samples );
    
    if ( view_data.use_accumulation )
    {
        float4 cur_value = output[pixel_id];
        output[pixel_id] = float4( cur_value.xyz + output_color, asfloat( asuint( cur_value.w ) + 1 ) );
    }
    else
    {
        output[pixel_id] = float4( output_color, asfloat( uint( 1 ) ) );
    }
}

[shader( "miss" )]
void VisibilityRMS( inout Payload payload )
{
}

[shader( "closesthit" )]
void VisibilityRCS( inout Payload payload, in BuiltInTriangleIntersectionAttributes attr )
{
    payload.barycentrics = attr.barycentrics.xy;
    payload.instance_index = InstanceIndex();
    payload.primitive_index = PrimitiveIndex();
    payload.t = RayTCurrent();
}