struct PixelIn
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
};

float4 main( PixelIn pin ) : SV_TARGET
{
	return pin.color;
}