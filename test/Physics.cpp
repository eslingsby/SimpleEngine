#include "Physics.hpp"

#include "Collider.hpp"
#include "Transform.hpp"

#include <BulletCollision\NarrowPhaseCollision\btRaycastCallback.h>

void Physics::addRigidBody(uint64_t id, float mass, btCollisionShape* shape){
	if (!_dynamicsWorld)
		return;

	Transform& transform = *_engine.entities.add<Transform>(id);
	Collider& collider = *_engine.entities.add<Collider>(id);

	if (collider._collisionShape || collider._rigidBody)
		return;
	
	collider._collisionShape = shape;

	btVector3 localInertia(0, 0, 0);

	if (mass != 0.f)
		collider._collisionShape->calculateLocalInertia(static_cast<btScalar>(mass), localInertia);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(static_cast<btScalar>(mass), &transform, collider._collisionShape, localInertia);
	collider._rigidBody = new btRigidBody(rbInfo);

	collider._rigidBody->setUserPointer(&transform);

	_dynamicsWorld->addRigidBody(collider._rigidBody);
}

Physics::Physics(Engine & engine) : _engine(engine){
	_engine.events.subscribe(this, Events::Load, &Physics::load);
	_engine.events.subscribe(this, Events::Update, &Physics::update);
}

Physics::~Physics(){
	delete _dynamicsWorld;
	delete _solver;
	delete _overlappingPairCache;
	delete _dispatcher;
	delete _collisionConfiguration;
}

void Physics::load(int argc, char ** argv){
	_collisionConfiguration = new btDefaultCollisionConfiguration();

	_dispatcher = new btCollisionDispatcher(_collisionConfiguration);

	_overlappingPairCache = new btDbvtBroadphase();

	_solver = new btSequentialImpulseConstraintSolver;

	_dynamicsWorld = new btDiscreteDynamicsWorld(_dispatcher, _overlappingPairCache, _solver, _collisionConfiguration);

	setGravity({ 0.f, 0.f, 0.f });
}

void Physics::update(double dt){
	for (uint32_t i = 0; i < DEFUALT_PHYSICS_STEPS; i++)
		_dynamicsWorld->stepSimulation(static_cast<float>(dt) / DEFUALT_PHYSICS_STEPS, 0);
}

void Physics::setGravity(const glm::dvec3& direction){
	if (!_dynamicsWorld)
		return;

	_dynamicsWorld->setGravity(toBt(direction));
}

void Physics::addSphere(uint64_t id, float radius, float mass) {
	if (_engine.entities.has<Collider>(id))
		_engine.entities.remove<Collider>(id);

	addRigidBody(id, mass, new btSphereShape(static_cast<btScalar>(radius)));
}

void Physics::addBox(uint64_t id, const glm::dvec3& dimensions, float mass) {
	if (_engine.entities.has<Collider>(id))
		_engine.entities.remove<Collider>(id);

	addRigidBody(id, mass, new btBoxShape(btVector3(dimensions.x, dimensions.y, dimensions.z) * 2));
}

void Physics::addCylinder(uint64_t id, float radius, float height, float mass) {
	if (_engine.entities.has<Collider>(id))
		_engine.entities.remove<Collider>(id);

	addRigidBody(id, mass, new btCylinderShape(btVector3(radius * 2, radius * 2, height)));
}

void Physics::addCapsule(uint64_t id, float radius, float height, float mass) {
	if (_engine.entities.has<Collider>(id))
		_engine.entities.remove<Collider>(id);

	addRigidBody(id, mass, new btCapsuleShape(radius, height));
}

void Physics::addStaticPlane(uint64_t id){
	if (_engine.entities.has<Collider>(id))
		_engine.entities.remove<Collider>(id);

	addRigidBody(id, 0.f, new btStaticPlaneShape(btVector3(0, 0, 1), 0));
}

void Physics::rayTest(const glm::dvec3 & from, const glm::dvec3 & to, std::vector<RayHit>& hits){
	btVector3 bFrom(toBt(from));
	btVector3 bTo(toBt(to));

	btCollisionWorld::AllHitsRayResultCallback results(bFrom, bTo);

	results.m_flags |= btTriangleRaycastCallback::kF_KeepUnflippedNormal;
	results.m_flags |= btTriangleRaycastCallback::kF_UseSubSimplexConvexCastRaytest;
	results.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;

	_dynamicsWorld->rayTest(bFrom, bTo, results);

	hits.reserve(results.m_collisionObjects.size());

	for (uint32_t i = 0; i < results.m_collisionObjects.size(); i++) {
		uint64_t id = static_cast<Transform*>(results.m_collisionObjects[i]->getUserPointer())->id();

		btVector3 position = results.m_hitPointWorld[i];
		btVector3 normal = results.m_hitNormalWorld[i];

		hits.push_back({ id,{ position.x(), position.y(), position.z() },{ normal.x(), normal.y(), normal.z() } });
	}
}

Physics::RayHit Physics::rayTest(const glm::dvec3 & from, const glm::dvec3 & to){
	btVector3 bFrom(toBt(from));
	btVector3 bTo(toBt(to));

	btCollisionWorld::ClosestRayResultCallback results(bFrom, bTo);

	results.m_flags |= btTriangleRaycastCallback::kF_KeepUnflippedNormal;
	results.m_flags |= btTriangleRaycastCallback::kF_UseSubSimplexConvexCastRaytest;

	_dynamicsWorld->rayTest(bFrom, bTo, results);

	if (!results.hasHit())
		return RayHit();

	uint64_t id = static_cast<Transform*>(results.m_collisionObject->getUserPointer())->id();

	btVector3 position = results.m_hitPointWorld;
	btVector3 normal = results.m_hitNormalWorld;

	return { id, { position.x(), position.y(), position.z() }, { normal.x(), normal.y(), normal.z() } };
}

void Physics::sphereTest(float radius, const glm::dvec3 & position, const glm::dquat & rotation, std::vector<SweepHit>& hits){


	//btCollisionWorld::ClosestConvexResultCallback results(0, )
}

void Physics::sphereSweep(uint64_t id, float radius, const glm::dvec3 & fromPos, const glm::dquat & fromRot, const glm::dvec3 & toPos, const glm::dvec3 & toRot, std::vector<SweepHit>& hits){

}
