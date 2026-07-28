cbuffer cbWater : register(b0)
{
	float4x4 WVP;
	float2 uvScroll;
	float fogStart;
	float fogEnd;
	float4 tint;
	float4 fogColor;
	float3 cameraPos;
	float time;
	float3 sunDir;
	float cloudReflect;
	float windSpeed;
	float cloudCoverage;
	float cloudDensity;
	float _pad1;
};

struct VS_INPUT
{
	float3 Pos : POSITION;
	float2 Tex : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float FogDist : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
};

/* Visible rolling swells — same phases as PS normals. */
float WaveHeight(float2 xz, float t)
{
	float2 d0 = float2(0.96f, 0.28f);
	float2 d1 = float2(-0.42f, 0.91f);
	float2 d2 = float2(0.71f, -0.70f);
	float2 d3 = float2(-0.85f, -0.52f);
	float2 dSwell = float2(0.98f, 0.18f);

	float h = 0.0f;
	h += sin(dot(xz, dSwell) * 0.048f + t * 0.55f) * 0.18f;
	h += sin(dot(xz, d0) * 0.085f + t * 0.72f) * 0.22f;
	h += sin(dot(xz, d1) * 0.12f  + t * 0.58f) * 0.16f;
	h += sin(dot(xz, d2) * 0.21f  + t * 0.95f) * 0.10f;
	h += sin(dot(xz, d3) * 0.33f  + t * 0.80f) * 0.07f;
	return h;
}

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	float3 worldPos = input.Pos;
	worldPos.y += WaveHeight(worldPos.xz, time);

	output.Pos = mul(float4(worldPos, 1.0f), WVP);
	/* Base UV + continuous flow so texture crawls even on huge tiles */
	output.Tex = input.Tex + uvScroll + float2(time * 0.028f, time * 0.019f);
	output.FogDist = output.Pos.w;
	output.WorldPos = worldPos;
	return output;
}
