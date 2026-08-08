#include "ui/Radar.h"
#include "graphics/TextureFactory.h"
#include "core/GameConfig.h"
#include "renderware.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>

Radar::Radar()
	: m_radardisc(nullptr)
	, m_centre(nullptr)
	, m_north(nullptr)
	, m_radarRange(MIN_RANGE)
	, m_originX(0)
	, m_originY(0)
	, m_cachedSin(0)
	, m_cachedCos(1)
{
	for (int i = 0; i < NUM_TILES * NUM_TILES; i++)
		m_tiles[i] = nullptr;
}

Radar::~Radar()
{
	Shutdown();
}

void Radar::Shutdown()
{
	for (int i = 0; i < NUM_TILES * NUM_TILES; i++) {
		if (m_tiles[i]) {
			m_tiles[i]->Release();
			m_tiles[i] = nullptr;
		}
	}
	if (m_radardisc) { m_radardisc->Release(); m_radardisc = nullptr; }
	if (m_centre) { m_centre->Release(); m_centre = nullptr; }
	if (m_north) { m_north->Release(); m_north = nullptr; }
}

bool Radar::LoadRadarTile(DXRender* render, IMG* img, int index)
{
	char name[32];
	sprintf(name, "radar%02d.txd", index);
	int fileId = img->GetFileIndexByName(name);
	if (fileId < 0)
		return false;

	char* fileBuffer = img->GetFileById(fileId);
	if (!fileBuffer)
		return false;

	size_t offset = 0;
	TextureDictionary txd;
	txd.read(fileBuffer, &offset);
	if (txd.texList.empty())
		return false;

	ID3D11ShaderResourceView* srv = nullptr;
	HRESULT hr = TextureFactory::CreateSrvFromNative(render, txd.texList[0], &srv);
	if (FAILED(hr) || !srv)
		return false;

	m_tiles[index] = srv;
	return true;
}

void Radar::LoadHudSprites(DXRender* render, const char* hudTxdPath)
{
	FILE* f = fopen(hudTxdPath, "rb");
	if (!f) {
		printf("[Error] Radar: cannot open %s\n", hudTxdPath);
		return;
	}
	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buffer = (char*)malloc(fileSize);
	if (!buffer) {
		fclose(f);
		return;
	}
	fread(buffer, 1, fileSize, f);
	fclose(f);

	size_t offset = 0;
	TextureDictionary txd;
	txd.read(buffer, &offset);

	for (size_t i = 0; i < txd.texList.size(); i++) {
		NativeTexture& t = txd.texList[i];
		ID3D11ShaderResourceView** dest = nullptr;
		if (_stricmp(t.name, "radardisc") == 0) dest = &m_radardisc;
		else if (_stricmp(t.name, "radar_centre") == 0) dest = &m_centre;
		else if (_stricmp(t.name, "radar_north") == 0) dest = &m_north;
		if (!dest || *dest)
			continue;
		TextureFactory::CreateSrvFromNative(render, t, dest);
	}
	free(buffer);
}

bool Radar::Init(DXRender* render, IMG* img, const char* hudTxdPath)
{
	if (!render || !img)
		return false;

	LoadHudSprites(render, hudTxdPath);

	int loaded = 0;
	for (int i = 0; i < NUM_TILES * NUM_TILES; i++) {
		if (LoadRadarTile(render, img, i))
			loaded++;
	}

	printf("[Info] Radar: tiles=%d/%d disc=%s centre=%s north=%s\n",
		loaded, NUM_TILES * NUM_TILES,
		m_radardisc ? "yes" : "no",
		m_centre ? "yes" : "no",
		m_north ? "yes" : "no");
	return loaded > 0;
}

void Radar::ScreenScale(float screenW, float screenH, float* sx, float* sy) const
{
	*sx = screenW / HUD_BASE_WIDTH;
	*sy = screenH / HUD_BASE_HEIGHT;
}

void Radar::GetTextureCorners(int tx, int ty, float outX[4], float outY[4]) const
{
	int x = tx - NUM_TILES / 2;
	int y = -(ty - NUM_TILES / 2);
	outX[0] = TILE_SIZE * (float)x;
	outY[0] = TILE_SIZE * (float)(y - 1);
	outX[1] = TILE_SIZE * (float)(x + 1);
	outY[1] = TILE_SIZE * (float)(y - 1);
	outX[2] = TILE_SIZE * (float)(x + 1);
	outY[2] = TILE_SIZE * (float)y;
	outX[3] = TILE_SIZE * (float)x;
	outY[3] = TILE_SIZE * (float)y;
}

void Radar::WorldToRadar(float wx, float wy, float* rx, float* ry) const
{
	float x = (wx - m_originX) * (1.0f / m_radarRange);
	float y = (wy - m_originY) * (1.0f / m_radarRange);
	*rx = m_cachedSin * y + m_cachedCos * x;
	*ry = m_cachedCos * y - m_cachedSin * x;
}

void Radar::RadarToScreen(float rx, float ry, float screenW, float screenH, float* sx, float* sy) const
{
	float scaleX, scaleY;
	ScreenScale(screenW, screenH, &scaleX, &scaleY);
	*sx = (rx + 1.0f) * 0.5f * (RADAR_WIDTH * scaleX) + (RADAR_LEFT * scaleX);
	*sy = (1.0f - ry) * 0.5f * (RADAR_HEIGHT * scaleY)
		+ (screenH - (RADAR_BOTTOM + RADAR_HEIGHT) * scaleY);
}

float Radar::LimitRadarPoint(float* rx, float* ry) const
{
	float dist = sqrtf((*rx) * (*rx) + (*ry) * (*ry));
	if (dist > 1.0f) {
		float inv = 1.0f / dist;
		*rx *= inv;
		*ry *= inv;
	}
	return dist;
}

void Radar::DrawMap(
	Sprite2d& sprites,
	float screenW, float screenH,
	float originGtaX, float originGtaY,
	float camHeadingRad)
{
	m_originX = originGtaX;
	m_originY = originGtaY;
	m_cachedSin = sinf(camHeadingRad);
	m_cachedCos = cosf(camHeadingRad);

	int tileX = (int)floorf((m_originX - MIN_X) / TILE_SIZE);
	int tileY = (int)ceilf((float)(NUM_TILES - 1) - (m_originY - MIN_Y) / TILE_SIZE);

	float scaleX, scaleY;
	ScreenScale(screenW, screenH, &scaleX, &scaleY);
	float radarL = RADAR_LEFT * scaleX;
	float radarT = screenH - (RADAR_BOTTOM + RADAR_HEIGHT) * scaleY;
	float radarR = radarL + RADAR_WIDTH * scaleX;
	float radarB = radarT + RADAR_HEIGHT * scaleY;

	/* Dark fill under map (re3 mask area). */
	sprites.DrawRect(radarL, radarT, radarR, radarB,
		0, 0, 1, 1, 0.0f, 0.0f, 0.0f, 0.55f, sprites.White(), true);

	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			int tx = tileX + dx;
			int ty = tileY + dy;
			if (tx < 0 || ty < 0 || tx >= NUM_TILES || ty >= NUM_TILES)
				continue;
			ID3D11ShaderResourceView* srv = m_tiles[tx + NUM_TILES * ty];
			if (!srv)
				continue;

			float wx[4], wy[4];
			GetTextureCorners(tx, ty, wx, wy);

			float ndcX[4], ndcY[4], u[4], v[4], cu[4], cv[4];
			for (int i = 0; i < 4; i++) {
				float rx, ry;
				WorldToRadar(wx[i], wy[i], &rx, &ry);
				float sx, sy;
				RadarToScreen(rx, ry, screenW, screenH, &sx, &sy);
				sprites.PixelToNdc(sx, sy, &ndcX[i], &ndcY[i]);

				/* Tex coords — re3 TransformRealWorldToTexCoordSpace. */
				u[i] = (wx[i] - ((float)tx * TILE_SIZE + MIN_X)) / TILE_SIZE;
				v[i] = -(wy[i] - (((float)NUM_TILES - (float)ty) * TILE_SIZE + MIN_Y)) / TILE_SIZE;

				cu[i] = (sx - radarL) / (radarR - radarL);
				cv[i] = (sy - radarT) / (radarB - radarT);
			}

			sprites.DrawQuad(ndcX, ndcY, u, v, cu, cv, 1, 1, 1, 1, srv);
		}
	}

	/* Radar disc border (slightly larger than radar rect). */
	if (m_radardisc) {
		float growX = 6.0f * scaleX;
		float growY = 6.0f * scaleY;
		sprites.DrawRect(
			radarL - growX, radarT - growY + 2.0f * scaleY,
			radarR + growX, radarB + growY + 2.0f * scaleY,
			0, 0, 1, 1, 0, 0, 0, 1, m_radardisc, false);
		sprites.DrawRect(
			radarL - growX, radarT - growY,
			radarR + growX, radarB + growY,
			0, 0, 1, 1, 1, 1, 1, 1, m_radardisc, false);
	}
}

void Radar::DrawBlips(
	Sprite2d& sprites,
	float screenW, float screenH,
	float originGtaX, float originGtaY,
	float camHeadingRad,
	float playerHeadingRad,
	float vehicleGtaX, float vehicleGtaY, bool hasVehicle)
{
	m_originX = originGtaX;
	m_originY = originGtaY;
	m_cachedSin = sinf(camHeadingRad);
	m_cachedCos = cosf(camHeadingRad);

	float scaleX, scaleY;
	ScreenScale(screenW, screenH, &scaleX, &scaleY);
	const float blipHalf = 8.0f;

	/* North indicator. */
	if (m_north) {
		float rx = 0.0f, ry = 0.0f;
		WorldToRadar(m_originX, m_originY + m_radarRange, &rx, &ry);
		LimitRadarPoint(&rx, &ry);
		float sx, sy;
		RadarToScreen(rx, ry, screenW, screenH, &sx, &sy);
		sprites.DrawRect(
			sx - blipHalf * scaleX, sy - blipHalf * scaleY,
			sx + blipHalf * scaleX, sy + blipHalf * scaleY,
			0, 0, 1, 1, 1, 1, 1, 1, m_north, false);
	}

	/* Vehicle blip. */
	if (hasVehicle) {
		float rx, ry;
		WorldToRadar(vehicleGtaX, vehicleGtaY, &rx, &ry);
		float dist = LimitRadarPoint(&rx, &ry);
		if (dist < 1.05f) {
			float sx, sy;
			RadarToScreen(rx, ry, screenW, screenH, &sx, &sy);
			sprites.DrawRect(
				sx - 4.0f * scaleX, sy - 4.0f * scaleY,
				sx + 4.0f * scaleX, sy + 4.0f * scaleY,
				0, 0, 1, 1, 0.99f, 0.54f, 0.95f, 1.0f, sprites.White(), false);
		}
	}

	/* Player centre — rotating arrow (re3 DrawRotatingRadarSprite). */
	if (m_centre) {
		float sx, sy;
		RadarToScreen(0.0f, 0.0f, screenW, screenH, &sx, &sy);

		/* angle = FindPlayerHeading() - (PI + cameraForward.Heading()) */
		const float kPi = 3.14159265f;
		float angle = playerHeadingRad - (kPi + camHeadingRad);
		float correctedAngle = angle - kPi * 0.25f;

		float sizeX = blipHalf * scaleX;
		float sizeY = blipHalf * scaleY;
		float ndcX[4], ndcY[4];
		float u[4] = { 0, 1, 1, 0 };
		float v[4] = { 0, 0, 1, 1 };
		float cu[4] = { -100, -100, -100, -100 };
		float cv[4] = { -100, -100, -100, -100 };

		for (int i = 0; i < 4; i++) {
			float cornerAngle = (float)i * (kPi * 0.5f) + correctedAngle;
			/* re3: x + Sin*sizeX, y - (0*Sin - 1*Cos)*sizeY = y + Cos*sizeY */
			float px = sx + sinf(cornerAngle) * sizeX;
			float py = sy + cosf(cornerAngle) * sizeY;
			sprites.PixelToNdc(px, py, &ndcX[i], &ndcY[i]);
		}
		sprites.DrawQuad(ndcX, ndcY, u, v, cu, cv, 1, 1, 1, 1, m_centre);
	}
}
