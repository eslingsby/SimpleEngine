#pragma once

#include <btBulletDynamicsCommon.h>

#include "Config.hpp"

#include <glm\vec3.hpp>
#include <glm\gtc\quaternion.hpp>

#include <vector>

class Transform;

class Collider{
	Engine& _engine;
	const uint64_t _id;

	const btRigidBody::btRigidBodyConstructionInfo& _constructionInfo;

	btCollisionShape* const _collisionShape;
	btRigidBody* _rigidBody;

	btCompoundShape* _compoundShape = nullptr;
	bool _rootCompound = false;
	uint32_t _compoundIndex;

	//bool _compoundInheritance = true;

public:
	Collider(Engine::EntityManager& entities, uint64_t id, btCollisionShape* const collisionShape, const btRigidBody::btRigidBodyConstructionInfo& constructionInfo);
	~Collider();

	void setGravity(const glm::dvec3& direction);
	void setFriction(double friction);

	void setAngularVelocity(const glm::dvec3& velocity);
	void setLinearVelocity(const glm::dvec3& velocity);

	void activate();
	void deactivate();

	//void setCompoundInheritance(bool value);

	//void ballConstraint(const glm::dvec3& position);
	//void ballConstraint(const glm::dvec3& position, uint64_t id, const glm::dvec3& otherPos);

	//void hingeConstraint(uint64_t id);
	//void sliderConstraint(uint64_t id);

	friend class Physics;
};