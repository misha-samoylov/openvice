/* Screen-space volumetric god rays (GPU Gems 3 radial blur).
 * Samples bright sky toward the projected sun UV + soft sun disc. */
Texture2D SceneTex : register(t0);
Texture2D DepthTex : register(t1);
SamplerState LinearSamp : register(s0);
SamplerState PointSamp : register(s1);

cbuffer GodRaysCB : register(b0)
{
	float2 LightPosUV;
	float Exposure;
	float Decay;
	float Density;
	float Weight;
	float Threshold;
	float Intensity;
	float DepthCutoff;
	float SunOcclusion;
	float2 Pad;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

static const int NUM_SAMPLES = 48;

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float2 uv = input.UV;
	float sunVisible = SunOcclusion;

	/* Soft occlusion only — never hard-kill when looking into open sky. */
	if (LightPosUV.x >= 0.0f && LightPosUV.x <= 1.0f &&
		LightPosUV.y >= 0.0f && LightPosUV.y <= 1.0f) {
		float sunDepth = DepthTex.SampleLevel(PointSamp, LightPosUV, 0).r;
		float occluded = saturate((DepthCutoff - sunDepth) * 40.0f);
		sunVisible *= lerp(1.0f, 0.15f, occluded);
	}

	float2 delta = (uv - LightPosUV) * (Density / (float)NUM_SAMPLES);
	float2 coord = uv;
	float decay = 1.0f;
	float3 accum = float3(0, 0, 0);
	float3 sunTint = float3(1.10f, 0.95f, 0.70f);

	[unroll]
	for (int i = 0; i < NUM_SAMPLES; i++) {
		coord -= delta;

		if (coord.x < 0.0f || coord.x > 1.0f || coord.y < 0.0f || coord.y > 1.0f) {
			decay *= Decay;
			continue;
		}

		float3 sampleColor = SceneTex.SampleLevel(LinearSamp, coord, 0).rgb;
		float depth = DepthTex.SampleLevel(PointSamp, coord, 0).r;
		float sky = saturate((depth - DepthCutoff) * 20.0f);

		float lum = dot(sampleColor, float3(0.2126f, 0.7152f, 0.0722f));
		float bright = saturate((lum - Threshold) / max(1e-3f, 1.0f - Threshold));

		accum += sampleColor * bright * sky * decay * Weight;

		/* Soft sun contribution — keep rays when looking at empty sky, without washout. */
		float2 d = coord - LightPosUV;
		float sunCore = exp(-dot(d, d) * 70.0f);
		accum += sunTint * sunCore * decay * (Weight * 0.35f);

		decay *= Decay;
	}

	float3 rays = accum * Exposure * Intensity * sunVisible * sunTint;
	return float4(rays, 1.0f);
}
