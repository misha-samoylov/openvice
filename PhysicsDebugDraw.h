#pragma once

#include <vector>
#include <DirectXMath.h>

#include "DXRender.hpp"

#define __BT_DISABLE_SSE__
#include "LinearMath/btIDebugDraw.h"

using namespace DirectX;

/* Collects Bullet debug primitives and draws them as colored DX11 line lists. */
class PhysicsDebugDraw : public btIDebugDraw
{
public:
	PhysicsDebugDraw();
	~PhysicsDebugDraw();

	bool Init(DXRender* render);
	void Cleanup();

	void BeginFrame();
	void SetViewProjection(const XMMATRIX& viewProj);
	/* Skip lines whose midpoint is farther than radius from the camera. */
	void SetCullSphere(float x, float y, float z, float radius);
	void Render(DXRender* render);

	void SetEnabled(bool enabled);
	bool IsEnabled() const { return m_enabled; }

	virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
	virtual void drawContactPoint(
		const btVector3& PointOnB, const btVector3& normalOnB,
		btScalar distance, int lifeTime, const btVector3& color) override;
	virtual void reportErrorWarning(const char* warningString) override;
	virtual void draw3dText(const btVector3& location, const char* textString) override;
	virtual void setDebugMode(int debugMode) override;
	virtual int getDebugMode() const override;

private:
	struct Vertex {
		float x, y, z;
		float r, g, b, a;
	};

	bool EnsureVertexBuffer(DXRender* render, UINT vertexCount);

	bool m_enabled;
	int m_debugMode;
	std::vector<Vertex> m_lines;

	float m_cullX, m_cullY, m_cullZ, m_cullRadiusSq;
	bool m_cullEnabled;

	XMMATRIX m_viewProj;

	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_ps;
	ID3D11InputLayout* m_layout;
	ID3D11Buffer* m_vb;
	ID3D11Buffer* m_cb;
	ID3D11RasterizerState* m_rs;
	ID3D11DepthStencilState* m_dss;
	ID3D11BlendState* m_bs;
	UINT m_vbCapacity;
};
