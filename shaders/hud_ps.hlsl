Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

cbuffer HudPSCB : register(b0)
{
	float4 ColorMul;
	float4 Pad0;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float2 Clip : TEXCOORD1;
	float4 Color : COLOR;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	/*
	 * Clip.x <= -90 → no mask (HUD).
	 * Otherwise Clip is radar-local UV (may be <0 or >1 for tiles outside the disc).
	 */
	if (input.Clip.x > -90.0f) {
		float2 d = input.Clip * 2.0f - 1.0f;
		if (dot(d, d) > 1.0f)
			discard;
	}

	float4 t = tex0.Sample(samp0, input.Tex);
	float4 c = t * input.Color * ColorMul;
	if (c.a < 0.004f)
		discard;
	return c;
}
