#pragma once

#include "Config.hpp"

#include <glm\vec2.hpp>
#include <glm\vec3.hpp>

class Controller {
	Engine& _engine;

	uint64_t _possessed = 0;
	uint64_t _cursor = 0;

	bool _forward = false;
	bool _back = false;
	bool _left = false;
	bool _right = false;

	bool _up = false;
	bool _down = false;

	bool _boost = false;

	bool _action0 = false;

	Vec2 _mousePos;
	Vec2 _dMousePos;

	bool _locked = true;
	
	Vec3 _cursorPosition;
	double _cursorI = 180.0;

public:
	Controller(Engine& engine);

	void load(int argc, char** argv);
	void update(double dt);
	void mousemove(double x, double y);
	void mousepress(int button, int action, int mods);
	void keypress(int key, int scancode, int action, int mods);
	void reset();

	void setPossessed(uint64_t id);
	void setCursor(uint64_t id);

	friend class EventManager;
};