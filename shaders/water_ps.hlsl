cbuffer cbWater : register(b0)
{
	float4x4 WVP;
	float2 uvScroll;
	float fogStart;
	float fogEnd;
	float4 tint;
	float4 fogColor;
};

Texture2D WaterTexture : register(t0);
SamplerState WaterSampler : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float FogDist : TEXCOORD1;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 tex = WaterTexture.Sample(WaterSampler, input.Tex);
	float3 rgb = tex.rgb * tint.rgb;
	float a = saturate(tex.a * tint.a);

	float fogFactor = saturate((fogEnd - input.FogDist) / (fogEnd - fogStart));
	rgb = lerp(fogColor.rgb, rgb, fogFactor);
	return float4(rgb, a);
}
