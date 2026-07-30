cbuffer RtFullCB : register(b0)
{
	float4x4 InvViewProj;
	float3 CamPos;
	float SeaLevelY;
	float3 SunDir;
	float SunStrength;
	float3 SkyColor;
	float ShadowBias;
	float2 ScreenSize;
	float MaxRayT;
	float Ambient;
	float Time;
	float SunCos;
	float2 Pad1;
};

RaytracingAccelerationStructure SceneBVH : register(t0);

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
};

StructuredBuffer<RtShadeTri> Tris : register(t1);
StructuredBuffer<RtInst> Insts : register(t2);
Texture2D BindlessTex[] : register(t0, space1);
SamplerState LinSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float TraceShadow(float3 origin, float3 dir, float tMax)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = normalize(dir);
	ray.TMin = 0.02f;
	ray.TMax = tMax;

	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
		RAY_FLAG_FORCE_OPAQUE |
		RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
	q.TraceRayInline(SceneBVH, RAY_FLAG_NONE, 0xFF, ray);
	q.Proceed();
	return (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0f : 1.0f;
}

float3 EvalSky(float3 dir)
{
	float3 d = normalize(dir);
	float elev = saturate(d.y * 0.5f + 0.5f);
	float3 zenith = SkyColor * 1.05f;
	float3 horizon = float3(0.72f, 0.82f, 0.92f);
	float3 col = lerp(horizon, zenith, elev * elev);

	float3 L = normalize(SunDir);
	float mu = saturate(dot(d, L));
	/* Soft glow around the sun. */
	float glow = pow(mu, 48.0f) * 0.55f;
	float3 sunTint = float3(1.0f, 0.92f, 0.75f);
	col += sunTint * glow * SunStrength;

	/* Hard sun disc. */
	if (mu > SunCos && L.y > -0.05f) {
		float core = saturate((mu - SunCos) / max(1.0f - SunCos, 1e-4f));
		col += sunTint * (1.6f + 1.2f * core) * SunStrength;
	}
	return saturate(col);
}

bool IntersectSea(float3 origin, float3 dir, float seaY, out float tHit, out float3 P)
{
	tHit = 0.0f;
	P = 0.0f;
	if (abs(dir.y) < 1e-5f)
		return false;
	float t = (seaY - origin.y) / dir.y;
	if (t < 0.05f || t > MaxRayT)
		return false;
	tHit = t;
	P = origin + dir * t;
	return true;
}

float3 ShadeWater(float3 P, float3 V, float3 L)
{
	float3 N = float3(0.0f, 1.0f, 0.0f);
	/* Mild animated ripples for sparkle (normal only — plane stay flat for hits). */
	float rip = sin(P.x * 0.11f + Time * 1.3f) * cos(P.z * 0.09f + Time * 0.9f);
	N = normalize(float3(rip * 0.08f, 1.0f, rip * 0.06f));

	float3 R = reflect(-V, N);
	float3 skyRefl = EvalSky(R);

	float fres = pow(1.0f - saturate(dot(N, V)), 4.0f);
	fres = lerp(0.12f, 0.92f, fres);

	float3 deep = float3(0.02f, 0.12f, 0.18f);
	float3 shallow = float3(0.05f, 0.28f, 0.32f);
	float3 waterAlb = lerp(deep, shallow, 0.45f);

	float shadow = TraceShadow(P + N * 0.15f, L, MaxRayT);
	float ndotl = saturate(dot(N, L));
	float3 diffuse = waterAlb * (0.25f + 0.55f * ndotl * shadow);

	float spec = pow(saturate(dot(R, L)), 96.0f) * shadow * SunStrength;
	float3 sunSpec = float3(1.0f, 0.95f, 0.85f) * spec * 2.2f;

	return saturate(lerp(diffuse, skyRefl, fres) + sunSpec);
}

float3 ShadeMesh(float3 P, float3 N, float3 V, float3 L, float4 albedo)
{
	float ndotl = saturate(dot(N, L));
	float bias = max(ShadowBias, 0.06f);
	float shadow = TraceShadow(P + N * bias, L, MaxRayT);
	float3 lit = albedo.rgb * (Ambient + SunStrength * ndotl * shadow);
	/* Cheap sky bounce on top faces. */
	lit += albedo.rgb * SkyColor * 0.08f * saturate(N.y);
	return saturate(lit);
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float2 uv = input.UV;
	float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
	float4 nearH = mul(float4(ndc, 0.0f, 1.0f), InvViewProj);
	float4 farH = mul(float4(ndc, 1.0f, 1.0f), InvViewProj);
	float3 nearW = nearH.xyz / max(nearH.w, 1e-6f);
	float3 farW = farH.xyz / max(farH.w, 1e-6f);
	float3 dir = normalize(farW - CamPos);

	RayDesc primary;
	primary.Origin = CamPos;
	primary.Direction = dir;
	primary.TMin = 0.05f;
	primary.TMax = MaxRayT;

	RayQuery<RAY_FLAG_FORCE_OPAQUE |
		RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
	q.TraceRayInline(SceneBVH, RAY_FLAG_NONE, 0xFF, primary);
	q.Proceed();

	float meshT = MaxRayT + 1.0f;
	float3 meshP = 0.0f;
	float3 meshN = float3(0, 1, 0);
	float4 meshAlb = 0.0f;
	bool hitMesh = false;

	if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
		uint instId = q.CommittedInstanceID();
		uint primId = q.CommittedPrimitiveIndex();
		RtInst inst = Insts[instId];
		if (primId < inst.triCount) {
			RtShadeTri tri = Tris[inst.triStart + primId];
			float2 bary = q.CommittedTriangleBarycentrics();
			float w = 1.0f - bary.x - bary.y;
			meshP = w * tri.p0 + bary.x * tri.p1 + bary.y * tri.p2;
			float2 texUv = w * tri.uv0 + bary.x * tri.uv1 + bary.y * tri.uv2;
			meshN = normalize(cross(tri.p1 - tri.p0, tri.p2 - tri.p0));
			if (dot(meshN, -dir) < 0.0f)
				meshN = -meshN;
			meshAlb = float4(0.65f, 0.65f, 0.65f, 1.0f);
			if (tri.texIndex != 0xFFFFFFFFu)
				meshAlb = BindlessTex[NonUniformResourceIndex(tri.texIndex)].SampleLevel(LinSamp, texUv, 0);
			if (meshAlb.a >= 0.01f) {
				hitMesh = true;
				meshT = q.CommittedRayT();
			}
		}
	}

	float waterT = MaxRayT + 1.0f;
	float3 waterP = 0.0f;
	bool hitWater = IntersectSea(CamPos, dir, SeaLevelY, waterT, waterP);

	float3 L = normalize(SunDir);
	float3 V = normalize(-dir);

	/* Prefer closer hit between mesh and sea plane. */
	if (hitWater && (!hitMesh || waterT < meshT))
		return float4(ShadeWater(waterP, V, L), 1.0f);

	if (hitMesh)
		return float4(ShadeMesh(meshP, meshN, V, L, meshAlb), 1.0f);

	return float4(EvalSky(dir), 1.0f);
}
