/* Procedural sun disc + atmospheric glow (additive). No texture. */

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float4 Color : COLOR;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	/* UV → radial distance (0 at core, 1.0 at mid-edge of the quad). */
	float2 p = input.Tex * 2.0f - 1.0f;
	float r2 = dot(p, p);
	float r = sqrt(r2);

	/* Hard circular kill — must be ~0 well before the square edge (r=1). */
	if (r > 0.92f)
		return float4(0.0f, 0.0f, 0.0f, 0.0f);

	float mask = 1.0f - smoothstep(0.55f, 0.90f, r);
	mask *= mask;

	/* Hot photosphere with soft limb darkening. */
	float discEdge = 0.095f;
	float disc = saturate(1.0f - smoothstep(discEdge * 0.70f, discEdge, r));
	float limb = saturate(1.0f - r / discEdge);
	limb = limb * limb * (3.0f - 2.0f * limb);
	float core = disc * (0.55f + 0.45f * limb);

	/* Glow falls off fast enough that intensity ≈ 0 before mask ends. */
	float tight = exp(-r2 * 48.0f);
	float mid   = exp(-r2 * 14.0f);
	float wide  = exp(-r2 * 5.5f);
	float haze  = exp(-r2 * 2.8f);

	float3 coreCol = float3(1.00f, 0.98f, 0.92f);
	float3 bloomCol = float3(1.00f, 0.82f, 0.48f);
	float3 hazeCol  = float3(1.00f, 0.62f, 0.28f);

	float3 rgb =
		coreCol  * core  * 3.4f +
		coreCol  * tight * 1.6f +
		bloomCol * mid   * 0.90f +
		bloomCol * wide  * 0.38f +
		hazeCol  * haze  * 0.16f;

	rgb *= input.Color.rgb * mask;
	return float4(rgb, 1.0f);
}
