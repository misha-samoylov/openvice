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
	float _pad0;
};

Texture2D WaterTexture : register(t0);
SamplerState WaterSampler : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float FogDist : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
};

/* Low-amplitude multi-wave normals — calm lake / bay, not open-sea chop. */
float3 CalmRippleNormal(float2 xz, float t)
{
	float3 n = float3(0.0f, 1.0f, 0.0f);

	/* Long soft swells (+5% livelier) */
	float2 d0 = float2(0.96f, 0.28f);
	float2 d1 = float2(-0.42f, 0.91f);
	float2 d2 = float2(0.71f, -0.70f);
	float2 d3 = float2(-0.85f, -0.52f);

	n.xz += d0 * cos(dot(xz, d0) * 0.085f + t * 0.231f) * 0.0294f;
	n.xz += d1 * cos(dot(xz, d1) * 0.12f  + t * 0.189f) * 0.0231f;
	n.xz += d2 * cos(dot(xz, d2) * 0.21f  + t * 0.3255f) * 0.0168f;
	n.xz += d3 * cos(dot(xz, d3) * 0.33f  + t * 0.2835f) * 0.0126f;

	/* Fine surface glitter / micro-ripples */
	n.xz += float2(0.6f, 0.8f) * cos(dot(xz, float2(0.6f, 0.8f)) * 0.55f + t * 0.4725f) * 0.0084f;
	n.xz += float2(-0.75f, 0.55f) * cos(dot(xz, float2(-0.75f, 0.55f)) * 0.72f + t * 0.399f) * 0.0063f;

	return normalize(n);
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float3 worldPos = input.WorldPos;
	float3 viewDir = normalize(cameraPos - worldPos);
	float3 N = CalmRippleNormal(worldPos.xz, time);

	/* Subtle UV warp from ripples — keeps the tiled waterclear texture alive */
	float2 warp = N.xz * 0.03675f;
	float2 uvA = input.Tex + warp;
	float2 uvB = input.Tex * 1.73f - uvScroll * 0.55f - warp * 0.6f;

	float4 texA = WaterTexture.Sample(WaterSampler, uvA);
	float4 texB = WaterTexture.Sample(WaterSampler, uvB);
	float3 texColor = lerp(texA.rgb, texB.rgb, 0.35f);

	/* Body / reflection colors — 30% darker vs previous */
	float3 deepColor = float3(0.056f, 0.1568f, 0.2016f);
	float3 shallowColor = texColor * tint.rgb;
	float3 skyColor = fogColor.rgb * 1.05f;

	float NdotV = saturate(dot(N, viewDir));
	/* Schlick-style fresnel; water F0 ≈ 0.02, slightly boosted for readability */
	float F0 = 0.035f;
	float fresnel = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);

	float3 waterBody = lerp(deepColor, shallowColor, 0.55f + 0.25f * NdotV);
	float3 color = lerp(waterBody, skyColor, fresnel * 0.72f);

	/* Soft, tight specular for calm water */
	float3 L = normalize(sunDir);
	float3 H = normalize(L + viewDir);
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));
	float spec = pow(NdotH, 420.0f) * NdotL;
	/* Broad soft glint under the tight highlight */
	spec += pow(NdotH, 48.0f) * NdotL * 0.084f;
	color += float3(0.92f, 0.96f, 1.0f) * spec * 0.5775f;

	/* Looking straight down: more of the textured body; horizon: more sky */
	float alpha = saturate(lerp(0.58f, 0.90f, fresnel) * tint.a * lerp(texA.a, texB.a, 0.35f));
	/* 30% more opaque */
	alpha = saturate(lerp(alpha, 1.0f, 0.30f));

	float fogFactor = saturate((fogEnd - input.FogDist) / (fogEnd - fogStart));
	color = lerp(fogColor.rgb, color, fogFactor);

	return float4(color, alpha);
}
