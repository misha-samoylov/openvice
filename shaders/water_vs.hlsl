cbuffer cbWater : register(b0)
{
	float4x4 WVP;
	float2 uvScroll;
	float2 pad;
	float4 tint;
};

struct VS_INPUT
{
	float3 Pos : POSITION;
	float2 Tex : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Pos = mul(float4(input.Pos, 1.0f), WVP);
	output.Tex = input.Tex + uvScroll;
	return output;
}
