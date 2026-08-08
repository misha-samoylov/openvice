#pragma once

#include <d3d11.h>
#include <vector>

#include "DXRender.hpp"

/* Lightweight CSprite2d analogue — screen-space textured quads for HUD/radar. */
class Sprite2d
{
public:
	struct Vertex {
		float x, y;       /* NDC */
		float u, v;       /* texture UV */
		float cu, cv;     /* clip UV (>=0 enables circle clip), or cu < 0 */
		float r, g, b, a;
	};

	Sprite2d();
	~Sprite2d();

	bool Init(DXRender* render);
	void Shutdown();

	void Begin(DXRender* render);
	void DrawRect(
		float left, float top, float right, float bottom,
		float u0, float v0, float u1, float v1,
		float r, float g, float b, float a,
		ID3D11ShaderResourceView* srv,
		bool circleClip = false
	);
	void DrawQuad(
		const float px[4], const float py[4],
		const float u[4], const float v[4],
		const float cu[4], const float cv[4],
		float r, float g, float b, float a,
		ID3D11ShaderResourceView* srv
	);
	void Flush(DXRender* render);

	ID3D11ShaderResourceView* White() const { return m_whiteSRV; }
	void PixelToNdc(float sx, float sy, float* nx, float* ny) const;

private:
	static constexpr int kMaxVerts = 4096;

	void FlushCurrent();

	UINT m_screenW;
	UINT m_screenH;

	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_ps;
	ID3D11InputLayout* m_layout;
	ID3D11Buffer* m_vb;
	ID3D11Buffer* m_psCB;
	ID3D11SamplerState* m_sampler;
	ID3D11BlendState* m_blend;
	ID3D11DepthStencilState* m_depthOff;
	ID3D11RasterizerState* m_rs;
	ID3D11ShaderResourceView* m_whiteSRV;
	ID3D11Texture2D* m_whiteTex;

	DXRender* m_render;
	std::vector<Vertex> m_verts;
	ID3D11ShaderResourceView* m_currentSrv;
	bool m_ready;
};
