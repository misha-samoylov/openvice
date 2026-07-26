Texture2D CloudTexture : register(t0);
SamplerState CloudSampler : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float4 Color : COLOR;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 tex = CloudTexture.Sample(CloudSampler, input.Tex);
	return tex * input.Color;
}
