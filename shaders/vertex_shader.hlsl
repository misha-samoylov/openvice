cbuffer cbPerObject
{
	float4x4 WVP;
	float4 fogColor;
	float fogStart;
	float fogEnd;
	float2 fogPad;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float FogDist : TEXCOORD1;
};

VS_OUTPUT main(float4 inPos : POSITION, float2 inTexCoord : TEXCOORD)
{
	VS_OUTPUT output;

	output.Pos = mul(inPos, WVP);
	output.TexCoord = inTexCoord;
	/* For LH perspective WVP, clip.w == view-space Z. */
	output.FogDist = output.Pos.w;

	return output;
}
