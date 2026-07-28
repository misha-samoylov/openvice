#include "Player.h"
#include "graphics/TextureFactory.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <Dds.h>

#include "loaders/Clump.h"
#include "renderware.h"

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

namespace {

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
	return TextureFactory::CreateSrvFromDxt(
		render, tex.data, tex.size, tex.width, tex.height, tex.dxt, outSRV);
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
