/* re3 POSTFX_NORMAL — colourfilterVC.frag port. */
Texture2D SceneTex : register(t0);
SamplerState PointSamp : register(s0);

cbuffer PostFXCB : register(b0)
{
	float4 BlurColor; /* rgb * intensity, a = 30/255 */
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float a = BlurColor.a;
	float4 doublec = saturate(BlurColor * 2.0f);
	float4 dst = SceneTex.Sample(PointSamp, input.UV);
	float4 prev = dst;

	[unroll]
	for (int i = 0; i < 5; i++) {
		float4 tmp = dst * (1.0f - a) + prev * doublec * a;
		tmp += prev * BlurColor;
		tmp += prev * BlurColor;
		prev = saturate(tmp);
	}

	return float4(prev.rgb, 1.0f);
}
