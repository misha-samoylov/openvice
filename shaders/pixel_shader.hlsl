cbuffer cbPerObject : register(b0)
{
	float4x4 WVP;
	float4x4 World;
	float4x4 LightVP;
	float4 fogColor;
	float fogStart;
	float fogEnd;
	float receiveShadows;
	float shadowBias;
};

Texture2D ObjTexture : register(t0);
SamplerState ObjSamplerState : register(s0);

Texture2D ShadowMap : register(t1);
SamplerComparisonState ShadowSampler : register(s1);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float FogDist : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
};

float SampleShadow(float3 worldPos)
{
	float4 lightClip = mul(float4(worldPos, 1.0f), LightVP);
	float3 ndc = lightClip.xyz / max(lightClip.w, 1e-6f);

	/* LH clip → UV (Y flip for D3D). */
	float2 uv = float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
	float lit = 1.0f;
	if (uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f && ndc.z >= 0.0f && ndc.z <= 1.0f) {
		float depth = ndc.z - shadowBias;
		float shadow = 0.0f;
		float2 texel = 1.0f / 4096.0f;
		[unroll]
		for (int y = -1; y <= 1; y++) {
			[unroll]
			for (int x = -1; x <= 1; x++) {
				float2 offset = float2(x, y) * texel;
				shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + offset, depth);
			}
		}
		lit = shadow / 9.0f;
	}
	return lit;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 color = ObjTexture.Sample(ObjSamplerState, input.TexCoord);
	/* Kill fully transparent texels so they never write depth or blend. */
	clip(color.a - 0.01f);

	if (receiveShadows > 0.5f) {
		float lit = SampleShadow(input.WorldPos);
		/* Soft ambient floor so shadowed areas stay readable. */
		color.rgb *= lerp(0.625f, 1.0f, lit);
	}

	float fogFactor = saturate((fogEnd - input.FogDist) / (fogEnd - fogStart));
	color.rgb = lerp(fogColor.rgb, color.rgb, fogFactor);
	return color;
}
