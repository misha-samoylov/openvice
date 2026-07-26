#pragma once

#include <DirectXMath.h>
#include <vector>
#include <stdint.h>

#include "DXRender.hpp"
#include "Mesh.hpp"
#include "loaders/IMG.hpp"
#include "loaders/IFP.h"

class CollisionWorld;
class btPairCachingGhostObject;
class btCapsuleShape;
class btKinematicCharacterController;

using namespace DirectX;

class Player
{
public:
	bool Init(IMG* img, DXRender* render, IFP* ifp);
	void Cleanup();

	void SetCollisionWorld(CollisionWorld* world);

	/* Move mode mirrors re3 PEDMOVE_WALK / RUN / SPRINT. Default when moving is run.
	 * Applies Bullet character input; call CollisionWorld::Step after Update. */
	void Update(float dt, float moveX, float moveZ, bool moving,
		bool walking = false, bool sprinting = false, bool jump = false);
	/* Call after CollisionWorld::Step to refresh pose for camera/render. */
	void SyncPhysics();
	void Render(DXRender* render, MeshRenderContext& ctx);

	XMVECTOR GetPosition() const;
	float GetHeading() const { return m_heading; }
	void SetPosition(float x, float y, float z);
	bool PlaceOnGround();
	bool IsStanding() const { return m_isStanding; }

	/* Disable ped capsule contacts (e.g. while driving so Cheetah doesn't hit Tommy). */
	void SetCollisionEnabled(bool enabled);
	bool IsCollisionEnabled() const { return m_collisionEnabled; }

private:
	struct SkinnedVertex {
		float x, y, z;
		float u, v;
		uint8_t boneIndices[4];
		float boneWeights[4];
	};

	struct SkinnedMeshPart {
		ID3D11Buffer* vb;
		ID3D11Buffer* ib;
		ID3D11ShaderResourceView* texture;
		ID3D11Buffer* objectCB;
		unsigned int indexCount;
		D3D_PRIMITIVE_TOPOLOGY topology;
		bool hasAlpha;
	};

	struct BoneRest {
		XMFLOAT3 pos;
		XMFLOAT4 quat;
		bool valid;
	};

	struct LoadedTex {
		char name[24];
		uint8_t* data;
		size_t size;
		uint32_t width;
		uint32_t height;
		uint32_t dxt;
		uint32_t depth;
		bool isAlpha;
	};

	bool LoadTextures(IMG* img);
	bool LoadModel(IMG* img, DXRender* render);
	bool InitPipeline(DXRender* render);
	HRESULT CreateTextureSRV(DXRender* render, LoadedTex& tex, ID3D11ShaderResourceView** outSRV);
	void BindAnims();
	void SampleAnim(IfpAnim* anim, float time);
	void BlendAnimPose(float dt);
	void UpdateBoneMatrices();
	void SetAnim(IfpAnim* anim);
	int FindTexIndex(const char* name) const;

	void CreateCharacterController();
	void DestroyCharacterController();
	void SyncFromBullet();
	void WarpCharacter(float x, float y, float z);

	float m_posX, m_posY, m_posZ;
	float m_velX, m_velY, m_velZ;
	float m_heading;
	float m_moveSpeed;
	bool m_isStanding;
	bool m_wasStanding;
	bool m_collisionEnabled;
	CollisionWorld* m_world;

	btPairCachingGhostObject* m_ghost;
	btCapsuleShape* m_capsule;
	btKinematicCharacterController* m_character;

	IFP* m_ifp;
	IfpAnim* m_animIdle;
	IfpAnim* m_animWalk;
	IfpAnim* m_animRun;
	IfpAnim* m_animSprint;
	IfpAnim* m_animJumpLaunch;
	IfpAnim* m_animJumpGlide;
	IfpAnim* m_animJumpLand;
	IfpAnim* m_currentAnim;
	float m_animTime;
	float m_animBlend; /* 0 = blend-from pose, 1 = fully current anim */
	static constexpr float ANIM_BLEND_DURATION = 0.22f;
	bool m_wasMoving;
	bool m_isJumping;
	float m_landAnimTimer;

	uint32_t m_boneCount;
	std::vector<int32_t> m_boneIds;
	std::vector<uint32_t> m_boneFlags;
	std::vector<BoneRest> m_boneRest;
	std::vector<XMFLOAT4X4> m_inverseBind;
	std::vector<XMFLOAT4> m_localQuat;
	std::vector<XMFLOAT3> m_localPos;
	std::vector<XMFLOAT4> m_blendFromQuat;
	std::vector<XMFLOAT3> m_blendFromPos;
	std::vector<XMFLOAT4X4> m_boneWorld;
	std::vector<XMFLOAT4X4> m_skinPalette;
	std::vector<IfpSequence*> m_boneSeq;

	std::vector<SkinnedMeshPart> m_meshes;
	std::vector<LoadedTex> m_textures;

	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_ps;
	ID3D11PixelShader* m_shadowPS;
	ID3D11InputLayout* m_layout;
	ID3D11SamplerState* m_sampler;
	ID3D11Buffer* m_boneCB;

	static const int MAX_BONES = 64;
};
