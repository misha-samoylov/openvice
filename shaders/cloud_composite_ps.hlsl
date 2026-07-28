/* Upsample half-res volumetric cloud buffer onto the main target. */
Texture2D CloudTex : register(t0);
SamplerState LinearSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return CloudTex.Sample(LinearSamp, input.UV);
}
