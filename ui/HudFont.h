#pragma once

#include "ui/Sprite2d.h"
#include "DXRender.hpp"

/*
 * re3 CFont FONT_HEADING path:
 *   SetFontStyle(FONT_HEADING) → FONT_STANDARD texture (font1) + bFontHalfTexture
 *   Digits/symbols remapped via FindNewCharacter into the Pricedown half of font1.
 */
class HudFont
{
public:
	HudFont();
	~HudFont();

	bool Init(DXRender* render, const char* fontsTxdPath);
	void Shutdown();

	void SetScale(float sx, float sy);
	void SetColor(float r, float g, float b, float a);
	void SetDropShadow(float dx, float dy, float r, float g, float b, float a);

	/* Right-justified string — re3 PrintString with RightJustifyOn + PropOff. */
	void PrintRight(Sprite2d& sprites, float x, float y, const char* text);
	float GetStringWidth(const char* text) const;

private:
	static int FindNewCharacter(int c); /* c = ascii - ' ' */
	bool CharUv(int slot, float* u0, float* v0, float* u1, float* v1) const;
	void DrawChar(Sprite2d& sprites, float x, float y, char c, float r, float g, float b, float a);

	ID3D11ShaderResourceView* m_fontSrv;
	float m_scaleX, m_scaleY;
	float m_r, m_g, m_b, m_a;
	float m_dropX, m_dropY;
	float m_dropR, m_dropG, m_dropB, m_dropA;

	/* re3 PropOff advance: Size[FONT_STANDARD][209] */
	static constexpr float kUnpropWidth = 20.0f;
	/* re3 PrintChar quad: 32 * scaleX by 40 * scaleY * 0.5 */
	static constexpr float kGlyphW = 32.0f;
	static constexpr float kGlyphH = 20.0f;
};
