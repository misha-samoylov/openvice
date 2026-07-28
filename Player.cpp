#include "Player.h"
#include "CollisionWorld.h"
#include "core/GtaCoords.h"

#include <stdio.h>
#include <cmath>

#define __BT_DISABLE_SSE__
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"

using namespace DirectX;

namespace {

const float kFixedTimeStep = 1.0f / 60.0f;

} // namespace

bool Player::Init(IMG* img, DXRender* render, IFP* ifp)
{
	m_ifp = ifp;
	m_posX = 0.0f;
	m_posY = 50.0f;
	m_posZ = 0.0f;
	m_velX = 0.0f;
	m_velY = 0.0f;
	m_velZ = 0.0f;
	m_heading = 0.0f;
	m_moveSpeed = 2.25f;
	m_isStanding = false;
	m_wasStanding = false;
	m_world = nullptr;
	m_ghost = nullptr;
	m_capsule = nullptr;
	m_character = nullptr;
	m_collisionEnabled = true;
	m_animTime = 0.0f;
	m_animBlend = 1.0f;
	m_wasMoving = false;
	m_isJumping = false;
	m_landAnimTimer = 0.0f;
	m_currentAnim = nullptr;
	m_animIdle = nullptr;
	m_animWalk = nullptr;
	m_animRun = nullptr;
	m_animSprint = nullptr;
	m_animJumpLaunch = nullptr;
	m_animJumpGlide = nullptr;
	m_animJumpLand = nullptr;
	m_boneCount = 0;
	m_vs = nullptr;
	m_ps = nullptr;
	m_shadowPS = nullptr;
	m_layout = nullptr;
	m_sampler = nullptr;
	m_boneCB = nullptr;

	if (!ifp) {
		printf("[Error] Player: no IFP\n");
		return false;
	}

	/* Player group in ped.ifp (re3 ANIM_STD_WALK/RUN/RUNFAST): walk_player, run_player, SPRINT_civi. */
	m_animIdle = ifp->FindAnim("IDLE_STANCE");
	m_animWalk = ifp->FindAnim("walk_player");
	m_animRun = ifp->FindAnim("run_player");
	m_animSprint = ifp->FindAnim("SPRINT_civi");
	m_animJumpLaunch = ifp->FindAnim("JUMP_launch");
	m_animJumpGlide = ifp->FindAnim("JUMP_glide");
	m_animJumpLand = ifp->FindAnim("JUMP_land");
	if (!m_animIdle)
		m_animIdle = ifp->FindAnim("idle_stance");
	if (!m_animWalk)
		m_animWalk = ifp->FindAnim("walk_civi");
	if (!m_animRun)
		m_animRun = ifp->FindAnim("run_civi");
	if (!m_animSprint)
		m_animSprint = ifp->FindAnim("sprint_civi");
	if (!m_animSprint)
		m_animSprint = ifp->FindAnim("sprint_panic");
	if (!m_animJumpLaunch)
		m_animJumpLaunch = ifp->FindAnim("jump_launch");
	if (!m_animJumpGlide)
		m_animJumpGlide = ifp->FindAnim("jump_glide");
	if (!m_animJumpLand)
		m_animJumpLand = ifp->FindAnim("jump_land");

	if (!m_animIdle) {
		printf("[Error] Player: IDLE_STANCE not found in IFP\n");
		return false;
	}
	if (!m_animWalk)
		printf("[Warn] Player: walk_player not found, idle only\n");
	if (!m_animRun)
		printf("[Warn] Player: run_player not found\n");
	if (!m_animSprint)
		printf("[Warn] Player: SPRINT_civi not found\n");
	if (!m_animJumpLaunch)
		printf("[Warn] Player: JUMP_launch not found\n");

	if (!InitPipeline(render))
		return false;
	if (!LoadTextures(img))
		return false;
	if (!LoadModel(img, render))
		return false;

	SetAnim(m_animIdle);
	UpdateBoneMatrices();

	printf("[Info] Player ready at map center (%.1f, %.1f, %.1f) bones=%u meshes=%d\n",
		m_posX, m_posY, m_posZ, m_boneCount, (int)m_meshes.size());
	return true;
}

void Player::SetCollisionWorld(CollisionWorld* world)
{
	if (m_world == world)
		return;
	DestroyCharacterController();
	m_world = world;
	if (m_world)
		CreateCharacterController();
}

void Player::SetCollisionEnabled(bool enabled)
{
	m_collisionEnabled = enabled;
	if (!m_ghost)
		return;

	int flags = m_ghost->getCollisionFlags();
	if (enabled)
		flags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
	else
		flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
	m_ghost->setCollisionFlags(flags);

	if (!enabled && m_character)
		m_character->setWalkDirection(btVector3(0, 0, 0));
}

void Player::SyncPhysics()
{
	SyncFromBullet();
}

void Player::Update(float dt, float moveX, float moveZ, bool moving, bool walking, bool sprinting, bool jump)
{
	if (dt < 0.0f)
		dt = 0.0f;
	if (dt > 0.1f)
		dt = 0.1f;

	SyncFromBullet();
	m_wasStanding = m_isStanding;

	float desiredVX = 0.0f;
	float desiredVZ = 0.0f;

	bool wantSprint = moving && sprinting;
	bool wantWalk = moving && walking && !wantSprint;

	if (moving) {
		float len = sqrtf(moveX * moveX + moveZ * moveZ);
		if (len > 0.0001f) {
			moveX /= len;
			moveZ /= len;
			m_heading = atan2f(-moveX, moveZ);
			float speedMul = 4.0f;
			if (wantSprint)
				speedMul = 6.0f;
			else if (wantWalk)
				speedMul = 0.6f;
			float speed = m_moveSpeed * (speedMul / 1.8f);
			desiredVX = moveX * speed;
			desiredVZ = moveZ * speed;
		}
	}

	m_wasMoving = moving;
	m_velX = desiredVX;
	m_velZ = desiredVZ;

	if (m_character) {
		/* Walk direction is displacement per internal sim step (fixed 1/60). */
		m_character->setWalkDirection(
			btVector3(desiredVX, 0.0f, desiredVZ) * kFixedTimeStep);

		if (jump && m_character->canJump() && !m_isJumping) {
			m_character->jump(btVector3(0.0f, PED_JUMP_SPEED, 0.0f));
			m_isStanding = false;
			m_isJumping = true;
			m_landAnimTimer = 0.0f;
			if (m_animJumpLaunch)
				SetAnim(m_animJumpLaunch);
			else if (m_animJumpGlide)
				SetAnim(m_animJumpGlide);
		}
	}

	auto pickMoveAnim = [&]() -> IfpAnim* {
		if (wantSprint && m_animSprint)
			return m_animSprint;
		if (wantSprint && m_animRun)
			return m_animRun;
		if (wantWalk && m_animWalk)
			return m_animWalk;
		if (m_animRun)
			return m_animRun;
		return m_animWalk;
	};

	if (m_isJumping && m_isStanding) {
		m_isJumping = false;
		m_landAnimTimer = 0.35f;
		if (m_animJumpLand)
			SetAnim(m_animJumpLand);
	}

	if (m_landAnimTimer > 0.0f) {
		m_landAnimTimer -= dt;
		if (m_landAnimTimer <= 0.0f && m_isStanding) {
			m_landAnimTimer = 0.0f;
			if (moving) {
				IfpAnim* moveAnim = pickMoveAnim();
				if (moveAnim)
					SetAnim(moveAnim);
			} else {
				SetAnim(m_animIdle);
			}
		}
	} else if (m_isJumping || !m_isStanding) {
		if (m_animJumpGlide && m_currentAnim == m_animJumpLaunch) {
			if (m_currentAnim->totalLength > 0.0f && m_animTime >= m_currentAnim->totalLength * 0.95f)
				SetAnim(m_animJumpGlide);
		} else if (m_animJumpGlide && m_currentAnim != m_animJumpGlide && m_currentAnim != m_animJumpLaunch)
			SetAnim(m_animJumpGlide);
	} else if (moving) {
		IfpAnim* moveAnim = pickMoveAnim();
		if (moveAnim)
			SetAnim(moveAnim);
	} else {
		SetAnim(m_animIdle);
	}

	if (m_currentAnim) {
		m_animTime += dt;
		if (m_currentAnim->totalLength > 0.0f) {
			bool oneshot = (m_currentAnim == m_animJumpLaunch || m_currentAnim == m_animJumpLand);
			if (oneshot) {
				if (m_animTime > m_currentAnim->totalLength)
					m_animTime = m_currentAnim->totalLength;
			} else {
				while (m_animTime >= m_currentAnim->totalLength)
					m_animTime -= m_currentAnim->totalLength;
			}
		}
		SampleAnim(m_currentAnim, m_animTime);
		BlendAnimPose(dt);
		UpdateBoneMatrices();
	}

	if (m_posY < FALL_THROUGH_Y) {
		m_posY = 1000.0f;
		m_velY = 0.0f;
		WarpCharacter(m_posX, m_posY, m_posZ);
		if (!PlaceOnGround()) {
			WarpCharacter(m_posX, 5.0f, m_posZ);
			m_isStanding = false;
		}
	}
}

bool Player::PlaceOnGround()
{
	if (!m_world)
		return false;

	float groundY = 0.0f;
	if (!m_world->FindGroundY(m_posX, m_posY + 5.0f, m_posZ, &groundY)) {
		if (!m_world->FindGroundY(m_posX, 1000.0f, m_posZ, &groundY))
			return false;
	}

	WarpCharacter(m_posX, groundY + PED_FEET_OFFSET, m_posZ);
	m_isStanding = true;
	m_wasStanding = true;
	m_isJumping = false;
	m_landAnimTimer = 0.0f;
	return true;
}

void Player::Render(DXRender* render, MeshRenderContext& ctx)
{
	ID3D11DeviceContext* dc = render->GetDeviceContext();

	/* Invalidate mesh cache — different pipeline. */
	ctx.layout = nullptr;
	ctx.vs = nullptr;
	ctx.ps = nullptr;
	ctx.vb = nullptr;
	ctx.ib = nullptr;
	ctx.srv = nullptr;
	ctx.sampler = nullptr;

	dc->IASetInputLayout(m_layout);
	dc->VSSetShader(m_vs, nullptr, 0);

	/*
	 * Shadow pass: depth-only (null PS) for opaque parts so alpha-clip cannot
	 * discard the whole ped. Alpha meshes still use shadow_ps + clip.
	 */
	const bool shadowPass = (ctx.pass == MESH_PASS_SHADOW);
	if (!shadowPass) {
		dc->PSSetShader(m_ps, nullptr, 0);
		dc->PSSetSamplers(0, 1, &m_sampler);
	}

	dc->VSSetConstantBuffers(1, 1, &m_boneCB);

	if (!shadowPass && ctx.shadowSRV && ctx.shadowSampler) {
		dc->PSSetShaderResources(1, 1, &ctx.shadowSRV);
		dc->PSSetSamplers(1, 1, &ctx.shadowSampler);
	}

	dc->UpdateSubresource(m_boneCB, 0, nullptr, m_skinPalette.data(), 0, 0);

	/*
	 * Skinning stays in GTA/RW space. Remap to engine Y-up on the world
	 * matrix only (same (x,z,y) swap as map models). Heading = rot around GTA Z.
	 */
	XMMATRIX world =
		XMMatrixRotationZ(m_heading) *
		GtaCoords::ToEngineMatrix() *
		XMMatrixTranslation(m_posX, m_posY, m_posZ);

	XMMATRIX wvp = XMMatrixMultiply(world, ctx.viewProj);
	objectConstBuffer cb;
	cb.WVP = XMMatrixTranspose(wvp);
	cb.World = XMMatrixTranspose(world);
	cb.LightVP = XMMatrixTranspose(ctx.lightViewProj);
	cb.fogColor = ctx.fogColor;
	cb.fogStart = ctx.fogStart;
	cb.fogEnd = ctx.fogEnd;
	cb.receiveShadows = shadowPass ? 0.0f : ctx.receiveShadows;
	cb.shadowBias = ctx.shadowBias;
	cb.windTime = 0.0f;
	cb.windAmount = 0.0f;
	cb.padWind[0] = 0.0f;
	cb.padWind[1] = 0.0f;

	for (size_t i = 0; i < m_meshes.size(); i++) {
		SkinnedMeshPart& part = m_meshes[i];

		UINT stride = sizeof(SkinnedVertex);
		UINT offset = 0;
		dc->IASetVertexBuffers(0, 1, &part.vb, &stride, &offset);
		dc->IASetIndexBuffer(part.ib, DXGI_FORMAT_R32_UINT, 0);
		dc->IASetPrimitiveTopology(part.topology);

		dc->UpdateSubresource(part.objectCB, 0, nullptr, &cb, 0, 0);
		dc->VSSetConstantBuffers(0, 1, &part.objectCB);
		dc->PSSetConstantBuffers(0, 1, &part.objectCB);

		if (shadowPass) {
			if (part.hasAlpha && part.texture) {
				dc->PSSetShader(m_shadowPS, nullptr, 0);
				dc->PSSetSamplers(0, 1, &m_sampler);
				dc->PSSetShaderResources(0, 1, &part.texture);
			} else {
				/* Opaque: write depth for every covered pixel. */
				dc->PSSetShader(nullptr, nullptr, 0);
			}
		} else {
			if (part.texture)
				dc->PSSetShaderResources(0, 1, &part.texture);
		}

		dc->DrawIndexed(part.indexCount, 0, 0);
	}

	/* Reset D3D binding cache so map meshes rebind correctly. */
	ctx.ClearBindings();
}

XMVECTOR Player::GetPosition() const
{
	return XMVectorSet(m_posX, m_posY, m_posZ, 0.0f);
}

void Player::SetPosition(float x, float y, float z)
{
	WarpCharacter(x, y, z);
	m_isStanding = false;
	m_wasStanding = false;
	m_isJumping = false;
	m_landAnimTimer = 0.0f;
}

void Player::Cleanup()
{
	DestroyCharacterController();
	m_world = nullptr;

	for (size_t i = 0; i < m_meshes.size(); i++) {
		if (m_meshes[i].vb) m_meshes[i].vb->Release();
		if (m_meshes[i].ib) m_meshes[i].ib->Release();
		if (m_meshes[i].texture) m_meshes[i].texture->Release();
		if (m_meshes[i].objectCB) m_meshes[i].objectCB->Release();
	}
	m_meshes.clear();

	for (size_t i = 0; i < m_textures.size(); i++)
		free(m_textures[i].data);
	m_textures.clear();

	if (m_boneCB) { m_boneCB->Release(); m_boneCB = nullptr; }
	if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
	if (m_layout) { m_layout->Release(); m_layout = nullptr; }
	if (m_shadowPS) { m_shadowPS->Release(); m_shadowPS = nullptr; }
	if (m_ps) { m_ps->Release(); m_ps = nullptr; }
	if (m_vs) { m_vs->Release(); m_vs = nullptr; }
}
