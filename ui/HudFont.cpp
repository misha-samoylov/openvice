#include "ui/HudFont.h"
#include "graphics/TextureFactory.h"
#include "renderware.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

HudFont::HudFont()
	: m_fontSrv(nullptr)
	, m_scaleX(1.0f)
	, m_scaleY(1.0f)
	, m_r(1), m_g(1), m_b(1), m_a(1)
	, m_dropX(0), m_dropY(0)
	, m_dropR(0), m_dropG(0), m_dropB(0), m_dropA(0)
{
}

HudFont::~HudFont()
{
	Shutdown();
}

bool HudFont::Init(DXRender* render, const char* fontsTxdPath)
{
	FILE* f = fopen(fontsTxdPath, "rb");
	if (!f) {
		printf("[Error] HudFont: cannot open %s\n", fontsTxdPath);
		return false;
	}
	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buffer = (char*)malloc(fileSize);
	if (!buffer) {
		fclose(f);
		return false;
	}
	fread(buffer, 1, fileSize, f);
	fclose(f);

	size_t offset = 0;
	TextureDictionary txd;
	txd.read(buffer, &offset);

	/* FONT_HEADING uses Sprite[FONT_STANDARD] = font1 (Pricedown / bold HUD). */
	NativeTexture* font1 = nullptr;
	for (size_t i = 0; i < txd.texList.size(); i++) {
		if (_stricmp(txd.texList[i].name, "font1") == 0) {
			font1 = &txd.texList[i];
			break;
		}
	}
	if (!font1) {
		printf("[Error] HudFont: font1 not found\n");
		free(buffer);
		return false;
	}

	HRESULT hr = TextureFactory::CreateSrvFromNative(render, *font1, &m_fontSrv);
	free(buffer);
	if (FAILED(hr) || !m_fontSrv) {
		printf("[Error] HudFont: failed to create font1 SRV\n");
		return false;
	}

	printf("[Info] HudFont: loaded font1 (FONT_HEADING / Pricedown)\n");
	return true;
}

void HudFont::Shutdown()
{
	if (m_fontSrv) {
		m_fontSrv->Release();
		m_fontSrv = nullptr;
	}
}

void HudFont::SetScale(float sx, float sy)
{
	m_scaleX = sx;
	m_scaleY = sy;
}

void HudFont::SetColor(float r, float g, float b, float a)
{
	m_r = r; m_g = g; m_b = b; m_a = a;
}

void HudFont::SetDropShadow(float dx, float dy, float r, float g, float b, float a)
{
	m_dropX = dx; m_dropY = dy;
	m_dropR = r; m_dropG = g; m_dropB = b; m_dropA = a;
}

/* re3 CFont::FindNewCharacter — remaps into Pricedown slots on font1. */
int HudFont::FindNewCharacter(int c)
{
	if (c >= 16 && c <= 26) return c + 128;
	if (c >= 8 && c <= 9) return c + 86;
	if (c == 4) return c + 89;
	if (c == 7) return 206;
	if (c == 14) return 207;
	if (c >= 33 && c <= 58) return c + 122;
	if (c >= 65 && c <= 90) return c + 90;
	if (c >= 96 && c <= 118) return c + 85;
	if (c >= 119 && c <= 140) return c + 62;
	if (c >= 141 && c <= 142) return 204;
	if (c == 143) return 205;
	if (c == 1) return 208;
	return c;
}

bool HudFont::CharUv(int slot, float* u0, float* v0, float* u1, float* v1) const
{
	if (slot < 0 || slot > 208)
		return false;

	/* re3 PrintChar for FONT_BANK / FONT_STANDARD: 16 cols, 12.8 rows (40px cells). */
	float xoff = (float)(slot % 16);
	float yoff = (float)(slot / 16);
	*u0 = xoff / 16.0f;
	*v0 = yoff / 12.8f + 0.0021f;
	*u1 = (xoff + 1.0f) / 16.0f - 0.001f;
	*v1 = (yoff + 1.0f) / 12.8f - 0.0021f;
	return true;
}

void HudFont::DrawChar(Sprite2d& sprites, float x, float y, char c, float r, float g, float b, float a)
{
	int slot = (unsigned char)c - ' ';
	slot = FindNewCharacter(slot);

	float u0, v0, u1, v1;
	if (!CharUv(slot, &u0, &v0, &u1, &v1))
		return;

	float w = kGlyphW * m_scaleX;
	float h = kGlyphH * m_scaleY;
	sprites.DrawRect(x, y, x + w, y + h, u0, v0, u1, v1, r, g, b, a, m_fontSrv, false);
}

float HudFont::GetStringWidth(const char* text) const
{
	if (!text)
		return 0.0f;
	return (float)strlen(text) * kUnpropWidth * m_scaleX;
}

void HudFont::PrintRight(Sprite2d& sprites, float x, float y, const char* text)
{
	if (!text || !m_fontSrv)
		return;

	float width = GetStringWidth(text);
	float left = x - width;
	float advance = kUnpropWidth * m_scaleX;
	float cursor = left;

	if (m_dropA > 0.0f && (m_dropX != 0.0f || m_dropY != 0.0f)) {
		float dcur = cursor;
		for (const char* p = text; *p; ++p) {
			DrawChar(sprites, dcur + m_dropX, y + m_dropY, *p, m_dropR, m_dropG, m_dropB, m_dropA * m_a);
			dcur += advance;
		}
	}

	for (const char* p = text; *p; ++p) {
		DrawChar(sprites, cursor, y, *p, m_r, m_g, m_b, m_a);
		cursor += advance;
	}
}
