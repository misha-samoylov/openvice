cbuffer cbPerFrame : register(b0)
{
	float4x4 ViewProj;
};

struct VS_INPUT
{
	float3 Pos : POSITION;
	float4 Color : COLOR;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float4 Color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT o;
	o.Pos = mul(float4(input.Pos, 1.0), ViewProj);
	o.Color = input.Color;
	return o;
}
