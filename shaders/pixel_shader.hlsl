Texture2D ObjTexture;
SamplerState ObjSamplerState;

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float3 pixelColor = ObjTexture.Sample(ObjSamplerState, input.TexCoord);
	return float4(pixelColor, 1.0f);

	//return ObjTexture.Sample(ObjSamplerState, input.TexCoord);
}