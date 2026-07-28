#include "Player.h"
#include "CollisionWorld.h"

#include <stdio.h>

#define __BT_DISABLE_SSE__
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"

namespace {

/* Capsule total height ≈ 2 * PED_FEET_OFFSET so origin matches re3 feet offset. */
const float kPedCapsuleRadius = PED_SPHERE_RADIUS;
const float kPedCapsuleCylinder = (PED_FEET_OFFSET * 2.0f) - (kPedCapsuleRadius * 2.0f);

} // namespace

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
