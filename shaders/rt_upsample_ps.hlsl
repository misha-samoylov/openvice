/* Bilinear upsample / copy of an offscreen RT into the back buffer. */
Texture2D Src : register(t0);
SamplerState LinSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return Src.SampleLevel(LinSamp, input.UV, 0);
}
