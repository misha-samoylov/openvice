/* Depth-only / alpha-clip for the directional shadow map pass. */
Texture2D ObjTexture : register(t0);
SamplerState ObjSamplerState : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float FogDist : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
};

void main(VS_OUTPUT input)
{
	float alpha = ObjTexture.Sample(ObjSamplerState, input.TexCoord).a;
	clip(alpha - 0.01f);
}
