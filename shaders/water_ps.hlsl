cbuffer cbWater : register(b0)
{
	float4x4 WVP;
	float2 uvScroll;
	float2 pad;
	float4 tint;
};

Texture2D WaterTexture : register(t0);
SamplerState WaterSampler : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 tex = WaterTexture.Sample(WaterSampler, input.Tex);
	float3 rgb = tex.rgb * tint.rgb;
	float a = saturate(tex.a * tint.a);
	return float4(rgb, a);
}
