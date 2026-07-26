#include "Player.h"
#include "CollisionWorld.h"

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <algorithm>

#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <Dds.h>

#include "loaders/Clump.h"
#include "renderware.h"

#define __BT_DISABLE_SSE__
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

namespace {

/* Capsule total height ≈ 2 * PED_FEET_OFFSET so origin matches re3 feet offset. */
const float kPedCapsuleRadius = PED_SPHERE_RADIUS;
const float kPedCapsuleCylinder = (PED_FEET_OFFSET * 2.0f) - (kPedCapsuleRadius * 2.0f);
const float kFixedTimeStep = 1.0f / 60.0f;

/* GTA (x,y,z) -> engine (x,z,y) — same as map meshes, applied only on world. */
XMMATRIX GtaToEngine()
{
	return XMMATRIX(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

void QuatFromMatrix(const float rot9[9], XMFLOAT4& out)
{
	XMMATRIX m = XMMatrixIdentity();
	m.r[0] = XMVectorSet(rot9[0], rot9[1], rot9[2], 0.0f);
	m.r[1] = XMVectorSet(rot9[3], rot9[4], rot9[5], 0.0f);
	m.r[2] = XMVectorSet(rot9[6], rot9[7], rot9[8], 0.0f);
	XMVECTOR q = XMQuaternionRotationMatrix(m);
	XMStoreFloat4(&out, q);
}

/* RW Matrix stores flags/pads in .w of right/up/at — must be 0 for skin math. */
void SanitizeRwMatrix(XMFLOAT4X4& m)
{
	m._14 = 0.0f;
	m._24 = 0.0f;
	m._34 = 0.0f;
	m._44 = 1.0f;
}

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

bool Player::InitPipeline(DXRender* render)
{
	HRESULT hr;
	ID3DBlob* vsBlob = nullptr;
	hr = D3DReadFileToBlob(L"skinned_vs.cso", &vsBlob);
	if (FAILED(hr)) {
		printf("[Error] Player: cannot read skinned_vs.cso\n");
		return false;
	}

	hr = render->GetDevice()->CreateVertexShader(
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
	if (FAILED(hr)) {
		vsBlob->Release();
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = render->GetDevice()->CreateInputLayout(
		layout, ARRAYSIZE(layout),
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
		&m_layout);
	vsBlob->Release();
	if (FAILED(hr)) {
		printf("[Error] Player: CreateInputLayout failed\n");
		return false;
	}

	ID3DBlob* psBlob = nullptr;
	hr = D3DReadFileToBlob(L"pixel_shader.cso", &psBlob);
	if (FAILED(hr)) {
		printf("[Error] Player: cannot read pixel_shader.cso\n");
		return false;
	}
	hr = render->GetDevice()->CreatePixelShader(
		psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);
	psBlob->Release();
	if (FAILED(hr))
		return false;

	ID3DBlob* shadowPsBlob = nullptr;
	hr = D3DReadFileToBlob(L"shadow_ps.cso", &shadowPsBlob);
	if (FAILED(hr)) {
		printf("[Error] Player: cannot read shadow_ps.cso\n");
		return false;
	}
	hr = render->GetDevice()->CreatePixelShader(
		shadowPsBlob->GetBufferPointer(), shadowPsBlob->GetBufferSize(), nullptr, &m_shadowPS);
	shadowPsBlob->Release();
	if (FAILED(hr))
		return false;

	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 16;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = render->GetDevice()->CreateSamplerState(&sampDesc, &m_sampler);
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(XMFLOAT4X4) * MAX_BONES;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = render->GetDevice()->CreateBuffer(&bd, nullptr, &m_boneCB);
	if (FAILED(hr))
		return false;

	return true;
}

bool Player::LoadTextures(IMG* img)
{
	char result_name[MAX_LENGTH_FILENAME + 4];
	strcpy(result_name, "player.txd");

	int fileId = img->GetFileIndexByName(result_name);
	if (fileId == -1)
		return false;

	char* fileBuffer = img->GetFileById(fileId);
	size_t offset = 0;
	TextureDictionary txd;
	txd.read(fileBuffer, &offset);

	for (uint32_t i = 0; i < txd.texList.size(); i++) {
		NativeTexture& t = txd.texList[i];
		LoadedTex tex;
		memset(&tex, 0, sizeof(tex));
		memcpy(tex.name, t.name, sizeof(tex.name));
		tex.size = t.dataSizes[0];
		tex.data = (uint8_t*)malloc(tex.size);
		memcpy(tex.data, t.texels[0], tex.size);
		tex.width = t.width[0];
		tex.height = t.height[0];
		tex.dxt = t.dxtCompression;
		tex.depth = t.depth;
		tex.isAlpha = t.IsAlpha;
		m_textures.push_back(tex);
	}

	printf("[Info] Player: loaded %d textures from player.txd\n", (int)m_textures.size());
	return !m_textures.empty();
}

int Player::FindTexIndex(const char* name) const
{
	for (size_t i = 0; i < m_textures.size(); i++) {
		if (_stricmp(m_textures[i].name, name) == 0)
			return (int)i;
	}
	return -1;
}

HRESULT Player::CreateTextureSRV(DXRender* render, LoadedTex& tex, ID3D11ShaderResourceView** outSRV)
{
	struct DDS_File {
		DWORD dwMagic;
		DDS_HEADER header;
	};

	DDS_File dds;
	dds.dwMagic = 0x20534444; /* 'DDS ' */
	memset(&dds.header, 0, sizeof(dds.header));
	dds.header.size = sizeof(DDS_HEADER);
	dds.header.flags = 0;
	dds.header.width = tex.width;
	dds.header.height = tex.height;
	dds.header.pitchOrLinearSize = tex.width * tex.height;
	dds.header.mipMapCount = 0;
	dds.header.ddspf.size = sizeof(DDS_PIXELFORMAT);
	dds.header.ddspf.flags = 0x00000004; /* DDS_FOURCC */
	switch (tex.dxt) {
	default:
	case 1: dds.header.ddspf.fourCC = MAKEFOURCC('D', 'X', 'T', '1'); break;
	case 3: dds.header.ddspf.fourCC = MAKEFOURCC('D', 'X', 'T', '3'); break;
	case 4: dds.header.ddspf.fourCC = MAKEFOURCC('D', 'X', 'T', '4'); break;
	case 5: dds.header.ddspf.fourCC = MAKEFOURCC('D', 'X', 'T', '5'); break;
	}

	size_t len = sizeof(dds) + tex.size;
	uint8_t* buf = (uint8_t*)malloc(len);
	memcpy(buf, &dds, sizeof(dds));
	memcpy(buf + sizeof(dds), tex.data, tex.size);

	ScratchImage image;
	HRESULT hr = LoadFromDDSMemory(buf, len, DDS_FLAGS_NONE, nullptr, image);
	if (SUCCEEDED(hr)) {
		hr = CreateShaderResourceView(
			render->GetDevice(),
			image.GetImages(),
			image.GetImageCount(),
			image.GetMetadata(),
			outSRV);
	}
	free(buf);
	return hr;
}

bool Player::LoadModel(IMG* img, DXRender* render)
{
	int fileId = img->GetFileIndexByName("player.dff");
	if (fileId == -1)
		return false;

	char* fileBuffer = img->GetFileById(fileId);
	Clump* clump = new Clump();
	clump->Read(fileBuffer);

	FrameList* frames = clump->GetFrameList();
	Geometry* skinGeom = nullptr;
	int skinGeomIndex = -1;

	for (uint32_t i = 0; i < clump->m_numGeometries; i++) {
		Geometry* g = clump->GetGeometryList()[i];
		if (g->hasSkin) {
			skinGeom = g;
			skinGeomIndex = (int)i;
			break;
		}
	}

	if (!skinGeom) {
		printf("[Error] Player: no skinned geometry in player.dff\n");
		clump->Clear();
		delete clump;
		return false;
	}

	/* Hierarchy lives on the frame that owns the bone ID table. */
	Frame* hierFrame = nullptr;
	for (int i = 0; i < frames->GetNumFrames(); i++) {
		Frame* f = frames->GetFrame(i);
		if (f->GetAnimBoneCount() > 0) {
			hierFrame = f;
			break;
		}
	}

	if (!hierFrame) {
		printf("[Error] Player: no HAnim hierarchy in player.dff\n");
		clump->Clear();
		delete clump;
		return false;
	}

	m_boneCount = hierFrame->GetAnimBoneCount();
	if (m_boneCount == 0 || m_boneCount > (uint32_t)MAX_BONES) {
		printf("[Error] Player: bad bone count %u\n", m_boneCount);
		clump->Clear();
		delete clump;
		return false;
	}

	if (skinGeom->boneCount != m_boneCount) {
		printf("[Warn] Player: skin bones %u vs hier %u\n", skinGeom->boneCount, m_boneCount);
	}

	m_boneIds.assign(hierFrame->GetHAnimBoneIds(), hierFrame->GetHAnimBoneIds() + m_boneCount);
	m_boneFlags.assign(hierFrame->GetHAnimBoneTypes(), hierFrame->GetHAnimBoneTypes() + m_boneCount);
	m_boneRest.resize(m_boneCount);
	m_inverseBind.resize(m_boneCount);
	m_localQuat.resize(m_boneCount);
	m_localPos.resize(m_boneCount);
	m_blendFromQuat.resize(m_boneCount);
	m_blendFromPos.resize(m_boneCount);
	m_boneWorld.resize(m_boneCount);
	m_skinPalette.resize(MAX_BONES);
	m_boneSeq.resize(m_boneCount, nullptr);

	for (uint32_t i = 0; i < m_boneCount; i++) {
		m_boneRest[i].valid = false;
		m_boneRest[i].pos = XMFLOAT3(0, 0, 0);
		m_boneRest[i].quat = XMFLOAT4(0, 0, 0, 1);
		m_localQuat[i] = m_boneRest[i].quat;
		m_localPos[i] = m_boneRest[i].pos;

		const float* inv = &skinGeom->inverseMatrices[i * 16];
		memcpy(&m_inverseBind[i], inv, sizeof(float) * 16);
		/* Critical: clear RW flags in .w channels (librw does invMats[i].flags = 0). */
		SanitizeRwMatrix(m_inverseBind[i]);
	}

	/* Rest pose from frames matched by bone ID (GTA/RW space). */
	for (int fi = 0; fi < frames->GetNumFrames(); fi++) {
		Frame* f = frames->GetFrame(fi);
		if (!f->HasHAnim() || f->GetHAnimBoneId() < 0)
			continue;

		for (uint32_t b = 0; b < m_boneCount; b++) {
			if (m_boneIds[b] != f->GetHAnimBoneId())
				continue;

			const float* pos = f->GetPosition();
			m_boneRest[b].pos = XMFLOAT3(pos[0], pos[1], pos[2]);
			QuatFromMatrix(f->GetRotationMatrix(), m_boneRest[b].quat);
			m_boneRest[b].valid = true;
			m_localPos[b] = m_boneRest[b].pos;
			m_localQuat[b] = m_boneRest[b].quat;
			break;
		}
	}

	/* Build GPU meshes from binmesh splits. */
	for (uint32_t si = 0; si < skinGeom->splits.size(); si++) {
		Split& split = skinGeom->splits[si];
		uint32_t vCount = skinGeom->vertexCount;

		std::vector<SkinnedVertex> verts(vCount);
		for (uint32_t v = 0; v < vCount; v++) {
			SkinnedVertex& sv = verts[v];
			/* Keep GTA/RW model space; world matrix applies GtaToEngine. */
			sv.x = skinGeom->vertices[v * 3 + 0];
			sv.y = skinGeom->vertices[v * 3 + 1];
			sv.z = skinGeom->vertices[v * 3 + 2];
			sv.u = 0.0f;
			sv.v = 0.0f;
			if (skinGeom->flags & FLAGS_TEXTURED) {
				sv.u = skinGeom->texCoords[0][v * 2 + 0];
				sv.v = skinGeom->texCoords[0][v * 2 + 1];
			}

			uint32_t packed = skinGeom->vertexBoneIndices[v];
			sv.boneIndices[0] = (uint8_t)(packed & 0xFF);
			sv.boneIndices[1] = (uint8_t)((packed >> 8) & 0xFF);
			sv.boneIndices[2] = (uint8_t)((packed >> 16) & 0xFF);
			sv.boneIndices[3] = (uint8_t)((packed >> 24) & 0xFF);
			sv.boneWeights[0] = skinGeom->vertexBoneWeights[v * 4 + 0];
			sv.boneWeights[1] = skinGeom->vertexBoneWeights[v * 4 + 1];
			sv.boneWeights[2] = skinGeom->vertexBoneWeights[v * 4 + 2];
			sv.boneWeights[3] = skinGeom->vertexBoneWeights[v * 4 + 3];
		}

		SkinnedMeshPart part;
		memset(&part, 0, sizeof(part));
		part.indexCount = split.m_numIndices;
		part.topology = skinGeom->faceType == FACETYPE_STRIP
			? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
			: D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		part.hasAlpha = false;
		part.texture = nullptr;

		D3D11_BUFFER_DESC bdv;
		ZeroMemory(&bdv, sizeof(bdv));
		bdv.Usage = D3D11_USAGE_DEFAULT;
		bdv.ByteWidth = (UINT)(sizeof(SkinnedVertex) * vCount);
		bdv.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA datav;
		ZeroMemory(&datav, sizeof(datav));
		datav.pSysMem = verts.data();
		if (FAILED(render->GetDevice()->CreateBuffer(&bdv, &datav, &part.vb))) {
			clump->Clear();
			delete clump;
			return false;
		}

		D3D11_BUFFER_DESC bdi;
		ZeroMemory(&bdi, sizeof(bdi));
		bdi.Usage = D3D11_USAGE_DEFAULT;
		bdi.ByteWidth = sizeof(unsigned int) * split.m_numIndices;
		bdi.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA datai;
		ZeroMemory(&datai, sizeof(datai));
		datai.pSysMem = split.indices;
		if (FAILED(render->GetDevice()->CreateBuffer(&bdi, &datai, &part.ib))) {
			clump->Clear();
			delete clump;
			return false;
		}

		D3D11_BUFFER_DESC bdcb;
		ZeroMemory(&bdcb, sizeof(bdcb));
		bdcb.Usage = D3D11_USAGE_DEFAULT;
		bdcb.ByteWidth = sizeof(objectConstBuffer);
		bdcb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(render->GetDevice()->CreateBuffer(&bdcb, nullptr, &part.objectCB))) {
			clump->Clear();
			delete clump;
			return false;
		}

		if (split.matIndex < skinGeom->m_numMaterials) {
			Material* mat = skinGeom->materialList[split.matIndex];
			int ti = FindTexIndex(mat->texture.name);
			if (ti >= 0) {
				CreateTextureSRV(render, m_textures[ti], &part.texture);
				part.hasAlpha = m_textures[ti].isAlpha;
			} else {
				printf("[Warn] Player: texture '%s' not found\n", mat->texture.name);
			}
		}

		m_meshes.push_back(part);
	}

	(void)skinGeomIndex;
	clump->Clear();
	delete clump;
	return !m_meshes.empty();
}

void Player::SetAnim(IfpAnim* anim)
{
	if (!anim || anim == m_currentAnim)
		return;

	/* Snapshot current pose and crossfade into the new clip. */
	if (m_currentAnim && m_boneCount > 0) {
		m_blendFromQuat = m_localQuat;
		m_blendFromPos = m_localPos;
		m_animBlend = 0.0f;
	} else {
		m_animBlend = 1.0f;
	}

	m_currentAnim = anim;
	m_animTime = 0.0f;
	BindAnims();
}

void Player::BlendAnimPose(float dt)
{
	if (m_animBlend >= 1.0f || m_boneCount == 0)
		return;

	m_animBlend += dt / ANIM_BLEND_DURATION;
	if (m_animBlend > 1.0f)
		m_animBlend = 1.0f;

	const float t = m_animBlend;
	for (uint32_t b = 0; b < m_boneCount; b++) {
		XMVECTOR qFrom = XMLoadFloat4(&m_blendFromQuat[b]);
		XMVECTOR qTo = XMLoadFloat4(&m_localQuat[b]);
		XMStoreFloat4(&m_localQuat[b], XMQuaternionSlerp(qFrom, qTo, t));

		m_localPos[b].x = m_blendFromPos[b].x + (m_localPos[b].x - m_blendFromPos[b].x) * t;
		m_localPos[b].y = m_blendFromPos[b].y + (m_localPos[b].y - m_blendFromPos[b].y) * t;
		m_localPos[b].z = m_blendFromPos[b].z + (m_localPos[b].z - m_blendFromPos[b].z) * t;
	}
}

void Player::BindAnims()
{
	for (uint32_t i = 0; i < m_boneCount; i++)
		m_boneSeq[i] = nullptr;

	if (!m_currentAnim)
		return;

	for (size_t s = 0; s < m_currentAnim->sequences.size(); s++) {
		IfpSequence& seq = m_currentAnim->sequences[s];
		if (seq.frames.empty() || seq.boneTag < 0)
			continue;

		for (uint32_t b = 0; b < m_boneCount; b++) {
			if (m_boneIds[b] == seq.boneTag) {
				m_boneSeq[b] = &seq;
				break;
			}
		}
	}
}

void Player::SampleAnim(IfpAnim* anim, float time)
{
	if (!anim)
		return;

	for (uint32_t b = 0; b < m_boneCount; b++) {
		if (m_boneRest[b].valid) {
			m_localQuat[b] = m_boneRest[b].quat;
			m_localPos[b] = m_boneRest[b].pos;
		} else {
			m_localQuat[b] = XMFLOAT4(0, 0, 0, 1);
			m_localPos[b] = XMFLOAT3(0, 0, 0);
		}

		IfpSequence* seq = m_boneSeq[b];
		if (!seq || seq->frames.empty())
			continue;

		float t = time;
		if (anim->totalLength > 0.0f) {
			while (t >= anim->totalLength)
				t -= anim->totalLength;
			while (t < 0.0f)
				t += anim->totalLength;
		}

		int frameA = 0;
		int frameB = 0;
		float remaining = 0.0f;
		const int numFrames = (int)seq->frames.size();

		if (numFrames == 1) {
			frameA = frameB = 0;
			remaining = 0.0f;
		} else {
			/* Match re3 CAnimBlendNode::FindKeyFrame */
			frameA = 0;
			frameB = 0;
			bool ok = true;
			while (ok && (frameA + 1) < numFrames && t > seq->frames[frameA + 1].time) {
				frameA++;
				t -= seq->frames[frameA].time;
				frameB = frameA;
				if (frameA + 1 >= numFrames) {
					frameA = 0;
					frameB = 0;
					ok = false;
				}
			}
			if (ok && frameA + 1 < numFrames) {
				frameA++;
				remaining = seq->frames[frameA].time - t;
				frameB = frameA - 1;
				if (frameB < 0)
					frameB += numFrames;
			} else {
				frameA = numFrames - 1;
				frameB = frameA;
				remaining = 0.0f;
			}
		}

		const IfpKeyFrame& kfA = seq->frames[frameA];
		const IfpKeyFrame& kfB = seq->frames[frameB];
		float blend = 0.0f;
		if (kfA.time > 0.0001f)
			blend = (kfA.time - remaining) / kfA.time;
		if (blend < 0.0f) blend = 0.0f;
		if (blend > 1.0f) blend = 1.0f;

		XMVECTOR qA = XMVectorSet(kfA.rx, kfA.ry, kfA.rz, kfA.rw);
		XMVECTOR qB = XMVectorSet(kfB.rx, kfB.ry, kfB.rz, kfB.rw);
		XMVECTOR q = XMQuaternionSlerp(qB, qA, blend);
		XMStoreFloat4(&m_localQuat[b], q);

		/*
		 * Play all anims in-place: ignore root-motion translation on Root/Pelvis
		 * (bone tags 0/1). WASD already moves the entity; baked IFP translation
		 * would slide Tommy forward then snap back on loop.
		 */
		if (kfA.hasTranslation && seq->boneTag != 0 && seq->boneTag != 1) {
			m_localPos[b].x = kfB.tx + blend * (kfA.tx - kfB.tx);
			m_localPos[b].y = kfB.ty + blend * (kfA.ty - kfB.ty);
			m_localPos[b].z = kfB.tz + blend * (kfA.tz - kfB.tz);
		}
	}
}

void Player::UpdateBoneMatrices()
{
	/* Build object-space bone matrices with PUSH/POP stack (librw HAnim). */
	XMMATRIX stack[64];
	int sp = 0;
	XMMATRIX parent = XMMatrixIdentity();
	stack[sp++] = parent;

	const uint32_t POP = 1;
	const uint32_t PUSH = 2;

	for (uint32_t i = 0; i < m_boneCount; i++) {
		XMVECTOR q = XMLoadFloat4(&m_localQuat[i]);
		XMMATRIX animMat = XMMatrixRotationQuaternion(q);
		animMat.r[3] = XMVectorSet(m_localPos[i].x, m_localPos[i].y, m_localPos[i].z, 1.0f);

		XMMATRIX cur = XMMatrixMultiply(animMat, parent);
		XMStoreFloat4x4(&m_boneWorld[i], cur);

		if (m_boneFlags[i] & PUSH) {
			if (sp < 64)
				stack[sp++] = parent;
		}
		parent = cur;
		if (m_boneFlags[i] & POP) {
			if (sp > 0)
				parent = stack[--sp];
		}

		/* skin = inverseBind * boneWorld (LOCALSPACEMATRICES path) */
		XMMATRIX invBind = XMLoadFloat4x4(&m_inverseBind[i]);
		XMMATRIX skin = XMMatrixMultiply(invBind, cur);
		XMStoreFloat4x4(&m_skinPalette[i], XMMatrixTranspose(skin));
	}

	for (uint32_t i = m_boneCount; i < (uint32_t)MAX_BONES; i++)
		XMStoreFloat4x4(&m_skinPalette[i], XMMatrixIdentity());
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

void Player::CreateCharacterController()
{
	DestroyCharacterController();
	if (!m_world || !m_world->GetDynamicsWorld())
		return;

	btDiscreteDynamicsWorld* dyn = m_world->GetDynamicsWorld();

	float cyl = kPedCapsuleCylinder;
	if (cyl < 0.2f)
		cyl = 0.2f;

	m_capsule = new btCapsuleShape(kPedCapsuleRadius, cyl);
	m_ghost = new btPairCachingGhostObject();
	btTransform start;
	start.setIdentity();
	start.setOrigin(btVector3(m_posX, m_posY, m_posZ));
	m_ghost->setWorldTransform(start);
	m_ghost->setCollisionShape(m_capsule);
	m_ghost->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

	m_character = new btKinematicCharacterController(
		m_ghost, m_capsule, 0.35f, btVector3(0.0f, 1.0f, 0.0f));
	m_character->setGravity(btVector3(0.0f, -PED_GRAVITY, 0.0f));
	m_character->setFallSpeed(50.0f);
	m_character->setJumpSpeed(PED_JUMP_SPEED);
	m_character->setMaxSlope(btRadians(50.0f));

	dyn->addCollisionObject(
		m_ghost,
		btBroadphaseProxy::CharacterFilter,
		btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::DebrisFilter);
	dyn->addAction(m_character);

	WarpCharacter(m_posX, m_posY, m_posZ);
	m_collisionEnabled = true;
	printf("[Info] Player: Bullet character controller ready\n");
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

void Player::DestroyCharacterController()
{
	if (m_world && m_world->GetDynamicsWorld()) {
		btDiscreteDynamicsWorld* dyn = m_world->GetDynamicsWorld();
		if (m_character)
			dyn->removeAction(m_character);
		if (m_ghost)
			dyn->removeCollisionObject(m_ghost);
	}
	if (m_character) {
		delete m_character;
		m_character = nullptr;
	}
	if (m_ghost) {
		delete m_ghost;
		m_ghost = nullptr;
	}
	if (m_capsule) {
		delete m_capsule;
		m_capsule = nullptr;
	}
}

void Player::WarpCharacter(float x, float y, float z)
{
	m_posX = x;
	m_posY = y;
	m_posZ = z;
	m_velX = m_velY = m_velZ = 0.0f;
	if (m_character)
		m_character->warp(btVector3(x, y, z));
	else if (m_ghost) {
		btTransform t = m_ghost->getWorldTransform();
		t.setOrigin(btVector3(x, y, z));
		m_ghost->setWorldTransform(t);
	}
}

void Player::SyncFromBullet()
{
	if (!m_ghost)
		return;
	const btVector3& p = m_ghost->getWorldTransform().getOrigin();
	m_posX = p.x();
	m_posY = p.y();
	m_posZ = p.z();
	if (m_character) {
		btVector3 lv = m_character->getLinearVelocity();
		m_velX = lv.x();
		m_velY = lv.y();
		m_velZ = lv.z();
		m_isStanding = m_character->onGround();
	}
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
			float speedMul = 3.0f;
			if (wantSprint)
				speedMul = 5.4f;
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
		GtaToEngine() *
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
