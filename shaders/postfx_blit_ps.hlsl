/* Sample previous/current frame * Color, with optional UV offset (motion trails). */
Texture2D SceneTex : register(t0);
SamplerState PointSamp : register(s0);

cbuffer BlitCB : register(b0)
{
	float4 Color;
	float2 UVOffset;
	float2 _pad;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return SceneTex.Sample(PointSamp, input.UV + UVOffset) * Color;
}
