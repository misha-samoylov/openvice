cbuffer cbPerObject : register(b0)
{
	float4x4 WVP;
	float4 fogColor;
	float fogStart;
	float fogEnd;
	float2 fogPad;
};

Texture2D ObjTexture : register(t0);
SamplerState ObjSamplerState : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float FogDist : TEXCOORD1;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 color = ObjTexture.Sample(ObjSamplerState, input.TexCoord);
	/* Kill fully transparent texels so they never write depth or blend. */
	clip(color.a - 0.01f);

	float fogFactor = saturate((fogEnd - input.FogDist) / (fogEnd - fogStart));
	color.rgb = lerp(fogColor.rgb, color.rgb, fogFactor);
	return color;
}
