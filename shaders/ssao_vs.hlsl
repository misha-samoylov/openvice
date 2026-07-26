/* Fullscreen triangle (no VB) — SV_VertexID 0..2. */
struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
};

VS_OUTPUT main(uint id : SV_VertexID)
{
	VS_OUTPUT o;
	o.UV = float2((id << 1) & 2, id & 2);
	o.Pos = float4(o.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return o;
}
