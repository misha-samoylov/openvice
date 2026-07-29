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

	ID3DBlob* psBlob = nullptr;
	hr = D3DReadFileToBlob(L"pixel_shader.cso", &psBlob);
	if (FAILED(hr)) {
		vsBlob->Release();
		printf("[Error] Player: cannot read pixel_shader.cso\n");
		return false;
	}

	ID3DBlob* shadowPsBlob = nullptr;
	hr = D3DReadFileToBlob(L"shadow_ps.cso", &shadowPsBlob);
	if (FAILED(hr)) {
		vsBlob->Release();
		psBlob->Release();
		printf("[Error] Player: cannot read shadow_ps.cso\n");
		return false;
	}

	D3D12_DESCRIPTOR_RANGE srv0 = {};
	srv0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srv0.NumDescriptors = 1;
	srv0.BaseShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE srv1 = {};
	srv1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srv1.NumDescriptors = 1;
	srv1.BaseShaderRegister = 1;

	D3D12_DESCRIPTOR_RANGE samp0 = {};
	samp0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	samp0.NumDescriptors = 1;
	samp0.BaseShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE samp1 = {};
	samp1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	samp1.NumDescriptors = 1;
	samp1.BaseShaderRegister = 1;

	D3D12_ROOT_PARAMETER params[6] = {};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[1].Descriptor.ShaderRegister = 1;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].DescriptorTable.NumDescriptorRanges = 1;
	params[2].DescriptorTable.pDescriptorRanges = &srv0;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[3].DescriptorTable.NumDescriptorRanges = 1;
	params[3].DescriptorTable.pDescriptorRanges = &srv1;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[4].DescriptorTable.NumDescriptorRanges = 1;
	params[4].DescriptorTable.pDescriptorRanges = &samp0;
	params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	params[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[5].DescriptorTable.NumDescriptorRanges = 1;
	params[5].DescriptorTable.pDescriptorRanges = &samp1;
	params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 6;
	rsDesc.pParameters = params;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* sigBlob = nullptr;
	ID3DBlob* errBlob = nullptr;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) {
		if (errBlob) {
			printf("[Error] Player root sig: %s\n", (char*)errBlob->GetBufferPointer());
			errBlob->Release();
		}
		vsBlob->Release();
		psBlob->Release();
		shadowPsBlob->Release();
		return false;
	}
	hr = render->GetDevice()->CreateRootSignature(
		0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));
	sigBlob->Release();
	if (FAILED(hr)) {
		vsBlob->Release();
		psBlob->Release();
		shadowPsBlob->Release();
		return false;
	}

	D3D12_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_RASTERIZER_DESC raster = {};
	raster.FillMode = D3D12_FILL_MODE_SOLID;
	raster.CullMode = D3D12_CULL_MODE_FRONT;
	raster.DepthClipEnable = TRUE;

	D3D12_BLEND_DESC blend = {};
	blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_DEPTH_STENCIL_DESC depth = {};
	depth.DepthEnable = TRUE;
	depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoColor = {};
	psoColor.pRootSignature = m_rootSig;
	psoColor.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoColor.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoColor.BlendState = blend;
	psoColor.SampleMask = UINT_MAX;
	psoColor.RasterizerState = raster;
	psoColor.DepthStencilState = depth;
	psoColor.InputLayout = { layout, ARRAYSIZE(layout) };
	psoColor.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoColor.NumRenderTargets = 1;
	psoColor.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoColor.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoColor.SampleDesc.Count = 1;
	hr = render->GetDevice()->CreateGraphicsPipelineState(&psoColor, IID_PPV_ARGS(&m_psoColor));
	if (FAILED(hr)) {
		vsBlob->Release();
		psBlob->Release();
		shadowPsBlob->Release();
		printf("[Error] Player: CreateGraphicsPipelineState color failed\n");
		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoShadow = {};
	psoShadow.pRootSignature = m_rootSig;
	psoShadow.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoShadow.PS = { shadowPsBlob->GetBufferPointer(), shadowPsBlob->GetBufferSize() };
	psoShadow.BlendState = blend;
	psoShadow.SampleMask = UINT_MAX;
	psoShadow.RasterizerState = raster;
	psoShadow.DepthStencilState = depth;
	psoShadow.InputLayout = { layout, ARRAYSIZE(layout) };
	psoShadow.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoShadow.NumRenderTargets = 0;
	psoShadow.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoShadow.SampleDesc.Count = 1;
	hr = render->GetDevice()->CreateGraphicsPipelineState(&psoShadow, IID_PPV_ARGS(&m_psoShadow));
	vsBlob->Release();
	psBlob->Release();
	shadowPsBlob->Release();
	if (FAILED(hr)) {
		printf("[Error] Player: CreateGraphicsPipelineState shadow failed\n");
		return false;
	}

	D3D12_SAMPLER_DESC samp = {};
	samp.Filter = D3D12_FILTER_ANISOTROPIC;
	samp.MaxAnisotropy = 16;
	samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samp.MaxLOD = D3D12_FLOAT32_MAX;
	m_samplerIndex = render->CreateSampler(samp);
	if (m_samplerIndex == UINT_MAX)
		return false;

	D3D12_SAMPLER_DESC cmp = {};
	cmp.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	cmp.AddressU = cmp.AddressV = cmp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	cmp.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	cmp.BorderColor[0] = cmp.BorderColor[1] = cmp.BorderColor[2] = cmp.BorderColor[3] = 1.0f;
	m_shadowSamplerIndex = render->CreateSampler(cmp);
	return m_shadowSamplerIndex != UINT_MAX;
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
		if (t.dxtCompression == 0)
			t.convertTo32Bit();
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

HRESULT Player::CreateTextureSRV(DXRender* render, LoadedTex& tex, GpuTexture* outTex)
{
	return TextureFactory::CreateSrvFromDxt(
		render, tex.data, tex.size, tex.width, tex.height, tex.dxt, outTex);
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

		SkinnedMeshPart part = {};
		part.indexCount = split.m_numIndices;
		part.vertexCount = vCount;
		part.topology = skinGeom->faceType == FACETYPE_STRIP
			? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
			: D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		part.hasAlpha = false;

		if (FAILED(render->CreateDefaultBuffer(
			verts.data(), (UINT64)(sizeof(SkinnedVertex) * vCount), &part.vb))) {
			clump->Clear();
			delete clump;
			return false;
		}

		if (FAILED(render->CreateDefaultBuffer(
			split.indices, (UINT64)(sizeof(unsigned int) * split.m_numIndices), &part.ib))) {
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
