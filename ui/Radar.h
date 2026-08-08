#pragma once

#include "ui/Sprite2d.h"
#include "DXRender.hpp"
#include "loaders/IMG.hpp"

#include <d3d11.h>

/* re3/miami CRadar — minimap from radar00..radar63 tiles + hud sprites. */
class Radar
{
public:
	static constexpr int NUM_TILES = 8;
	static constexpr float MIN_X = -2000.0f;
	static constexpr float MIN_Y = -2000.0f;
	static constexpr float MAX_X = 2000.0f;
	static constexpr float MAX_Y = 2000.0f;
	static constexpr float TILE_SIZE = (MAX_X - MIN_X) / (float)NUM_TILES;
	static constexpr float MIN_RANGE = 120.0f;
	static constexpr float MAX_RANGE = 350.0f;

	/* Screen layout from re3 Radar.h (640x480 space, scaled by Hud). */
	static constexpr float RADAR_LEFT = 40.0f;
	static constexpr float RADAR_BOTTOM = 40.0f;
	static constexpr float RADAR_WIDTH = 94.0f;
	static constexpr float RADAR_HEIGHT = 76.0f;

	Radar();
	~Radar();

	bool Init(DXRender* render, IMG* img, const char* hudTxdPath);
	void Shutdown();

	void SetRange(float range) { m_radarRange = range; }
	float GetRange() const { return m_radarRange; }

	/* originGtaX/Y = player map position (GTA XY). headingRad = camera forward. */
	void DrawMap(
		Sprite2d& sprites,
		float screenW, float screenH,
		float originGtaX, float originGtaY,
		float camHeadingRad
	);
	void DrawBlips(
		Sprite2d& sprites,
		float screenW, float screenH,
		float originGtaX, float originGtaY,
		float camHeadingRad,
		float playerHeadingRad,
		float vehicleGtaX, float vehicleGtaY, bool hasVehicle
	);

private:
	void LoadHudSprites(DXRender* render, const char* hudTxdPath);
	bool LoadRadarTile(DXRender* render, IMG* img, int index);
	void GetTextureCorners(int tx, int ty, float outX[4], float outY[4]) const;
	void WorldToRadar(float wx, float wy, float* rx, float* ry) const;
	void RadarToScreen(float rx, float ry, float screenW, float screenH, float* sx, float* sy) const;
	void ScreenScale(float screenW, float screenH, float* sx, float* sy) const;
	float LimitRadarPoint(float* rx, float* ry) const;

	ID3D11ShaderResourceView* m_tiles[NUM_TILES * NUM_TILES];
	ID3D11ShaderResourceView* m_radardisc;
	ID3D11ShaderResourceView* m_centre;
	ID3D11ShaderResourceView* m_north;

	float m_radarRange;
	float m_originX, m_originY;
	float m_cachedSin, m_cachedCos;
};
