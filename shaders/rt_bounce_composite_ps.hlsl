/* Apply RT sun shadow + spatially filtered RTAO over raster color. */
Texture2D SceneColor : register(t0);
Texture2D RtLit : register(t1);
SamplerState LinearSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

/* Blur channel from RtLit; skip sky (a≈0). ch: 0=AO(.r), 3=shadow(.a). */
float BlurRtChannel(float2 uv, int ch, int radius, float falloff)
{
	float w, h;
	RtLit.GetDimensions(w, h);
	float2 texel = float2(1.0f / max(w, 1.0f), 1.0f / max(h, 1.0f));

	float sum = 0.0f;
	float wsum = 0.0f;

	[loop]
	for (int y = -radius; y <= radius; ++y) {
		[loop]
		for (int x = -radius; x <= radius; ++x) {
			float2 o = float2((float)x, (float)y);
			float k = exp(-falloff * dot(o, o));
			float4 s = RtLit.SampleLevel(LinearSamp, uv + o * texel, 0);
			if (s.a > 0.001f) {
				float v = (ch == 0) ? s.r : s.a;
				sum += v * k;
				wsum += k;
			}
		}
	}
	return (wsum > 1e-4f) ? (sum / wsum) : 1.0f;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 base = SceneColor.SampleLevel(LinearSamp, input.UV, 0);
	float4 rt = RtLit.SampleLevel(LinearSamp, input.UV, 0);

	/* a==0 => sky / no surface — leave master raster color. */
	if (rt.a <= 0.001f)
		return base;

	/* Soften noisy soft-shadow / AO; shadow kernel slightly tighter than AO. */
	float shadow = BlurRtChannel(input.UV, 3, 2, 0.45f);
	float ao = BlurRtChannel(input.UV, 0, 2, 0.35f);
	return float4(base.rgb * shadow * ao, base.a);
}
