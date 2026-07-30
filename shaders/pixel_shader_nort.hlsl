/* Non-RT fallback (SM 5.1) — used when DXR PSO creation fails. */
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

/* Screen-space geometric normal (same as RT path; no vertex normals). */
float3 GeometricNormal(float3 worldPos)
{
	float3 dx = ddx(worldPos);
	float3 dy = ddy(worldPos);
	float3 n = cross(dy, dx);
	float len = length(n);
	return (len > 1e-8f) ? (n / len) : float3(0.0f, 1.0f, 0.0f);
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 color = ObjTexture.Sample(ObjSamplerState, input.TexCoord);
	clip(color.a - 0.01f);

	/* Analytical sun (no RayQuery) — used only if RT PSO fails. */
	float3 N = GeometricNormal(input.WorldPos);
	float3 L = normalize(sunDir);
	float ndotl = max(saturate(dot(N, L)), saturate(dot(-N, L)) * 0.35f);
	float3 ambient = float3(0.22f, 0.25f, 0.32f);
	float3 sunColor = float3(1.15f, 1.05f, 0.92f);
	color.rgb *= ambient + sunColor * ndotl;

	float fogFactor = saturate((fogEnd - input.FogDist) / (fogEnd - fogStart));
	color.rgb = lerp(fogColor.rgb, color.rgb, fogFactor);
	return color;
}
