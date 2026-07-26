cbuffer cbPerObject
{
	float4x4 WVP;
	float4x4 World;
	float4x4 LightVP;
	float4 fogColor;
	float fogStart;
	float fogEnd;
	float receiveShadows;
	float shadowBias;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float FogDist : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
};

VS_OUTPUT main(float4 inPos : POSITION, float2 inTexCoord : TEXCOORD)
{
	VS_OUTPUT output;

	float4 worldPos = mul(inPos, World);
	output.WorldPos = worldPos.xyz;
	output.Pos = mul(inPos, WVP);
	output.TexCoord = inTexCoord;
	/* For LH perspective WVP, clip.w == view-space Z. */
	output.FogDist = output.Pos.w;

	return output;
}
