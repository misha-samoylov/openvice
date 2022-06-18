cbuffer cbPerObject
{
	float4x4 WVP;
};

float4 VS( float4 Pos : POSITION ) : SV_POSITION
{
    // Оставляем координаты точки без изменений
    //return Pos;

	float4 output = mul(Pos, WVP);

	return output;
}