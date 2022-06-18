cbuffer cbPerObject
{
	float4x4 WVP;
};

float4 main(float4 Pos: POSITION): SV_POSITION
{
	float4 output = mul(Pos, WVP);
	return output;
}