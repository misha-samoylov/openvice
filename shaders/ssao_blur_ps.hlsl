cbuffer cbSSAO : register(b0)
{
	float4x4 Proj;
	float2 InvFullSize;
	float2 InvHalfSize;
	float Radius;
	float Bias;
	float Intensity;
	float Power;
	float Proj33;
	float Proj43;
	float Proj11;
	float Proj22;
};

Texture2D AOTex : register(t0);
Texture2D DepthTex : register(t1);
SamplerState PointSamp : register(s0);

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float2 uv = input.UV;
	float centerDepth = DepthTex.SampleLevel(PointSamp, uv, 0).r;
	float centerAO = AOTex.SampleLevel(PointSamp, uv, 0).r;

	if (centerDepth >= 0.9999f)
		return 1.0f.xxxx;

	float sum = centerAO;
	float wsum = 1.0f;

	[unroll]
	for (int y = -1; y <= 1; y++) {
		[unroll]
		for (int x = -1; x <= 1; x++) {
			if (x == 0 && y == 0)
				continue;
			float2 offset = float2(x, y) * InvHalfSize;
			float2 suv = uv + offset;
			float d = DepthTex.SampleLevel(PointSamp, suv, 0).r;
			float a = AOTex.SampleLevel(PointSamp, suv, 0).r;
			float w = exp(-abs(d - centerDepth) * 80.0f);
			sum += a * w;
			wsum += w;
		}
	}

	return (sum / wsum).xxxx;
}
