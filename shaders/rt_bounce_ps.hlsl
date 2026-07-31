/* Master-look RT sun shadows: soft penumbra + alpha punch-through.
 * color *= lerp(0.625, 1.0, lit) in composite. */
cbuffer RtBounceCB : register(b0)
{
	float4x4 InvViewProj;
	float4x4 ViewProj;
	float3 CamPos;
	float BounceStrength;
	float3 SunDir;
	float SunStrength;
	float3 SkyColor;
	float ShadowBias;
	float MaxRayT;
	float AoRadius;
	float ReflectStrength;
	float AoStrength;
};

Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
RaytracingAccelerationStructure SceneBVH : register(t2);

struct RtShadeTri
{
	float3 p0; float pad0;
	float3 p1; float pad1;
	float3 p2; float pad2;
	float2 uv0;
	float2 uv1;
	float2 uv2;
	uint texIndex;
	uint pad3;
};

struct RtInst
{
	uint triStart;
	uint triCount;
	uint pad0;
	uint pad1;
	float4 row0;
	float4 row1;
	float4 row2;
};

StructuredBuffer<RtShadeTri> Tris : register(t3);
StructuredBuffer<RtInst> Insts : register(t4);
Texture2D BindlessTex[] : register(t0, space1);
SamplerState PointSamp : register(s0);
SamplerState LinSamp : register(s1);

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

float3 Hash23(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * float3(0.1031f, 0.1030f, 0.0973f));
	p3 += dot(p3, p3.yxz + 33.33f);
	return frac((p3.xxy + p3.yxx) * p3.zyx);
}

/* Soft shadow with cutout alpha punch-through (no solid card quads). lit=1. */
float TraceSunLitDir(float3 origin, float3 dir, float tMax)
{
	float3 d = normalize(dir);
	float tMin = 0.02f;

	[loop]
	for (uint i = 0; i < 8u; ++i) {
		RayDesc ray;
		ray.Origin = origin;
		ray.Direction = d;
		ray.TMin = tMin;
		ray.TMax = tMax;

		RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
		q.TraceRayInline(SceneBVH, RAY_FLAG_NONE, 0xFF, ray);
		q.Proceed();

		if (q.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
			return 1.0f;

		uint instId = q.CommittedInstanceID();
		uint primId = q.CommittedPrimitiveIndex();
		RtInst inst = Insts[instId];
		if (primId >= inst.triCount)
			return 0.0f;

		RtShadeTri tri = Tris[inst.triStart + primId];
		float a = 1.0f;
		if (tri.texIndex != 0xFFFFFFFFu) {
			float2 bary = q.CommittedTriangleBarycentrics();
			float w = 1.0f - bary.x - bary.y;
			float2 texUv = w * tri.uv0 + bary.x * tri.uv1 + bary.y * tri.uv2;
			a = BindlessTex[NonUniformResourceIndex(tri.texIndex)].SampleLevel(LinSamp, texUv, 0).a;
		}

		/* Opaque / cutout solid — blocks sun. Transparent hole — continue. */
		if (a >= 0.01f)
			return 0.0f;

		float t = q.CommittedRayT();
		tMin = t + max(0.002f, t * 1e-4f);
		if (tMin >= tMax)
			return 1.0f;
	}
	return 0.0f;
}

/* Contact-hardening soft shadow (4 taps). */
float TraceSoftSun(float3 origin, float3 L, float tMax, float2 noise)
{
	float3 up = abs(L.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0);
	float3 t = normalize(cross(L, up));
	float3 b = cross(L, t);

	/* First ray estimates blocker distance for penumbra size. */
	float3 d0 = L;
	float lit0 = TraceSunLitDir(origin, d0, tMax);

	/* If fully lit, skip extra taps. */
	if (lit0 > 0.99f)
		return 1.0f;

	/* Penumbra grows with travel; keep modest for VC scale. */
	float penumbra = 0.022f;
	float2 o = (noise.xy * 2.0f - 1.0f);
	float3 d1 = normalize(L + (t * o.x + b * o.y) * penumbra);
	float3 d2 = normalize(L + (t * -o.y + b * o.x) * penumbra);
	float3 d3 = normalize(L + (t * o.y + b * -o.x) * (penumbra * 0.7f));

	float lit = lit0;
	lit += TraceSunLitDir(origin, d1, tMax);
	lit += TraceSunLitDir(origin, d2, tMax);
	lit += TraceSunLitDir(origin, d3, tMax);
	return lit * 0.25f;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float2 uv = input.UV;
	float depth = SceneDepth.SampleLevel(PointSamp, uv, 0).r;

	if (depth >= 0.9995f)
		return float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3 P = ReconstructWorld(uv, depth);
	float3 N = cross(ddy(P), ddx(P));
	float nLen = length(N);
	N = (nLen > 1e-6f) ? (N / nLen) : float3(0.0f, 1.0f, 0.0f);
	if (dot(N, CamPos - P) < 0.0f)
		N = -N;

	float3 L = normalize(SunDir);
	float bias = max(ShadowBias, 0.06f);
	/* Stronger bias at grazing to reduce acne without losing contact. */
	float ndotl = saturate(abs(dot(N, L)));
	bias = lerp(bias * 2.5f, bias, ndotl);
	float3 origin = P + N * bias + L * (bias * 0.35f);

	float2 noise = Hash23(uv * float2(input.Pos.x, input.Pos.y) + CamPos.xz).xy;
	float lit = TraceSoftSun(origin, L, MaxRayT, noise);

	/* Master CSM darken. */
	float shade = lerp(0.625f, 1.0f, lit);
	return float4(0.0f, 0.0f, 0.0f, shade);
}
