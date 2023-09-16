#include "Utils.hlsli"

#include "ACES.hlsli"

[[vk::binding( 0 )]]
Texture2D<float4> TextureObject;
[[vk::binding( 1 )]]
SamplerState TextureObject_Sampler;

struct DisplayMappingParams
{
    uint method;
	uint show_hue_test_image;
	float white_point;
};
[[vk::binding( 2 )]] ConstantBuffer<DisplayMappingParams> pass_params;

static const uint DISPLAY_MAPPING_METHOD_LINEAR = 0;
static const uint DISPLAY_MAPPING_METHOD_REINHARD = 1;
static const uint DISPLAY_MAPPING_METHOD_REINHARD_WHITE_POINT = 2;
static const uint DISPLAY_MAPPING_METHOD_REINHARD_LUMINANCE = 3;
static const uint DISPLAY_MAPPING_METHOD_REINHARD_JODIE = 4;
static const uint DISPLAY_MAPPING_METHOD_UNCHARTED2 = 5;
static const uint DISPLAY_MAPPING_METHOD_ACES = 6;
static const uint DISPLAY_MAPPING_METHOD_ACESCG = 7;
static const uint DISPLAY_MAPPING_METHOD_AGX = 8;

static const float2 full_screen_triangle_uv[3] =
{
	float2( 0, 0 ),
	float2( 0, 2 ),
	float2( 2, 0 )
};

static const float2 full_screen_triangle_ndc[3] =
{
	float2( -1, 1 ),
	float2( -1, -3 ),
	float2( 3, 1 )
};

float3 HueChartImage( float2 uv, float exposure )
{
    float L = ( uv.x * 48.0f ) / 48.0f - 0.4f;
	float3 color;
	if ( uv.y > 0.8f )
	{
		uv.y = ( uv.y - 0.8f ) * 15.0f;
		float3 red = float3( 1, 0, 0 );
		float3 green = float3( 0, 1, 0 );
		float3 blue = float3( 0, 0, 1 );
		color = exp( 15.0f * L ) * ( uv.y > 2 ? blue : ( uv.y > 1 ? green : red ) );
	} else {
		uv.y /= 0.8f;
		float h = ( 1.0f + 24.0f * uv.y ) / 24.0f * 3.141592f * 2.0f;
		color = cos( h + float3( 0.0f, 1.0f, 2.0f ) * 3.141592f * 2.0f / 3.0f );
		float3 maxRGB = max( color.r, max( color.g, color.b ) );
		float minRGB = min( color.r, min( color.g, color.b ) );
		color = exp( 15.0f * L ) * ( color - minRGB ) / ( maxRGB - minRGB );
	}
    
    
    return max( color, _float3( 0.0f ) ) * pow( 2.0f, exposure );
}

float3 AgxDefaultContrastApprox( float3 x )
{
  float3 x2 = x * x;
  float3 x4 = x2 * x2;
  
  return + 15.5     * x4 * x2
         - 40.14    * x4 * x
         + 31.96    * x4
         - 6.868    * x2 * x
         + 0.4298   * x2
         + 0.1191   * x
         - 0.00232;
}

float3 Agx( float3 val, float exposure_bias )
{
  const float3x3 agx_mat = float3x3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992,  0.878468636469772,  0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104 );
    
  const float min_ev = -12.47393f + exposure_bias;
  const float max_ev = 4.026069f + exposure_bias;

  // Input transform (inset)
  val = mul( val, agx_mat );
  
  // Log2 space encoding
  val = clamp( log2( val ), min_ev, max_ev );
  val = (val - min_ev) / (max_ev - min_ev);
  
  // Apply sigmoid function approximation
  val = AgxDefaultContrastApprox( val );

  return val;
}

float3 AgxEotf( float3 val )
{
  const float3x3 agx_mat_inv = float3x3(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116 );
    
  // Inverse input transform (outset)
  val = saturate( mul( val, agx_mat_inv ) );
  
  // sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
  // NOTE: We're linearizing the output here. Comment/adjust when
  // *not* using a sRGB render target
  val = pow( val, _float3( 2.2f ) );

  return val;
}

//#define AGX_LOOK 1

float3 AgxLook( float3 val )
{
  const float3 lw = float3( 0.2126, 0.7152, 0.0722 );
  float luma = dot(val, lw);
  
  // Default
  float3 offset = _float3( 0.0 );
  float3 slope = _float3( 1.0 );
  float3 power = _float3( 1.0 );
  float sat = 1.0;
 
#if AGX_LOOK == 1
  // Golden
  slope = float3( 1.0, 0.9, 0.5 );
  power = _float3( 0.8 );
  sat = 0.8f;
#elif AGX_LOOK == 2
  // Punchy
  slope = _float3(1.0);
  power = float3(1.35, 1.35, 1.35);
  sat = 1.4;
#endif
  
  // ASC CDL
  val = pow( val * slope + offset, power );
  return luma + sat * ( val - luma );
}

float3 ApplyAgx( float3 v, float exposure_bias )
{
	v = Agx( v, exposure_bias );
#if defined( AGX_LOOK )
	v = AgxLook( v );
#endif
	v = AgxEotf( v );
	
	return v;
}

float3 TonemapUncharted2Partial( float3 x )
{
	float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ( ( x * ( A * x + C * B ) + D * E ) / ( x * ( A * x + B ) + D * F ) ) - E / F;
}

[[vk::location( 0 )]] float4 DisplayMappingVS( in uint vertex_id : SV_VertexID, [[vk::location( 1 )]] out float2 uv : TEXCOORD0 ) : SV_POSITION
{
	float4 pos = float4( full_screen_triangle_ndc[vertex_id], 0.5f, 1 );
	uv = full_screen_triangle_uv[vertex_id];
	return pos;
}

[[vk::location( 0 )]] float4 DisplayMappingPS( [[vk::location( 1 )]] in float2 uv : TEXCOORD0 ) : SV_TARGET0
{
	float3 input_linear = TextureObject.Sample( TextureObject_Sampler, uv ).rgb;
	if ( pass_params.show_hue_test_image )
	{
		input_linear = HueChartImage( uv, 1.0f );
	}
	
    float3 output = float3( 0, 1, 0 );
	
	uint method = pass_params.method;	
	
	if ( method != DISPLAY_MAPPING_METHOD_REINHARD_WHITE_POINT && method != DISPLAY_MAPPING_METHOD_AGX )
	{
		input_linear /= pass_params.white_point;	  
	}
	
	if ( method == DISPLAY_MAPPING_METHOD_LINEAR )
    {
        output = input_linear;
    }
	else if ( method == DISPLAY_MAPPING_METHOD_REINHARD )
    {
		output = input_linear / ( _float3( 1.0f ) + input_linear );
    }
	else if ( method == DISPLAY_MAPPING_METHOD_REINHARD_WHITE_POINT )
    {
		output = input_linear * ( _float3( 1.0f ) + input_linear / sqr( pass_params.white_point ) ) / ( _float3( 1.0f ) + input_linear );
    }
	else if ( method == DISPLAY_MAPPING_METHOD_REINHARD_LUMINANCE )
    {
		float linear_brightness = CalcLinearBrightness( input_linear );
		float reihhard_brightness = linear_brightness / ( 1.0f + linear_brightness );
		
		output = input_linear / ( linear_brightness + FLT_EPSILON ) * reihhard_brightness;
    }
	else if ( method == DISPLAY_MAPPING_METHOD_REINHARD_JODIE )
    {
		float linear_brightness = CalcLinearBrightness( input_linear );
		float3 tv = input_linear / ( _float3( 1.0f ) + input_linear );
		
		output = lerp( input_linear / ( 1.0f + linear_brightness ), tv, tv );
    }
	else if ( method == DISPLAY_MAPPING_METHOD_UNCHARTED2 )
    {
		float3 curr = TonemapUncharted2Partial( input_linear * 2.0f );
		float3 w = _float3( 11.2f );
		float3 white_scale = _float3( 1.0f ) / TonemapUncharted2Partial( w );
		
		output = curr * white_scale;
    }
	else if ( method == DISPLAY_MAPPING_METHOD_ACES )
	{
		output = ACESFitted( input_linear );
	}
	else if ( method == DISPLAY_MAPPING_METHOD_ACESCG )
	{
		output = ACEScgRefMapping( input_linear );
	}
	else if ( method == DISPLAY_MAPPING_METHOD_AGX )
	{
		output = ApplyAgx( input_linear, log2( pass_params.white_point ) );
	}

    return float4( output, 1.0f );
}