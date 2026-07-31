/* Master-look RT sun shadows + RTAO.
 * Composite: color *= lerp(0.625, 1.0, lit) * ao. */
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

/* Soft shadow with cutout alpha punch-through (no solid card quads). lit=1.
 * maxSteps limits work for short AO rays. */
float TraceOccludedDir(float3 origin, float3 dir, float tMax, uint maxSteps)
{
	float3 d = normalize(dir);
	float tMin = 0.02f;

	[loop]
	for (uint i = 0; i < maxSteps; ++i) {
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

		if (a >= 0.01f)
			return 0.0f;

		float t = q.CommittedRayT();
		tMin = t + max(0.002f, t * 1e-4f);
		if (tMin >= tMax)
			return 1.0f;
	}
	return 0.0f;
}

float TraceSunLitDir(float3 origin, float3 dir, float tMax)
{
	return TraceOccludedDir(origin, dir, tMax, 8u);
}

/* Soft sun shadow: fixed Vogel disk (stable) + light noise rotation. */
float TraceSoftSun(float3 origin, float3 L, float tMax, float2 noise)
{
	float3 up = abs(L.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0);
	float3 t = normalize(cross(L, up));
	float3 b = cross(L, t);

	float lit0 = TraceSunLitDir(origin, L, tMax);
	if (lit0 > 0.99f)
		return 1.0f;

	/* Modest penumbra — large random offsets were the main noise source. */
	const float penumbra = 0.016f;
	const float golden = 2.3999632f; /* 2*pi / golden ratio */
	float rot = noise.x * 6.2831853f;

	float lit = lit0;
	const int kShadowSamples = 8;
	[unroll]
	for (int i = 1; i < kShadowSamples; ++i) {
		float r = sqrt(((float)i + 0.5f) / (float)kShadowSamples);
		float a = (float)i * golden + rot;
		float2 d = float2(cos(a), sin(a)) * r * penumbra;
		float3 dir = normalize(L + t * d.x + b * d.y);
		lit += TraceSunLitDir(origin, dir, tMax);
	}
	return lit * (1.0f / (float)kShadowSamples);
}

/* Ray-traced AO: cosine hemisphere, distance-weighted, alpha-aware. */
float TraceRTAO(float3 origin, float3 N, float radius, float2 noise)
{
	float3 t = normalize(cross(N, abs(N.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0)));
	float3 b = cross(N, t);

	float ao = 0.0f;
	const int kAoSamples = 8;
	[unroll]
	for (int i = 0; i < kAoSamples; ++i) {
		float2 u = Hash23(noise + float2(i * 19.7f, i * 11.3f)).xy;
		float phi = u.x * 6.2831853f;
		float cosTheta = sqrt(u.y);
		float sinTheta = sqrt(1.0f - u.y);
		float3 dir = normalize(
			(cos(phi) * sinTheta) * t +
			(sin(phi) * sinTheta) * b +
			cosTheta * N);

		float3 d = normalize(dir);
		RayDesc ray;
		ray.Origin = origin;
		ray.Direction = d;
		ray.TMin = 0.02f;
		ray.TMax = radius;

		RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
		q.TraceRayInline(SceneBVH, RAY_FLAG_NONE, 0xFF, ray);
		q.Proceed();

		float vis = 1.0f;
		if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
			uint instId = q.CommittedInstanceID();
			uint primId = q.CommittedPrimitiveIndex();
			RtInst inst = Insts[instId];
			bool solid = true;
			if (primId < inst.triCount) {
				RtShadeTri tri = Tris[inst.triStart + primId];
				if (tri.texIndex != 0xFFFFFFFFu) {
					float2 bary = q.CommittedTriangleBarycentrics();
					float w = 1.0f - bary.x - bary.y;
					float2 texUv = w * tri.uv0 + bary.x * tri.uv1 + bary.y * tri.uv2;
					float a = BindlessTex[NonUniformResourceIndex(tri.texIndex)].SampleLevel(LinSamp, texUv, 0).a;
					solid = (a >= 0.01f);
				}
			}
			if (solid) {
				float hitT = q.CommittedRayT();
				float wOcc = saturate(1.0f - hitT / max(radius, 1e-3f));
				/* Softer curve — less salt-and-pepper than linear. */
				vis = 1.0f - (wOcc * wOcc);
			}
		}
		ao += vis;
	}
	return ao * (1.0f / (float)kAoSamples);
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
	float ndotl = saturate(abs(dot(N, L)));
	bias = lerp(bias * 2.5f, bias, ndotl);
	float3 origin = P + N * bias + L * (bias * 0.35f);
	float3 aoOrigin = P + N * max(bias, 0.05f);

	float2 noise = Hash23(uv * float2(input.Pos.x, input.Pos.y) + CamPos.xz).xy;
	float lit = 1.0f;
	if (SunStrength > 0.5f)
		lit = TraceSoftSun(origin, L, MaxRayT, noise);

	float ao = 1.0f;
	if (AoStrength > 0.001f && AoRadius > 0.001f)
		ao = TraceRTAO(aoOrigin, N, AoRadius, noise + 0.71f);
	ao = lerp(1.0f, ao, saturate(AoStrength));

	/* r = AO (blurred in composite), a = sun shadow darken (kept sharper). */
	float shadow = lerp(0.625f, 1.0f, lit);
	return float4(saturate(ao), 0.0f, 0.0f, saturate(shadow));
}
