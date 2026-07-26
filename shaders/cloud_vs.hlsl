struct VS_INPUT
{
	float2 Pos : POSITION;
	float2 Tex : TEXCOORD0;
	float4 Color : COLOR;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float4 Color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	/* Screen-space sprites: already in NDC, far depth so world draws over them. */
	output.Pos = float4(input.Pos, 0.999f, 1.0f);
	output.Tex = input.Tex;
	output.Color = input.Color;
	return output;
}
