cbuffer cbSSAO : register(b0)
{
	float4x4 Proj;       /* camera projection (row-major uploaded transposed) */
	float2 InvFullSize;  /* 1 / full resolution */
	float2 InvHalfSize;  /* 1 / AO resolution */
	float Radius;
	float Bias;
	float Intensity;
	float Power;
	float Proj33;        /* proj._33 before transpose upload helpers */
	float Proj43;        /* proj._43 */
	float Proj11;
	float Proj22;
};

Texture2D DepthTex : register(t0);
SamplerState PointSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

/* Interleaved gradient noise — stable per-pixel rotation. */
float IGN(float2 p)
{
	return frac(52.9829189f * frac(dot(p, float2(0.06711056f, 0.00583715f))));
}

float LinearizeDepth(float d)
{
	/* LH perspective: viewZ = Proj43 / (d - Proj33). */
	return Proj43 / (d - Proj33);
}

float3 ViewPosFromDepth(float2 uv, float d)
{
	float viewZ = LinearizeDepth(d);
	float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
	float3 pos;
	pos.z = viewZ;
	pos.x = ndc.x * viewZ / Proj11;
	pos.y = ndc.y * viewZ / Proj22;
	return pos;
}

float3 ReconstructNormal(float3 pos)
{
	float3 dx = ddx(pos);
	float3 dy = ddy(pos);
	return normalize(cross(dy, dx));
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float2 uv = input.UV;
	float depth = DepthTex.SampleLevel(PointSamp, uv, 0).r;

	/* Sky / clear — no occlusion. */
	if (depth >= 0.9999f)
		return 1.0f.xxxx;

	float3 pos = ViewPosFromDepth(uv, depth);
	float3 normal = ReconstructNormal(pos);

	float ao = 0.0f;
	const int kSamples = 16;
	/* Project world radius into UV space. */
	float2 radiusUV = 0.5f * float2(Proj11, Proj22) * (Radius / max(pos.z, 1e-3f));
	float noise = IGN(uv / InvHalfSize);
	float angleBase = noise * 6.2831853f;

	[unroll]
	for (int i = 0; i < kSamples; i++) {
		float fi = (float)i + 0.5f;
		float angle = angleBase + fi * 2.3999632f; /* golden angle */
		float r = sqrt(fi / (float)kSamples);
		float2 offset = float2(cos(angle), sin(angle)) * radiusUV * r;

		float2 sampleUV = uv + offset;
		float sampleDepth = DepthTex.SampleLevel(PointSamp, sampleUV, 0).r;
		if (sampleDepth >= 0.9999f)
			continue;

		float3 samplePos = ViewPosFromDepth(sampleUV, sampleDepth);
		float3 dir = samplePos - pos;
		float dist = length(dir);
		float3 v = dir / max(dist, 1e-4f);

		float ndotv = saturate(dot(normal, v) - Bias);
		float range = saturate(1.0f - dist / Radius);
		ao += ndotv * range;
	}

	ao = 1.0f - saturate(ao / (float)kSamples * Intensity);
	ao = pow(saturate(ao), Power);
	return ao.xxxx;
}
