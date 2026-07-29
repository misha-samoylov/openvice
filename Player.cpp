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
	m_rootSig = nullptr;
	m_psoColor = nullptr;
	m_psoShadow = nullptr;
	m_samplerIndex = UINT_MAX;
	m_shadowSamplerIndex = UINT_MAX;

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
	ID3D12GraphicsCommandList* cmd = render->GetCommandList();

	/* Invalidate mesh cache — different pipeline / root signature. */
	ctx.pso = nullptr;
	ctx.vb = nullptr;
	ctx.ib = nullptr;
	ctx.srvIndex = UINT_MAX;
	ctx.samplerIndex = UINT_MAX;
	ctx.topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

	const bool shadowPass = (ctx.pass == MESH_PASS_SHADOW);
	ID3D12PipelineState* pso = shadowPass ? m_psoShadow : m_psoColor;

	cmd->SetGraphicsRootSignature(m_rootSig);
	cmd->SetPipelineState(pso);
	ctx.pso = pso;

	const UINT boneBytes = sizeof(XMFLOAT4X4) * MAX_BONES;
	D3D12_GPU_VIRTUAL_ADDRESS boneAddr = 0;
	void* bonePtr = render->AllocFrameConstants(boneBytes, &boneAddr);
	if (!bonePtr)
		return;
	memcpy(bonePtr, m_skinPalette.data(), boneBytes);
	cmd->SetGraphicsRootConstantBufferView(1, boneAddr);

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
	for (int i = 0; i < 4; i++)
		cb.LightVP[i] = XMMatrixTranspose(ctx.lightViewProj[i]);
	cb.cascadeSplits = ctx.cascadeSplits;
	cb.fogColor = ctx.fogColor;
	cb.fogStart = ctx.fogStart;
	cb.fogEnd = ctx.fogEnd;
	cb.receiveShadows = shadowPass ? 0.0f : ctx.receiveShadows;
	cb.shadowBias = ctx.shadowBias;
	cb.windTime = 0.0f;
	cb.windAmount = 0.0f;
	cb.padWind[0] = 0.0f;
	cb.padWind[1] = 0.0f;

	D3D12_GPU_VIRTUAL_ADDRESS objAddr = 0;
	void* objPtr = render->AllocFrameConstants(sizeof(cb), &objAddr);
	if (!objPtr)
		return;
	memcpy(objPtr, &cb, sizeof(cb));
	cmd->SetGraphicsRootConstantBufferView(0, objAddr);

	UINT samp = (ctx.samplerIndex != UINT_MAX) ? ctx.samplerIndex : m_samplerIndex;

	for (size_t i = 0; i < m_meshes.size(); i++) {
		SkinnedMeshPart& part = m_meshes[i];
		if (!part.vb || !part.ib || !part.texture.Valid())
			continue;

		if (ctx.topology != part.topology) {
			cmd->IASetPrimitiveTopology(part.topology);
			ctx.topology = part.topology;
		}

		D3D12_VERTEX_BUFFER_VIEW vbv = {};
		vbv.BufferLocation = part.vb->GetGPUVirtualAddress();
		vbv.SizeInBytes = part.vertexCount * (UINT)sizeof(SkinnedVertex);
		vbv.StrideInBytes = (UINT)sizeof(SkinnedVertex);
		cmd->IASetVertexBuffers(0, 1, &vbv);
		ctx.vb = part.vb;

		D3D12_INDEX_BUFFER_VIEW ibv = {};
		ibv.BufferLocation = part.ib->GetGPUVirtualAddress();
		ibv.SizeInBytes = part.indexCount * sizeof(unsigned int);
		ibv.Format = DXGI_FORMAT_R32_UINT;
		cmd->IASetIndexBuffer(&ibv);
		ctx.ib = part.ib;

		UINT texSrv = part.texture.srvIndex;
		cmd->SetGraphicsRootDescriptorTable(2, render->GetSrvGpu(texSrv));
		UINT shadowSrv = (!shadowPass && ctx.shadowSrvIndex != UINT_MAX)
			? ctx.shadowSrvIndex : texSrv;
		cmd->SetGraphicsRootDescriptorTable(3, render->GetSrvGpu(shadowSrv));
		ctx.srvIndex = texSrv;

		cmd->SetGraphicsRootDescriptorTable(4, render->GetSamplerGpu(samp));
		UINT shadowSamp = (!shadowPass && ctx.shadowSamplerIndex != UINT_MAX)
			? ctx.shadowSamplerIndex
			: ((m_shadowSamplerIndex != UINT_MAX) ? m_shadowSamplerIndex : samp);
		cmd->SetGraphicsRootDescriptorTable(5, render->GetSamplerGpu(shadowSamp));

		cmd->DrawIndexedInstanced(part.indexCount, 1, 0, 0, 0);
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
		if (m_meshes[i].vb) {
			m_meshes[i].vb->Release();
			m_meshes[i].vb = nullptr;
		}
		if (m_meshes[i].ib) {
			m_meshes[i].ib->Release();
			m_meshes[i].ib = nullptr;
		}
		if (m_meshes[i].texture.resource) {
			m_meshes[i].texture.resource->Release();
			m_meshes[i].texture.resource = nullptr;
			m_meshes[i].texture.srvIndex = UINT_MAX;
		}
	}
	m_meshes.clear();

	for (size_t i = 0; i < m_textures.size(); i++)
		free(m_textures[i].data);
	m_textures.clear();

	if (m_psoColor) { m_psoColor->Release(); m_psoColor = nullptr; }
	if (m_psoShadow) { m_psoShadow->Release(); m_psoShadow = nullptr; }
	if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
	m_samplerIndex = UINT_MAX;
	m_shadowSamplerIndex = UINT_MAX;
}
