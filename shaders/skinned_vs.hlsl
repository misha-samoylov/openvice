cbuffer cbPerObject : register(b0)
{
	float4x4 WVP;
	float4 fogColor;
	float fogStart;
	float fogEnd;
	float2 fogPad;
};

cbuffer cbBones : register(b1)
{
	float4x4 Bones[64];
};

struct VS_INPUT
{
	float3 Pos : POSITION;
	float2 TexCoord : TEXCOORD;
	uint4 BlendIndices : BLENDINDICES;
	float4 BlendWeights : BLENDWEIGHT;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float FogDist : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 pos = float4(input.Pos, 1.0f);
	float4 skinned =
		mul(pos, Bones[input.BlendIndices.x]) * input.BlendWeights.x +
		mul(pos, Bones[input.BlendIndices.y]) * input.BlendWeights.y +
		mul(pos, Bones[input.BlendIndices.z]) * input.BlendWeights.z +
		mul(pos, Bones[input.BlendIndices.w]) * input.BlendWeights.w;

	output.Pos = mul(skinned, WVP);
	output.TexCoord = input.TexCoord;
	output.FogDist = output.Pos.w;
	return output;
}
