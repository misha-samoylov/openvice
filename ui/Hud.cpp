#include "ui/Hud.h"
#include "core/GameConfig.h"
#include "Player.h"
#include "Vehicle.h"

#include <stdio.h>
#include <cmath>

Hud::Hud()
	: m_money(400)
	, m_clockHours((float)WORLD_HOUR)
	, m_frameCounter(0)
	, m_ready(false)
{
}

Hud::~Hud()
{
	Shutdown();
}

bool Hud::Init(DXRender* render, IMG* img)
{
	m_sprites.reset(new Sprite2d());
	if (!m_sprites->Init(render)) {
		printf("[Error] Hud: Sprite2d init failed\n");
		return false;
	}

	m_font.reset(new HudFont());
	if (!m_font->Init(render, GTA_VC_FONTS_TXD)) {
		printf("[Warn] Hud: font failed — text HUD disabled\n");
		m_font.reset();
	}

	m_radar.reset(new Radar());
	if (!m_radar->Init(render, img, GTA_VC_HUD_TXD)) {
		printf("[Warn] Hud: radar failed — minimap disabled\n");
		m_radar.reset();
	}

	m_ready = true;
	printf("[Info] Hud ready\n");
	return true;
}

void Hud::Shutdown()
{
	m_ready = false;
	if (m_radar) {
		m_radar->Shutdown();
		m_radar.reset();
	}
	if (m_font) {
		m_font->Shutdown();
		m_font.reset();
	}
	if (m_sprites) {
		m_sprites->Shutdown();
		m_sprites.reset();
	}
}

float Hud::ScaleX(float screenW, float v) const
{
	return v * (screenW / HUD_BASE_WIDTH);
}

float Hud::ScaleY(float screenH, float v) const
{
	return v * (screenH / HUD_BASE_HEIGHT);
}

float Hud::FromRight(float screenW, float v) const
{
	return screenW - ScaleX(screenW, v);
}

float Hud::FromBottom(float screenH, float v) const
{
	return screenH - ScaleY(screenH, v);
}

void Hud::Update(float dt, GameWorld& world)
{
	(void)world;
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.1f) dt = 0.1f;

	/* ~1 real minute → 1 game minute (re3-ish pacing for a prototype clock). */
	m_clockHours += dt / 60.0f;
	while (m_clockHours >= 24.0f)
		m_clockHours -= 24.0f;

	m_frameCounter++;

	if (world.GetPlayer() && m_radar) {
		float range = Radar::MIN_RANGE;
		if (world.ControllingVehicle())
			range = Radar::MIN_RANGE + 80.0f;
		m_radar->SetRange(range);
	}
}

void Hud::Draw(DXRender* render, GameWorld& world, float camYaw)
{
	if (!m_ready || !render || !m_sprites)
		return;

	const float screenW = (float)render->GetBackBufferWidth();
	const float screenH = (float)render->GetBackBufferHeight();

	Player* player = world.GetPlayer();
	if (!player)
		return;

	m_sprites->Begin(render);

	/* ---- Text HUD (re3 CHud layout / colours) ---- */
	if (m_font) {
		const float textScaleMulX = screenW / HUD_BASE_WIDTH;
		const float textScaleMulY = screenH / HUD_BASE_HEIGHT;
		m_font->SetScale(textScaleMulX * 0.7f, textScaleMulY * 1.25f);
		m_font->SetDropShadow(
			ScaleX(screenW, 2.0f), ScaleY(screenH, 2.0f),
			0, 0, 0, 1);

		char buf[32];

		/* Clock — CLOCK_COLOR(97,194,247) at FROM_RIGHT(111), Y(22) */
		int hours = (int)m_clockHours;
		int mins = (int)((m_clockHours - (float)hours) * 60.0f);
		if (mins < 0) mins = 0;
		if (mins > 59) mins = 59;
		sprintf(buf, "%02d:%02d", hours, mins);
		m_font->SetColor(97.0f / 255.0f, 194.0f / 255.0f, 247.0f / 255.0f, 1.0f);
		m_font->PrintRight(*m_sprites, FromRight(screenW, 111.0f), ScaleY(screenH, 22.0f), buf);

		/* Money — MONEY_COLOR(0,207,133) at FROM_RIGHT(110), Y(43) */
		sprintf(buf, "$%08d", m_money);
		m_font->SetColor(0.0f, 207.0f / 255.0f, 133.0f / 255.0f, 1.0f);
		m_font->PrintRight(*m_sprites, FromRight(screenW, 110.0f), ScaleY(screenH, 43.0f), buf);

		/* Health — HEALTH_COLOR(255,150,225), icon '{' */
		float health = player->GetHealth();
		bool flashHealth = (health < 10.0f) && (m_frameCounter & 8);
		if (!flashHealth || (m_frameCounter & 8)) {
			sprintf(buf, "%03d", (int)(health + 0.5f));
			m_font->SetColor(255.0f / 255.0f, 150.0f / 255.0f, 225.0f / 255.0f, 1.0f);
			m_font->PrintRight(*m_sprites, FromRight(screenW, 110.0f), ScaleY(screenH, 65.0f), buf);
			m_font->PrintRight(*m_sprites, FromRight(screenW, 110.0f + 54.0f), ScaleY(screenH, 65.0f), "{");
		}

		/* Armour — ARMOUR_COLOR typically light blue/grey; re3 uses white-ish icon '<' */
		float armour = player->GetArmour();
		if (armour > 1.0f) {
			sprintf(buf, "%03d", (int)(armour + 0.5f));
			m_font->SetColor(185.0f / 255.0f, 185.0f / 255.0f, 185.0f / 255.0f, 1.0f);
			m_font->PrintRight(*m_sprites, FromRight(screenW, 182.0f), ScaleY(screenH, 65.0f), buf);
			m_font->PrintRight(*m_sprites, FromRight(screenW, 182.0f + 52.0f), ScaleY(screenH, 65.0f), "<");
		}
	}

	/* ---- Radar ---- */
	if (m_radar) {
		XMVECTOR p = player->GetPosition();
		float gtaX = XMVectorGetX(p);
		float gtaY = XMVectorGetZ(p); /* engine Z = GTA Y */

		float vehX = 0, vehY = 0;
		bool hasVeh = false;
		if (world.GetVehicle()) {
			XMVECTOR vp = world.GetVehicle()->GetPosition();
			vehX = XMVectorGetX(vp);
			vehY = XMVectorGetZ(vp);
			hasVeh = true;
		}

		/* Camera yaw in openvice Follow: Heading ≈ camYaw; map uses -yaw for axis remap. */
		float camHeading = -camYaw;
		float playerHeading = player->GetHeading();
		if (world.ControllingVehicle() && world.GetVehicle())
			playerHeading = world.GetVehicle()->GetHeading();

		m_radar->DrawMap(*m_sprites, screenW, screenH, gtaX, gtaY, camHeading);
		m_radar->DrawBlips(*m_sprites, screenW, screenH, gtaX, gtaY, camHeading,
			playerHeading, vehX, vehY, hasVeh);
	}

	m_sprites->Flush(render);
}
