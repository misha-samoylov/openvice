cbuffer RtBounceCB : register(b0)
{
	float4x4 InvViewProj;
	float3 CamPos;
	float BounceStrength;
	float3 SunDir;
	float SunStrength;
	float3 SkyColor;
	float ShadowBias;
	float MaxRayT;
	float Pad0;
	float2 Pad1;
};

Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
RaytracingAccelerationStructure SceneBVH : register(t2);
SamplerState PointSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float3 ReconstructWorld(float2 uv, float depth)
{
	float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
	float4 clip = float4(ndc, depth, 1.0f);
	float4 world = mul(clip, InvViewProj);
	return world.xyz / max(world.w, 1e-6f);
}

float TraceOccluded(float3 origin, float3 dir, float tMax)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = normalize(dir);
	ray.TMin = 0.0f;
	ray.TMax = tMax;

	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
		RAY_FLAG_FORCE_OPAQUE |
		RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
	q.TraceRayInline(SceneBVH, RAY_FLAG_NONE, 0xFF, ray);
	q.Proceed();
	return (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0f : 1.0f;
}

float TraceBounceSky(float3 origin, float3 dir, float tMax)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = normalize(dir);
	ray.TMin = 0.0f;
	ray.TMax = tMax;

	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
		RAY_FLAG_FORCE_OPAQUE |
		RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
	q.TraceRayInline(SceneBVH, RAY_FLAG_NONE, 0xFF, ray);
	q.Proceed();
	return (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.22f : 1.0f;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float2 uv = input.UV;
	float4 color = SceneColor.SampleLevel(PointSamp, uv, 0);
	float depth = SceneDepth.SampleLevel(PointSamp, uv, 0).r;

	if (depth >= 0.9995f)
		return float4(color.rgb, 0.0f);

	float3 P = ReconstructWorld(uv, depth);
	float3 N = normalize(cross(ddy(P), ddx(P)));
	if (dot(N, CamPos - P) < 0.0f)
		N = -N;

	float3 L = normalize(SunDir);
	float3 V = normalize(CamPos - P);
	float bias = max(ShadowBias, 0.08f);

	/* Ray 1: sun hard shadow on already-rasterized color. */
	float sunVis = TraceOccluded(P + N * bias, L, MaxRayT);

	/* Ray 2: one bounce — sky vs blocked. */
	float3 bounceDir = reflect(-V, N);
	if (dot(bounceDir, N) < 0.05f)
		bounceDir = normalize(N + L);
	float bounceFactor = TraceBounceSky(P + N * bias, bounceDir, MaxRayT * 0.5f);

	float3 shadowed = color.rgb * lerp(0.52f, 1.0f, sunVis);
	float3 bounce = color.rgb * SkyColor * BounceStrength * bounceFactor;
	return float4(saturate(shadowed + bounce), 1.0f);
}
