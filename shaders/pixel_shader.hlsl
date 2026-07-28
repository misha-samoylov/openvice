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
	float2 padWind;
};

Texture2D ObjTexture : register(t0);
SamplerState ObjSamplerState : register(s0);

Texture2DArray ShadowMap : register(t1);
SamplerComparisonState ShadowSampler : register(s1);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float FogDist : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
};

float CascadeBiasScale(int cascade)
{
	float nearSplit = max(cascadeSplits.x, 1e-3f);
	float split = cascadeSplits.x;
	if (cascade == 1) split = cascadeSplits.y;
	else if (cascade == 2) split = cascadeSplits.z;
	else if (cascade >= 3) split = cascadeSplits.w;
	return split / nearSplit;
}

float SampleCascade(float3 worldPos, int cascade)
{
	float4 lightClip = mul(float4(worldPos, 1.0f), LightVP[cascade]);
	float3 ndc = lightClip.xyz / max(lightClip.w, 1e-6f);

	float2 uv = float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
	float lit = 1.0f;
	if (uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f && ndc.z >= 0.0f && ndc.z <= 1.0f) {
		float scale = CascadeBiasScale(cascade);
		float bias = min(shadowBias * scale, 0.00035f * scale);
		float depth = ndc.z - bias;
		float2 texel = 1.0f / 2048.0f;
		float slice = (float)cascade;
		float shadow = ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(uv, slice), depth);
		shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(uv + float2(-0.5f, 0.0f) * texel, slice), depth);
		shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(uv + float2( 0.5f, 0.0f) * texel, slice), depth);
		shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(uv + float2(0.0f, -0.5f) * texel, slice), depth);
		shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(uv + float2(0.0f,  0.5f) * texel, slice), depth);
		lit = shadow / 5.0f;
	}
	return lit;
}

float SampleShadow(float3 worldPos, float viewDepth)
{
	int cascade = 3;
	if (viewDepth < cascadeSplits.x)
		cascade = 0;
	else if (viewDepth < cascadeSplits.y)
		cascade = 1;
	else if (viewDepth < cascadeSplits.z)
		cascade = 2;

	float lit = SampleCascade(worldPos, cascade);

	/* Soft blend across ~10% of the split width into the next cascade. */
	if (cascade < 3) {
		float splitEnd = cascadeSplits.x;
		float splitBegin = 0.0f;
		if (cascade == 1) {
			splitBegin = cascadeSplits.x;
			splitEnd = cascadeSplits.y;
		} else if (cascade == 2) {
			splitBegin = cascadeSplits.y;
			splitEnd = cascadeSplits.z;
		} else if (cascade == 0) {
			splitBegin = 0.0f;
			splitEnd = cascadeSplits.x;
		}
		float width = max(splitEnd - splitBegin, 1e-3f);
		float blendStart = splitEnd - width * 0.10f;
		float t = saturate((viewDepth - blendStart) / max(splitEnd - blendStart, 1e-3f));
		if (t > 0.0f) {
			float litNext = SampleCascade(worldPos, cascade + 1);
			lit = lerp(lit, litNext, t);
		}
	}

	return lit;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 color = ObjTexture.Sample(ObjSamplerState, input.TexCoord);
	clip(color.a - 0.01f);

	if (receiveShadows > 0.5f) {
		float lit = SampleShadow(input.WorldPos, input.FogDist);
		color.rgb *= lerp(0.625f, 1.0f, lit);
	}

	float fogFactor = saturate((fogEnd - input.FogDist) / (fogEnd - fogStart));
	color.rgb = lerp(fogColor.rgb, color.rgb, fogFactor);
	return color;
}
