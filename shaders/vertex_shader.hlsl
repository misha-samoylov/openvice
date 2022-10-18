cbuffer cbPerObject
{
	float4x4 WVP;
};

float4 main(float4 Pos: POSITION, float2 Tex: TEXCOORD0): SV_POSITION
{
	float4 output = mul(Pos, WVP);
	return output;
}