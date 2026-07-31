/* Master-look mesh PS without RayQuery (SM 6.0 fallback / player). */
cbuffer cbPerObject : register(b0)
{
	float4x4 WVP;
	float4x4 World;
	float4x4 LightVP[4];
	float4 cascadeSplits;
	float4 fogColor;
	float fogStart;
	float fogEnd;
	float receiveShadows;
	float shadowBias;
	float windTime;
	float windAmount;
	float2 padWindAlign;
	float3 sunDir;
	float padSun;
};

Texture2D ObjTexture : register(t0);
SamplerState ObjSamplerState : register(s0);
Texture2D UnusedShadowSlot : register(t1);
SamplerState UnusedShadowSampler : register(s1);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float FogDist : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 color = ObjTexture.Sample(ObjSamplerState, input.TexCoord);
	clip(color.a - 0.01f);

	float fogFactor = saturate((fogEnd - input.FogDist) / (fogEnd - fogStart));
	color.rgb = lerp(fogColor.rgb, color.rgb, fogFactor);
	return color;
}
