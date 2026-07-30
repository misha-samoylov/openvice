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
	float4 lit = RtLit.SampleLevel(LinearSamp, input.UV, 0);
	/* a=0 => sky / no RT — keep raster color. */
	float3 outRgb = lerp(base.rgb, lit.rgb, lit.a);
	return float4(outRgb, base.a);
}
