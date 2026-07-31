/* Apply master-style RT sun shadow darken over raster color. */
Texture2D SceneColor : register(t0);
Texture2D RtLit : register(t1);
SamplerState LinearSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 base = SceneColor.SampleLevel(LinearSamp, input.UV, 0);
	float4 rt = RtLit.SampleLevel(LinearSamp, input.UV, 0);

	/* a==0 => sky / no surface — leave master raster color. */
	if (rt.a <= 0.001f)
		return base;

	/* a = lerp(0.625, 1.0, lit) from RT sun shadow pass. */
	return float4(base.rgb * rt.a, base.a);
}
