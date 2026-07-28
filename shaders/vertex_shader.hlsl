cbuffer cbPerObject
{
	float4x4 WVP;
	float4x4 World;
	float4x4 LightVP;
	float4 fogColor;
	float fogStart;
	float fogEnd;
	float receiveShadows;
	float shadowBias;
	float windTime;
	float windAmount;
	float2 padWind;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float FogDist : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
};

VS_OUTPUT main(float4 inPos : POSITION, float2 inTexCoord : TEXCOORD)
{
	VS_OUTPUT output;

	float3 pos = inPos.xyz;
	if (windAmount > 0.0f) {
		/* World origin → unique phase per instance so palms aren't locked. */
		float3 origin = World[3].xyz;
		float phase = origin.x * 0.07f + origin.z * 0.09f + windTime * 1.35f;
		float height = max(pos.y, 0.0f);
		float bend = saturate(height * 0.085f);
		bend *= bend;
		float sway = sin(phase + height * 0.22f) * windAmount * bend;
		float sway2 = cos(phase * 0.81f + height * 0.18f + pos.x * 0.4f) * windAmount * bend * 0.65f;
		float flutter = sin(phase * 2.7f + pos.x * 1.8f + pos.z * 1.5f) * windAmount * bend * 0.22f;
		pos.x += sway + flutter;
		pos.z += sway2 + flutter * 0.7f;
		pos.y -= (abs(sway) + abs(sway2)) * 0.08f;
	}

	float4 localPos = float4(pos, 1.0f);
	float4 worldPos = mul(localPos, World);
	output.WorldPos = worldPos.xyz;
	output.Pos = mul(localPos, WVP);
	output.TexCoord = inTexCoord;
	/* For LH perspective WVP, clip.w == view-space Z. */
	output.FogDist = output.Pos.w;

	return output;
}
