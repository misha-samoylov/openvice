Texture2D ObjTexture;
SamplerState ObjSamplerState;

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 color = ObjTexture.Sample(ObjSamplerState, input.TexCoord);
	/* Kill fully transparent texels so they never write depth or blend. */
	clip(color.a - 0.01f);
	return color;
}
