/* Upsample half-res god rays for additive blend onto the back buffer. */
Texture2D RaysTex : register(t0);
SamplerState LinearSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float3 rays = RaysTex.Sample(LinearSamp, input.UV).rgb;
	return float4(rays, 1.0f);
}
