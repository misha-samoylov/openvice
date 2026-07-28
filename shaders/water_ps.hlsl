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
	float cloudReflect;
	float windSpeed;
	float cloudCoverage;
	float cloudDensity;
	float _pad1;
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

/* Must match Clouds.h slab / look (cheap reflection march). */
static const float CLOUD_BOTTOM = 140.0f;
static const float CLOUD_TOP = 320.0f;
static const float CLOUD_ABSORPTION = 0.55f;
static const int CLOUD_REFLECT_STEPS = 10;

/* Animated surface normals — clearly moving, still not storm chop. */
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
	n.xz += d1 * cos(dot(xz, d1) * 0.12f  + t * 0.58f) * 0.09f;
	n.xz += d2 * cos(dot(xz, d2) * 0.21f  + t * 0.95f) * 0.07f;
	n.xz += d3 * cos(dot(xz, d3) * 0.33f  + t * 0.80f) * 0.055f;

	n.xz += float2(0.6f, 0.8f) * cos(dot(xz, float2(0.6f, 0.8f)) * 0.55f + t * 1.35f) * 0.045f;
	n.xz += float2(-0.75f, 0.55f) * cos(dot(xz, float2(-0.75f, 0.55f)) * 0.72f + t * 1.15f) * 0.035f;
	n.xz += float2(0.35f, -0.9f) * cos(dot(xz, float2(0.35f, -0.9f)) * 1.05f + t * 1.55f) * 0.028f;

	return normalize(n);
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

float SampleCloudDensity(float3 p)
{
	float h = saturate((p.y - CLOUD_BOTTOM) / max(1.0f, CLOUD_TOP - CLOUD_BOTTOM));
	float heightGrad = saturate(h * 4.0f) * saturate((1.0f - h) * 3.2f);

	float3 wind = float3(time * windSpeed, 0.0f, time * windSpeed * 0.35f);
	float3 q = (p + wind) * 0.0011f;
	q.y *= 1.65f;

	float base = Fbm2(q);
	float cover = saturate(cloudCoverage);
	float shape = saturate(Remap(base, 1.0f - cover, 1.0f, 0.0f, 1.0f));
	float detail = ValueNoise(q * 3.4f + 19.7f);
	float dens = saturate(shape - detail * 0.22f * (1.0f - shape));
	return dens * heightGrad * cloudDensity;
}

/* Cheap cloud look-up along a sky reflection ray. */
float4 SampleCloudReflection(float3 ro, float3 rd)
{
	if (cloudReflect < 0.01f || rd.y < 0.02f)
		return float4(0, 0, 0, 0);

	float tEnter, tExit;
	if (!IntersectCloudSlab(ro, rd, CLOUD_BOTTOM, CLOUD_TOP, tEnter, tExit))
		return float4(0, 0, 0, 0);

	float marchLen = tExit - tEnter;
	float stepLen = marchLen / (float)CLOUD_REFLECT_STEPS;
	float t = tEnter + stepLen * 0.35f;

	float transmittance = 1.0f;
	float3 scattered = 0.0f.xxx;

	float3 L = normalize(sunDir);
	float cosTheta = dot(rd, L);
	float phase = lerp(1.0f, 0.75f + 0.55f * cosTheta, 0.3f);
	float3 sunLight = float3(1.25f, 1.18f, 1.05f);
	float3 ambientCol = float3(0.92f, 0.95f, 1.0f) * 1.15f;

	[unroll]
	for (int i = 0; i < CLOUD_REFLECT_STEPS; i++) {
		if (transmittance < 0.05f)
			break;

		float3 p = ro + rd * t;
		float dens = SampleCloudDensity(p);
		if (dens > 1e-4f) {
			float sampleExt = dens * stepLen;
			float sampleTrans = exp(-sampleExt * CLOUD_ABSORPTION);
			float toTop = max(0.0f, CLOUD_TOP - p.y);
			float sunY = max(L.y, 0.12f);
			float lightT = max(exp(-dens * (toTop / sunY) * CLOUD_ABSORPTION * 0.08f), 0.45f);

			float3 lightEnergy = sunLight * lightT * phase + ambientCol;
			scattered += transmittance * lightEnergy * (1.0f - sampleTrans);
			transmittance *= sampleTrans;
		}
		t += stepLen;
	}

	float opacity = saturate(1.0f - transmittance);
	float horizon = saturate(1.0f - exp(-max(rd.y, 0.0f) * 6.0f));
	opacity *= lerp(0.35f, 1.0f, horizon) * cloudReflect;

	float3 cloudRgb = scattered / max(opacity, 1e-3f);
	cloudRgb = saturate(cloudRgb * 1.15f);
	cloudRgb = max(cloudRgb, float3(0.88f, 0.90f, 0.93f));
	return float4(cloudRgb, opacity);
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float3 worldPos = input.WorldPos;
	float3 viewDir = normalize(cameraPos - worldPos);
	float3 N = CalmRippleNormal(worldPos.xz, time);

	/* Dual-layer flow + ripple warp — surface clearly crawls */
	float2 flowA = float2(time * 0.045f, time * 0.028f);
	float2 flowB = float2(-time * 0.032f, time * 0.041f);
	float2 distort;
	distort.x = sin(worldPos.x * 0.11f + worldPos.z * 0.07f + time * 1.4f) * 0.055f;
	distort.y = cos(worldPos.z * 0.10f - worldPos.x * 0.06f + time * 1.15f) * 0.055f;
	distort += N.xz * 0.12f;

	float2 uvA = input.Tex + flowA + distort;
	float2 uvB = input.Tex * 1.73f - uvScroll * 0.55f + flowB - distort * 0.65f;

	float4 texA = WaterTexture.Sample(WaterSampler, uvA);
	float4 texB = WaterTexture.Sample(WaterSampler, uvB);
	float3 texColor = lerp(texA.rgb, texB.rgb, 0.42f);

	float3 deepColor = float3(0.056f, 0.1568f, 0.2016f);
	float3 shallowColor = texColor * tint.rgb;
	float3 skyColor = fogColor.rgb * 1.05f;

	float NdotV = saturate(dot(N, viewDir));
	float F0 = 0.035f;
	float fresnel = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);

	/* Reflect sky + procedural clouds (same slab as volumetric pass). */
	float3 R = reflect(-viewDir, N);
	/* Nudge upward so glancing normals still hit the cloud layer */
	R = normalize(R + float3(0.0f, 0.08f, 0.0f));
	float4 clouds = SampleCloudReflection(worldPos, R);
	float3 reflectColor = lerp(skyColor, clouds.rgb, clouds.a);

	float3 waterBody = lerp(deepColor, shallowColor, 0.55f + 0.25f * NdotV);
	float3 color = lerp(waterBody, reflectColor, fresnel * 0.78f);

	float3 L = normalize(sunDir);
	float3 H = normalize(L + viewDir);
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));
	float spec = pow(NdotH, 420.0f) * NdotL;
	spec += pow(NdotH, 48.0f) * NdotL * 0.084f;
	color += float3(0.92f, 0.96f, 1.0f) * spec * 0.5775f;

	float alpha = saturate(lerp(0.58f, 0.90f, fresnel) * tint.a * lerp(texA.a, texB.a, 0.35f));
	alpha = saturate(lerp(alpha, 1.0f, 0.30f));

	float fogFactor = saturate((fogEnd - input.FogDist) / (fogEnd - fogStart));
	color = lerp(fogColor.rgb, color, fogFactor);

	return float4(color, alpha);
}
