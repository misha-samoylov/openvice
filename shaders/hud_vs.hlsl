struct VS_INPUT
{
	float2 Pos : POSITION;
	float2 Tex : TEXCOORD0;
	float2 Clip : TEXCOORD1; /* radar-local 0..1, or x < 0 to disable */
	float4 Color : COLOR;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float2 Clip : TEXCOORD1;
	float4 Color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Pos = float4(input.Pos, 0.0f, 1.0f);
	output.Tex = input.Tex;
	output.Clip = input.Clip;
	output.Color = input.Color;
	return output;
}
