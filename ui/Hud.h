#pragma once

#include <memory>

#include "ui/Sprite2d.h"
#include "ui/HudFont.h"
#include "ui/Radar.h"
#include "DXRender.hpp"
#include "loaders/IMG.hpp"
#include "world/GameWorld.h"

/* re3/miami CHud — money, clock, health, armour, radar. */
class Hud
{
public:
	Hud();
	~Hud();

	bool Init(DXRender* render, IMG* img);
	void Shutdown();

	void Update(float dt, GameWorld& world);
	void Draw(DXRender* render, GameWorld& world, float camYaw);

	void SetMoney(int money) { m_money = money; }
	int GetMoney() const { return m_money; }

private:
	float ScaleX(float screenW, float v) const;
	float ScaleY(float screenH, float v) const;
	float FromRight(float screenW, float v) const;
	float FromBottom(float screenH, float v) const;

	std::unique_ptr<Sprite2d> m_sprites;
	std::unique_ptr<HudFont> m_font;
	std::unique_ptr<Radar> m_radar;

	int m_money;
	float m_clockHours; /* fractional hours 0..24 */
	int m_frameCounter;
	bool m_ready;
};
