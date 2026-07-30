/* Full-scene RayQuery: mesh + sea plane + volumetric cloud slab + sun.
 * Cloud/water look matches Clouds.h + water_ps.hlsl / cloud_ps.hlsl. */
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
	float2 WaterUvScroll;
	uint WaterTexIndex;
	float FogStart;
	float FogEnd;
	float PadWater;
	float4 WaterTint;
	float4 FogColor;
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
	float4 row0;
	float4 row1;
	float4 row2;
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

float3 XformPoint(RtInst inst, float3 p)
{
	return float3(
		dot(inst.row0.xyz, p) + inst.row0.w,
		dot(inst.row1.xyz, p) + inst.row1.w,
		dot(inst.row2.xyz, p) + inst.row2.w);
}

float3 XformNormal(RtInst inst, float3 n)
{
	return normalize(float3(
		dot(inst.row0.xyz, n),
		dot(inst.row1.xyz, n),
		dot(inst.row2.xyz, n)));
}

/* ---- Match Clouds.h ---- */
static const float CLOUD_BOTTOM = 140.0f;
static const float CLOUD_TOP = 320.0f;
static const float CLOUD_COVERAGE = 0.48f;
static const float CLOUD_DENSITY = 0.055f;
static const float CLOUD_ABSORPTION = 0.55f;
static const float CLOUD_WIND = 12.0f;
static const float CLOUD_AMBIENT = 1.15f;
static const float3 CLOUD_SILVER = float3(1.15f, 1.08f, 0.98f);

static const int CLOUD_PRIMARY_STEPS = 20;
static const float CLOUD_MAX_STEP = 18.0f;
static const int CLOUD_REFLECT_STEPS = 10;

/* Soft shadow visibility: 1 = lit. Punch through leaf cutout holes
   (same threshold as shadow_ps / primary TracePrimaryCutout). */
float TraceShadow(float3 origin, float3 dir, float tMax)
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

		RayQuery<RAY_FLAG_FORCE_OPAQUE |
			RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
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

float Hash13(float3 p)
{
	p = frac(p * 0.1031f);
	p += dot(p, p.yzx + 33.33f);
	return frac((p.x + p.y) * p.z);
}

float ValueNoise(float3 x)
{
	float3 i = floor(x);
	float3 f = frac(x);
	f = f * f * (3.0f - 2.0f * f);

	float n000 = Hash13(i + float3(0, 0, 0));
	float n100 = Hash13(i + float3(1, 0, 0));
	float n010 = Hash13(i + float3(0, 1, 0));
	float n110 = Hash13(i + float3(1, 1, 0));
	float n001 = Hash13(i + float3(0, 0, 1));
	float n101 = Hash13(i + float3(1, 0, 1));
	float n011 = Hash13(i + float3(0, 1, 1));
	float n111 = Hash13(i + float3(1, 1, 1));

	float nx00 = lerp(n000, n100, f.x);
	float nx10 = lerp(n010, n110, f.x);
	float nx01 = lerp(n001, n101, f.x);
	float nx11 = lerp(n011, n111, f.x);
	return lerp(lerp(nx00, nx10, f.y), lerp(nx01, nx11, f.y), f.z);
}

float Fbm3(float3 p)
{
	float sum = 0.0f;
	float amp = 0.5f;
	[unroll]
	for (int i = 0; i < 3; i++) {
		sum += amp * ValueNoise(p);
		p = p * 2.02f + 17.1f;
		amp *= 0.5f;
	}
	return sum;
}

float Fbm2(float3 p)
{
	float sum = 0.0f;
	float amp = 0.5f;
	[unroll]
	for (int i = 0; i < 2; i++) {
		sum += amp * ValueNoise(p);
		p = p * 2.02f + 17.1f;
		amp *= 0.5f;
	}
	return sum;
}

float Remap(float v, float lo, float hi, float nlo, float nhi)
{
	return nlo + (v - lo) / max(1e-5f, hi - lo) * (nhi - nlo);
}

float IGN(float2 p)
{
	return frac(52.9829189f * frac(dot(p, float2(0.06711056f, 0.00583715f))));
}

bool IntersectCloudSlab(float3 ro, float3 rd, float y0, float y1, out float tEnter, out float tExit)
{
	if (abs(rd.y) < 1e-5f) {
		if (ro.y < y0 || ro.y > y1)
			return false;
		tEnter = 0.0f;
		tExit = 1800.0f;
		return true;
	}

	float t0 = (y0 - ro.y) / rd.y;
	float t1 = (y1 - ro.y) / rd.y;
	float tNear = min(t0, t1);
	float tFar = max(t0, t1);
	if (tFar < 0.0f)
		return false;

	tEnter = max(tNear, 0.0f);
	tExit = min(tFar, 1800.0f);
	return tEnter < tExit;
}

float SampleDensity(float3 p, bool highQuality)
{
	float h = saturate((p.y - CLOUD_BOTTOM) / max(1.0f, CLOUD_TOP - CLOUD_BOTTOM));
	float heightGrad = saturate(h * 4.0f) * saturate((1.0f - h) * 3.2f);

	float3 wind = float3(Time * CLOUD_WIND, 0.0f, Time * CLOUD_WIND * 0.35f);
	float3 q = (p + wind) * 0.0011f;
	q.y *= 1.65f;

	float base = highQuality ? Fbm3(q) : Fbm2(q);
	float cover = saturate(CLOUD_COVERAGE);
	float shape = saturate(Remap(base, 1.0f - cover, 1.0f, 0.0f, 1.0f));
	float detail = ValueNoise(q * 3.4f + 19.7f);
	float dens = saturate(shape - detail * 0.22f * (1.0f - shape));
	return dens * heightGrad * CLOUD_DENSITY;
}

float CheapLight(float3 p, float dens)
{
	float toTop = max(0.0f, CLOUD_TOP - p.y);
	float sunY = max(SunDir.y, 0.12f);
	float optical = dens * (toTop / sunY) * CLOUD_ABSORPTION * 0.08f;
	return max(exp(-optical), 0.45f);
}

/* Volumetric clouds — same look as cloud_ps.hlsl (half-res friendly). */
float4 MarchClouds(float3 ro, float3 rd, float2 pixelXY)
{
	if (rd.y < -0.05f && ro.y < CLOUD_BOTTOM)
		return float4(0, 0, 0, 0);

	float tEnter, tExit;
	if (!IntersectCloudSlab(ro, rd, CLOUD_BOTTOM, CLOUD_TOP, tEnter, tExit))
		return float4(0, 0, 0, 0);

	float marchLen = tExit - tEnter;
	float stepLen = min(marchLen / (float)CLOUD_PRIMARY_STEPS, CLOUD_MAX_STEP);
	int steps = min(CLOUD_PRIMARY_STEPS, (int)ceil(marchLen / max(stepLen, 1e-3f)));
	float t = tEnter + stepLen * IGN(pixelXY);

	float transmittance = 1.0f;
	float3 scattered = 0.0f.xxx;

	float3 L = normalize(SunDir);
	float cosTheta = dot(rd, L);
	float phase = lerp(1.0f, 0.75f + 0.55f * cosTheta, 0.3f);

	float3 albedo = float3(1.0f, 1.0f, 1.0f);
	float3 sunLight = float3(1.25f, 1.18f, 1.05f);
	float3 ambientCol = float3(0.92f, 0.95f, 1.0f) * CLOUD_AMBIENT;

	[loop]
	for (int i = 0; i < CLOUD_PRIMARY_STEPS; i++) {
		if (i >= steps || transmittance < 0.03f || t > tExit)
			break;

		float3 p = ro + rd * t;
		float dens = SampleDensity(p, true);
		if (dens > 1e-4f) {
			float sampleExt = dens * stepLen;
			float sampleTrans = exp(-sampleExt * CLOUD_ABSORPTION);
			float lightT = CheapLight(p, dens);

			float3 lightEnergy = albedo * (sunLight * lightT * phase + ambientCol);
			scattered += transmittance * lightEnergy * (1.0f - sampleTrans);
			transmittance *= sampleTrans;
		}
		t += stepLen;
	}

	float opacity = saturate(1.0f - transmittance);
	float horizon = saturate(1.0f - exp(-max(rd.y, 0.0f) * 6.0f));
	opacity *= lerp(0.35f, 1.0f, horizon);

	float3 cloudRgb = scattered / max(opacity, 1e-3f);
	cloudRgb = saturate(cloudRgb * 1.15f);
	cloudRgb = max(cloudRgb, float3(0.88f, 0.90f, 0.93f));
	cloudRgb = lerp(cloudRgb, CLOUD_SILVER, saturate((phase - 1.0f) * 0.2f + 0.05f));
	return float4(cloudRgb, opacity);
}

/* Cheap cloud reflection for water (matches water_ps SampleCloudReflection). */
float4 SampleCloudReflection(float3 ro, float3 rd)
{
	if (rd.y < 0.02f)
		return float4(0, 0, 0, 0);

	float tEnter, tExit;
	if (!IntersectCloudSlab(ro, rd, CLOUD_BOTTOM, CLOUD_TOP, tEnter, tExit))
		return float4(0, 0, 0, 0);

	float marchLen = tExit - tEnter;
	float stepLen = marchLen / (float)CLOUD_REFLECT_STEPS;
	float t = tEnter + stepLen * 0.35f;

	float transmittance = 1.0f;
	float3 scattered = 0.0f.xxx;

	float3 L = normalize(SunDir);
	float cosTheta = dot(rd, L);
	float phase = lerp(1.0f, 0.75f + 0.55f * cosTheta, 0.3f);
	float3 sunLight = float3(1.25f, 1.18f, 1.05f);
	float3 ambientCol = float3(0.92f, 0.95f, 1.0f) * CLOUD_AMBIENT;

	[unroll]
	for (int i = 0; i < CLOUD_REFLECT_STEPS; i++) {
		if (transmittance < 0.05f)
			break;

		float3 p = ro + rd * t;
		float dens = SampleDensity(p, false);
		if (dens > 1e-4f) {
			float sampleExt = dens * stepLen;
			float sampleTrans = exp(-sampleExt * CLOUD_ABSORPTION);
			float lightT = CheapLight(p, dens);
			float3 lightEnergy = sunLight * lightT * phase + ambientCol;
			scattered += transmittance * lightEnergy * (1.0f - sampleTrans);
			transmittance *= sampleTrans;
		}
		t += stepLen;
	}

	float opacity = saturate(1.0f - transmittance);
	float horizon = saturate(1.0f - exp(-max(rd.y, 0.0f) * 6.0f));
	opacity *= lerp(0.35f, 1.0f, horizon);

	float3 cloudRgb = scattered / max(opacity, 1e-3f);
	cloudRgb = saturate(cloudRgb * 1.15f);
	cloudRgb = max(cloudRgb, float3(0.88f, 0.90f, 0.93f));
	return float4(cloudRgb, opacity);
}

float3 EvalSkyGradient(float3 dir)
{
	float3 d = normalize(dir);
	float elev = saturate(d.y * 0.5f + 0.5f);
	float3 zenith = SkyColor * 1.05f;
	float3 horizon = float3(0.72f, 0.82f, 0.92f);
	return lerp(horizon, zenith, elev * elev);
}

/* Procedural sun disc + glow (same spirit as cloud_sun_ps.hlsl). */
float3 EvalSun(float3 dir)
{
	float3 L = normalize(SunDir);
	if (L.y < -0.2f)
		return 0.0f.xxx;

	float3 d = normalize(dir);
	float mu = saturate(dot(d, L));
	float ang = acos(saturate(mu));
	float r = ang / 0.065f; /* ~ normalized disc radius like screen sun */

	float elev = saturate(L.y);
	float warmth = 1.0f - elev * 0.35f;
	float3 tint = float3(1.0f, 1.0f - warmth * 0.18f, 1.0f - warmth * 0.42f);
	float intensity = (0.72f + elev * 0.35f) * SunStrength;

	if (r > 0.92f) {
		/* Wide atmospheric haze only. */
		float haze = exp(-ang * ang * 180.0f);
		return tint * float3(1.0f, 0.62f, 0.28f) * haze * 0.22f * intensity;
	}

	float discEdge = 0.095f;
	float disc = saturate(1.0f - smoothstep(discEdge * 0.70f, discEdge, r));
	float limb = saturate(1.0f - r / discEdge);
	limb = limb * limb * (3.0f - 2.0f * limb);
	float core = disc * (0.55f + 0.45f * limb);

	float r2 = r * r;
	float tight = exp(-r2 * 48.0f);
	float mid = exp(-r2 * 14.0f);
	float wide = exp(-r2 * 5.5f);
	float haze = exp(-r2 * 2.8f);
	float mask = 1.0f - smoothstep(0.55f, 0.90f, r);
	mask *= mask;

	float3 coreCol = float3(1.00f, 0.98f, 0.92f);
	float3 bloomCol = float3(1.00f, 0.82f, 0.48f);
	float3 hazeCol = float3(1.00f, 0.62f, 0.28f);

	float3 rgb =
		coreCol * core * 3.4f +
		coreCol * tight * 1.6f +
		bloomCol * mid * 0.90f +
		bloomCol * wide * 0.38f +
		hazeCol * haze * 0.16f;

	return rgb * tint * intensity * mask;
}

float3 EvalSkyWithClouds(float3 dir, float2 pixelXY)
{
	float3 col = EvalSkyGradient(dir);
	col += EvalSun(dir);
	float4 clouds = MarchClouds(CamPos, dir, pixelXY);
	col = lerp(col, clouds.rgb, clouds.a);
	return saturate(col);
}

/* Match water_vs WaveHeight. */
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
	h += sin(dot(xz, d1) * 0.12f + t * 0.58f) * 0.16f;
	h += sin(dot(xz, d2) * 0.21f + t * 0.95f) * 0.10f;
	h += sin(dot(xz, d3) * 0.33f + t * 0.80f) * 0.07f;
	return h;
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
	/* Match water_vs WaveHeight displacement for shading UVs / view. */
	P.y = seaY + WaveHeight(P.xz, Time);
	return true;
}

/* Match water_ps CalmRippleNormal. */
float3 CalmRippleNormal(float2 xz, float t)
{
	float3 n = float3(0.0f, 1.0f, 0.0f);

	float2 d0 = float2(0.96f, 0.28f);
	float2 d1 = float2(-0.42f, 0.91f);
	float2 d2 = float2(0.71f, -0.70f);
	float2 d3 = float2(-0.85f, -0.52f);
	float2 dSwell = float2(0.98f, 0.18f);

	n.xz += dSwell * cos(dot(xz, dSwell) * 0.048f + t * 0.55f) * 0.12f;
	n.xz += d0 * cos(dot(xz, d0) * 0.085f + t * 0.72f) * 0.11f;
	n.xz += d1 * cos(dot(xz, d1) * 0.12f + t * 0.58f) * 0.09f;
	n.xz += d2 * cos(dot(xz, d2) * 0.21f + t * 0.95f) * 0.07f;
	n.xz += d3 * cos(dot(xz, d3) * 0.33f + t * 0.80f) * 0.055f;

	n.xz += float2(0.6f, 0.8f) * cos(dot(xz, float2(0.6f, 0.8f)) * 0.55f + t * 1.35f) * 0.045f;
	n.xz += float2(-0.75f, 0.55f) * cos(dot(xz, float2(-0.75f, 0.55f)) * 0.72f + t * 1.15f) * 0.035f;
	n.xz += float2(0.35f, -0.9f) * cos(dot(xz, float2(0.35f, -0.9f)) * 1.05f + t * 1.55f) * 0.028f;

	return normalize(n);
}

float3 ShadeWater(float3 P, float3 V, float3 L)
{
	float3 N = CalmRippleNormal(P.xz, Time);
	float3 viewDir = V;

	/* Coast/ocean tiles: 32 world units → 1 UV (Water.cpp). */
	float2 baseTex = P.xz * (1.0f / 32.0f) + WaterUvScroll + float2(Time * 0.028f, Time * 0.019f);

	float2 flowA = float2(Time * 0.045f, Time * 0.028f);
	float2 flowB = float2(-Time * 0.032f, Time * 0.041f);
	float2 distort;
	distort.x = sin(P.x * 0.11f + P.z * 0.07f + Time * 1.4f) * 0.055f;
	distort.y = cos(P.z * 0.10f - P.x * 0.06f + Time * 1.15f) * 0.055f;
	distort += N.xz * 0.12f;

	float2 uvA = baseTex + flowA + distort;
	float2 uvB = baseTex * 1.73f - WaterUvScroll * 0.55f + flowB - distort * 0.65f;

	float4 texA = float4(0.12f, 0.28f, 0.32f, 1.0f);
	float4 texB = texA;
	if (WaterTexIndex != 0xFFFFFFFFu) {
		texA = BindlessTex[NonUniformResourceIndex(WaterTexIndex)].SampleLevel(LinSamp, uvA, 0);
		texB = BindlessTex[NonUniformResourceIndex(WaterTexIndex)].SampleLevel(LinSamp, uvB, 0);
	}
	float3 texColor = lerp(texA.rgb, texB.rgb, 0.42f);

	float3 deepColor = float3(0.056f, 0.1568f, 0.2016f);
	float3 shallowColor = texColor * WaterTint.rgb;
	float3 skyColor = FogColor.rgb * 1.05f;

	float NdotV = saturate(dot(N, viewDir));
	float F0 = 0.035f;
	float fresnel = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);

	float3 R = reflect(-viewDir, N);
	R = normalize(R + float3(0.0f, 0.08f, 0.0f));
	float4 clouds = SampleCloudReflection(P, R);
	float3 reflectColor = lerp(skyColor, clouds.rgb, clouds.a);

	float3 waterBody = lerp(deepColor, shallowColor, 0.55f + 0.25f * NdotV);
	float3 color = lerp(waterBody, reflectColor, fresnel * 0.78f);

	float3 H = normalize(L + viewDir);
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));
	float spec = pow(NdotH, 420.0f) * NdotL;
	spec += pow(NdotH, 48.0f) * NdotL * 0.084f;
	color += float3(0.92f, 0.96f, 1.0f) * spec * 0.5775f;

	float fogDist = length(P - CamPos);
	float fogFactor = saturate((FogEnd - fogDist) / max(FogEnd - FogStart, 1e-3f));
	color = lerp(FogColor.rgb, color, fogFactor);

	return saturate(color);
}

float3 ShadeMesh(float3 P, float3 N, float3 L, float4 albedo)
{
	float ndotl = saturate(dot(N, L));
	float bias = max(ShadowBias, 0.06f);
	float shadow = TraceShadow(P + N * bias, L, MaxRayT);
	float3 lit = albedo.rgb * (Ambient + SunStrength * ndotl * shadow);
	lit += albedo.rgb * SkyColor * 0.08f * saturate(N.y);
	return saturate(lit);
}

/* Skip alpha-cutout foliage (palm leaves, etc.): transparent texels must not
   block the ray — otherwise holes show the sea plane instead of what's behind. */
bool TracePrimaryCutout(
	float3 origin,
	float3 dir,
	out float hitT,
	out float3 hitP,
	out float3 hitN,
	out float4 hitAlb)
{
	hitT = MaxRayT + 1.0f;
	hitP = 0.0f;
	hitN = float3(0, 1, 0);
	hitAlb = 0.0f;

	float tMin = 0.05f;
	[loop]
	for (uint i = 0; i < 12u; ++i) {
		RayDesc primary;
		primary.Origin = origin;
		primary.Direction = dir;
		primary.TMin = tMin;
		primary.TMax = MaxRayT;

		RayQuery<RAY_FLAG_FORCE_OPAQUE |
			RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
		q.TraceRayInline(SceneBVH, RAY_FLAG_NONE, 0xFF, primary);
		q.Proceed();

		if (q.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
			return false;

		uint instId = q.CommittedInstanceID();
		uint primId = q.CommittedPrimitiveIndex();
		RtInst inst = Insts[instId];
		if (primId >= inst.triCount)
			return false;

		RtShadeTri tri = Tris[inst.triStart + primId];
		float2 bary = q.CommittedTriangleBarycentrics();
		float w = 1.0f - bary.x - bary.y;
		float3 localP = w * tri.p0 + bary.x * tri.p1 + bary.y * tri.p2;
		float2 texUv = w * tri.uv0 + bary.x * tri.uv1 + bary.y * tri.uv2;
		float3 localN = normalize(cross(tri.p1 - tri.p0, tri.p2 - tri.p0));
		float3 P = XformPoint(inst, localP);
		float3 N = XformNormal(inst, localN);
		if (dot(N, -dir) < 0.0f)
			N = -N;

		float4 alb = float4(0.65f, 0.65f, 0.65f, 1.0f);
		if (tri.texIndex != 0xFFFFFFFFu)
			alb = BindlessTex[NonUniformResourceIndex(tri.texIndex)].SampleLevel(LinSamp, texUv, 0);

		float t = q.CommittedRayT();
		/* Match raster clip(a - 0.01): punch through cutout holes. */
		if (alb.a >= 0.01f) {
			hitT = t;
			hitP = P;
			hitN = N;
			hitAlb = alb;
			return true;
		}

		tMin = t + max(0.002f, t * 1e-4f);
		if (tMin >= MaxRayT)
			return false;
	}
	return false;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float2 uv = input.UV;
	float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
	float4 nearH = mul(float4(ndc, 0.0f, 1.0f), InvViewProj);
	float4 farH = mul(float4(ndc, 1.0f, 1.0f), InvViewProj);
	float3 farW = farH.xyz / max(farH.w, 1e-6f);
	float3 dir = normalize(farW - CamPos);

	float meshT = MaxRayT + 1.0f;
	float3 meshP = 0.0f;
	float3 meshN = float3(0, 1, 0);
	float4 meshAlb = 0.0f;
	bool hitMesh = TracePrimaryCutout(CamPos, dir, meshT, meshP, meshN, meshAlb);

	float waterT = MaxRayT + 1.0f;
	float3 waterP = 0.0f;
	bool hitWater = IntersectSea(CamPos, dir, SeaLevelY, waterT, waterP);

	float3 L = normalize(SunDir);
	float3 V = normalize(-dir);

	if (hitWater && (!hitMesh || waterT < meshT))
		return float4(ShadeWater(waterP, V, L), 1.0f);

	if (hitMesh)
		return float4(ShadeMesh(meshP, meshN, L, meshAlb), 1.0f);

	/* Sky / sun / clouds — same layering as Clouds::Render (sky+sun then clouds). */
	return float4(EvalSkyWithClouds(dir, input.Pos.xy), 1.0f);
}
