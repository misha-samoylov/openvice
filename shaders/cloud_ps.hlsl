/* Raymarched volumetric clouds — tuned for half-res + cheap lighting.
 * Density: 3-octave FBM. Lighting: analytic height Beer (no light march). */
cbuffer CloudsCB : register(b0)
{
	float4x4 InvViewProj;
	float3 CamPos;
	float Time;
	float3 SunDir;
	float Coverage;
	float3 SkyColor;
	float DensityMult;
	float3 CloudSilver;
	float Absorption;
	float CloudBottom;
	float CloudTop;
	float WindSpeed;
	float Ambient;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

static const int PRIMARY_STEPS = 24;
static const float MAX_STEP = 18.0f;

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

float SampleDensity(float3 p)
{
	float h = saturate((p.y - CloudBottom) / max(1.0f, CloudTop - CloudBottom));
	float heightGrad = saturate(h * 4.0f) * saturate((1.0f - h) * 3.2f);

	float3 wind = float3(Time * WindSpeed, 0.0f, Time * WindSpeed * 0.35f);
	float3 q = (p + wind) * 0.0011f;
	q.y *= 1.65f;

	float base = Fbm3(q);
	float cover = saturate(Coverage);
	float shape = saturate(Remap(base, 1.0f - cover, 1.0f, 0.0f, 1.0f));

	/* Single cheap detail sample instead of a second FBM. */
	float detail = ValueNoise(q * 3.4f + 19.7f);
	float dens = saturate(shape - detail * 0.22f * (1.0f - shape));
	return dens * heightGrad * DensityMult;
}

/* Analytic sun transmittance — avoids nested light raymarch. */
float CheapLight(float3 p, float dens)
{
	float toTop = max(0.0f, CloudTop - p.y);
	float sunY = max(SunDir.y, 0.12f);
	float optical = dens * (toTop / sunY) * Absorption * 0.08f;
	return max(exp(-optical), 0.45f);
}

float IGN(float2 p)
{
	return frac(52.9829189f * frac(dot(p, float2(0.06711056f, 0.00583715f))));
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float2 uv = input.UV;
	float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);

	float4 worldFar = mul(float4(ndc, 1.0f, 1.0f), InvViewProj);
	worldFar.xyz /= worldFar.w;

	float3 rd = normalize(worldFar.xyz - CamPos);
	float3 ro = CamPos;

	if (rd.y < -0.05f && ro.y < CloudBottom)
		return float4(0, 0, 0, 0);

	float tEnter, tExit;
	if (!IntersectCloudSlab(ro, rd, CloudBottom, CloudTop, tEnter, tExit))
		return float4(0, 0, 0, 0);

	float marchLen = tExit - tEnter;
	float stepLen = min(marchLen / (float)PRIMARY_STEPS, MAX_STEP);
	int steps = min(PRIMARY_STEPS, (int)ceil(marchLen / max(stepLen, 1e-3f)));
	float t = tEnter + stepLen * IGN(input.Pos.xy);

	float transmittance = 1.0f;
	float3 scattered = 0.0f.xxx;

	float cosTheta = dot(rd, SunDir);
	float phase = lerp(1.0f, 0.75f + 0.55f * cosTheta, 0.3f);

	float3 albedo = float3(1.0f, 1.0f, 1.0f);
	float3 sunLight = float3(1.25f, 1.18f, 1.05f);
	float3 ambientCol = float3(0.92f, 0.95f, 1.0f) * Ambient;

	[loop]
	for (int i = 0; i < PRIMARY_STEPS; i++) {
		if (i >= steps || transmittance < 0.03f || t > tExit)
			break;

		float3 p = ro + rd * t;
		float dens = SampleDensity(p);
		if (dens > 1e-4f) {
			float sampleExt = dens * stepLen;
			float sampleTrans = exp(-sampleExt * Absorption);
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
	cloudRgb = lerp(cloudRgb, CloudSilver, saturate((phase - 1.0f) * 0.2f + 0.05f));

	return float4(cloudRgb, opacity);
}
