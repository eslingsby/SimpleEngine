#include "Controller.hpp"

#include "Renderer.hpp"
#include "Physics.hpp"

#include "Transform.hpp"
#include "Collider.hpp"

uint64_t _test = 0;

Controller::Controller(Engine& engine) : _engine(engine) {
	_engine.events.subscribe(this, Events::Update, &Controller::update);
	_engine.events.subscribe(this, Events::Load, &Controller::load);
	_engine.events.subscribe(this, Events::Reset, &Controller::reset);

	_engine.events.subscribe(this, Events::Mousemove, &Controller::mousemove);
	_engine.events.subscribe(this, Events::Mousepress, &Controller::mousepress);
	_engine.events.subscribe(this, Events::Keypress, &Controller::keypress);
}

void Controller::load(int argc, char ** argv){
	_cursor = _engine.entities.create();
	_engine.entities.add<Transform>(_cursor);

	_engine.system<Renderer>().addShader(_cursor, "vertexShader.glsl", "fragmentShader.glsl");
	_engine.system<Renderer>().addMesh(_cursor, "arrow.obj");
	_engine.system<Renderer>().addTexture(_cursor, "arrow.png");

	_engine.entities.reference(_cursor);
}

void Controller::update(double dt) {
	if (!_possessed || !_engine.entities.has<Transform>(_possessed))
		return;

	Transform& transform = *_engine.entities.get<Transform>(_possessed);

	if (_locked) {
		transform.globalRotate(glm::dquat({ 0.0, 0.0, -_dMousePos.x * dt }));
		transform.rotate(glm::dquat({ -_dMousePos.y * dt, 0.0, 0.0 }));

		_dMousePos = { 0.0, 0.0 };

		double moveSpeed;

		if (_boost)
			moveSpeed = 200.0 * dt;
		else
			moveSpeed = 100.0 * dt;

		if (_forward)
			transform.translate(LocalDVec3::forward * moveSpeed);
		if (_back)
			transform.translate(LocalDVec3::back * moveSpeed);
		if (_left)
			transform.translate(LocalDVec3::left * moveSpeed);
		if (_right)
			transform.translate(LocalDVec3::right * moveSpeed);
		if (_up)
			transform.globalTranslate(GlobalDVec3::up * moveSpeed);
		if (_down)
			transform.globalTranslate(GlobalDVec3::down * moveSpeed);
	}

	Collider* collider = _engine.entities.get<Collider>(_possessed);

	if (collider) {
		glm::dvec3 angles = glm::eulerAngles(transform.rotation());
		transform.setRotation(glm::dquat({ angles.x, 0.f, angles.z }));

		collider->setAngularVelocity({ 0, 0, 0 });
		collider->setLinearVelocity({ 0, 0, 0 });

		collider->activate();
	}

	if (_action0 && _cursor && _engine.entities.has<Transform>(_cursor)) {
		Renderer& renderer = _engine.system<Renderer>();

		glm::dvec2 mousePos = ((_mousePos / static_cast<glm::dvec2>(renderer.windowSize())) * 2.0) - 1.0;

		glm::dvec4 cursor = glm::inverse(renderer.projectionMatrix() * renderer.viewMatrix()) * glm::dvec4(mousePos.x, -mousePos.y, 0.0, 1.0);

		Transform& cursorTransform = *_engine.entities.get<Transform>(_cursor);

		glm::dvec3 cursorPosition = glm::dvec3(cursor.x / cursor.w, cursor.y / cursor.w, cursor.z / cursor.w);
		glm::dvec3 possessedPosition = _engine.entities.get<Transform>(_possessed)->position();

		glm::dvec3 target = possessedPosition + glm::normalize(cursorPosition - possessedPosition) * 1000.0;

		auto hit = _engine.system<Physics>().rayTest(possessedPosition, target);

		if (hit.id)
			cursorTransform.setPosition(hit.position);
		else
			cursorTransform.setPosition(target);
	}
}

void Controller::mousemove(double x, double y){
	glm::dvec2 newCursor = { x, y };

	if (_mousePos == glm::dvec2(0.0, 0.0))
		_mousePos = newCursor;

	_dMousePos = newCursor - _mousePos;
	_mousePos = newCursor;
}

void Controller::mousepress(int button, int action, int mods){
	if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS)
		_action0 = true;
	else if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_RELEASE)
		_action0 = false;
}

void Controller::keypress(int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
		_locked = !_locked;
		_engine.system<Renderer>().lockCursor(_locked);
		return;
	}

	if (!_possessed)
		return;

	bool value;

	if (action == GLFW_PRESS)
		value = true;
	else if (action == GLFW_RELEASE)
		value = false;
	else
		return;

	if (key == GLFW_KEY_W)
		_forward = value;
	if (key == GLFW_KEY_S)
		_back = value;
	if (key == GLFW_KEY_A)
		_left = value;
	if (key == GLFW_KEY_D)
		_right = value;
	if (key == GLFW_KEY_SPACE)
		_up = value;
	if (key == GLFW_KEY_LEFT_CONTROL)
		_down = value;
	if (key == GLFW_KEY_LEFT_SHIFT)
		_boost = value;
}

void Controller::reset(){
	if (_possessed)
		_engine.entities.dereference(_possessed);

	_possessed = 0;
}

void Controller::setPossessed(uint64_t id) {
	if (id && !_engine.entities.valid(id))
		return;

	if (_possessed)
		_engine.entities.dereference(id);

	if (id)
		_engine.entities.reference(id);
	
	_possessed = id;
}
