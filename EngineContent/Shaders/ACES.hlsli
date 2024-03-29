#pragma once

// HLSL port of ACES color transforms

static const float2x4 REC709_PRI =
{
    { 0.64000f,  0.33000f },
    { 0.30000f,  0.60000f },
    { 0.15000f,  0.06000f },
    { 0.31270f,  0.32900f }
};

static const float3x3 RGB_TO_XYZ =
{
    { 0.4124564f, 0.2126729f, 0.0193339f },
    { 0.3575761f, 0.7151522f, 0.1191920f },
    { 0.1804375f, 0.0721750f, 0.9503041f }
};

static const float3x3 XYZ_TO_RGB =
{
     { 3.2404542f, -0.9692660f, 0.0556434f },
     { -1.5371385f, 1.8760108f, -0.2040259f },
     { -0.4985314f, 0.0415560f, 1.0572252f }
};

// Bradford CAT (https://www.colour-science.org:8010/apps/rgb_colourspace_transformation_matrix)
static const float3x3 sRGB_TO_ACEScg =
{
    { 0.6131324224f, 0.3395380158f, 0.0474166960f },
    { 0.0701243808f, 0.9163940113f, 0.0134515240f },
    { 0.0205876575f, 0.1095745716f, 0.8697854040f }
};

static const float3x3 ACEScg_TO_sRGB =
{
	{ 1.7048586763f, -0.6217160219f, -0.0832993717f },
	{ -0.1300768242f, 1.1407357748f, -0.0105598017f },
	{ -0.0239640729f, -0.1289755083f, 1.1530140189f }
};

float3 sRGBLinearToACEScg( float3 srgb_linear )
{
    return mul( sRGB_TO_ACEScg, srgb_linear );    
}

float3 ACEScgTosRGBLinear( float3 aces_cg )
{
    return mul( ACEScg_TO_sRGB, aces_cg );
}

float3 RttAndOdtFit( float3 x )
{
	float3 a = x * ( x + 0.0245786f ) - 0.000090537f;
    float3 b = x * ( 0.983729f * x + 0.4329510f ) + 0.238081f;
    return a / b;
}

float3 ACESFitted( float3 x )
{	
	const float3x3 aces_input_tf =
	{
		{0.59719, 0.35458, 0.04823},
		{0.07600, 0.90834, 0.01566},
		{0.02840, 0.13383, 0.83777}
	};

	const float3x3 aces_output_tf =
	{
		{ 1.60475, -0.53108, -0.07367},
		{-0.10208,  1.10813, -0.00605},
		{-0.00327, -0.07276,  1.07602}
	};
	
	x = mul( aces_input_tf, x );
	
	x = RttAndOdtFit( x );
	
	return saturate( mul( aces_output_tf, x ) );
}

float3 ACEScgRefMapping( float3 srgb_linear )
{
	float3 aces_cg = sRGBLinearToACEScg( srgb_linear );
	
	aces_cg = RttAndOdtFit( aces_cg );
	
	return saturate( ACEScgTosRGBLinear( aces_cg ) );
}
