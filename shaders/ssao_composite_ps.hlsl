Texture2D AOTex : register(t0);
SamplerState LinearSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float ao = AOTex.Sample(LinearSamp, input.UV).r;
	/* Soft floor so corners never crush to black. */
	ao = lerp(0.55f, 1.0f, ao);
	return float4(ao, ao, ao, 1.0f);
}
