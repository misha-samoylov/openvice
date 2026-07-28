cbuffer cbWater : register(b0)
{
	float4x4 WVP;
	float2 uvScroll;
	float fogStart;
	float fogEnd;
	float4 tint;
	float4 fogColor;
	float3 cameraPos;
	float time;
	float3 sunDir;
	float _pad0;
};

struct VS_INPUT
{
	float3 Pos : POSITION;
	float2 Tex : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float FogDist : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Pos = mul(float4(input.Pos, 1.0f), WVP);
	output.Tex = input.Tex + uvScroll;
	output.FogDist = output.Pos.w;
	output.WorldPos = input.Pos;
	return output;
}
