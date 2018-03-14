#include "Renderer.hpp"

#include "Transform.hpp"
#include "Model.hpp"

#include <iostream>
#include <fstream>
#include <string>

#include <tiny_obj_loader.h>
#include <stb_image.h>

struct Attributes {
	glm::tvec3<GLfloat> vertex;
	glm::tvec3<GLfloat> normal;
	glm::tvec2<GLfloat> texcoord;
};

int verboseCheckError() {
	GLenum error = glGetError();

	if (error == GL_NO_ERROR)
		return 0;

	std::cerr << error << std::endl;

	return 1;
}

#define glCheckError() assert(verboseCheckError() == 0)

void errorCallback(int error, const char* description) {
	std::cerr << "GLFW Error - " << error << " - " << description << std::endl;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);

	Renderer& renderer = *(Renderer*)glfwGetWindowUserPointer(window);

	renderer._engine.events.dispatch(Events::Keypress, key, scancode, action, mods);
}

void windowSizeCallback(GLFWwindow* window, int height, int width) {
	Renderer& renderer = *(Renderer*)glfwGetWindowUserPointer(window);

	renderer._reshape(height, width);
}

bool compileShader(GLuint type, GLuint* shader, const std::string& src) {
	*shader = glCreateShader(type);

	const GLchar* srcPtr = static_cast<const GLchar*>(src.c_str());
	glShaderSource(*shader, 1, &srcPtr, 0);

	glCompileShader(*shader);

	GLint compiled;
	glGetShaderiv(*shader, GL_COMPILE_STATUS, &compiled);

	if (compiled == GL_TRUE)
		return true;

	GLint length = 0;
	glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &length);

	std::vector<GLchar> message(length);
	glGetShaderInfoLog(*shader, length, &length, &message[0]);

	std::cerr << "GLSL error - " << static_cast<char*>(&message[0]) << std::endl;
	return false;
}

bool createProgram(GLuint* program, GLuint* vertexShader, GLuint* fragmentShader, const std::string& vertexSrc, const std::string& fragmentSrc) {
	// create shaders
	bool failed = false;

	if (!compileShader(GL_VERTEX_SHADER, vertexShader, vertexSrc))
		failed = true;

	if (!compileShader(GL_FRAGMENT_SHADER, fragmentShader, fragmentSrc))
		failed = true;

	if (failed) {
		glDeleteShader(*vertexShader);
		glDeleteShader(*fragmentShader);

		return false;
	}

	glCheckError();

	*program = glCreateProgram();
	glAttachShader(*program, *vertexShader);
	glAttachShader(*program, *fragmentShader);

	glLinkProgram(*program);

	// link the program
	GLint success;
	glGetProgramiv(*program, GL_LINK_STATUS, &success);

	if (!success) {
		GLint length = 0;
		glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &length);

		std::vector<GLchar> message(length);
		glGetProgramInfoLog(*program, length, &length, &message[0]);

		std::cerr << "GLSL error - " << static_cast<char*>(&message[0]) << std::endl;

		glDeleteShader(*vertexShader);
		glDeleteShader(*fragmentShader);
		glDeleteProgram(*program);

		return false;
	}

	return true;
}

void Renderer::_reshape(int height, int width) {
	_windowSize = { height, width };
	_viewMatrix = glm::perspectiveFov(100.f, static_cast<float>(height), static_cast<float>(width), 0.1f, 1000.f);

	glViewport(0, 0, height, width);
}

Renderer::Renderer(Engine& engine) : _engine(engine) {
	_engine.events.subscribe(this, Events::Load, &Renderer::load);
	_engine.events.subscribe(this, Events::Update, &Renderer::update);
}

Renderer::~Renderer() {
	if (_window)
		glfwDestroyWindow(_window);

	glfwTerminate();
}

void Renderer::load(int argc, char** argv) {
	// setup data stuff
	_path = upperPath(replace('\\', '/', argv[0])) + DATA_FOLDER + '/';
	_windowSize = { 512, 512 };

	stbi_set_flip_vertically_on_load(true);

	// setup GLFW
	glfwSetErrorCallback(errorCallback);

	if (!glfwInit()) {
		_engine.events.unsubscribe(this);
		return;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

	glfwWindowHint(GLFW_REFRESH_RATE, 1);

	_window = glfwCreateWindow(_windowSize.x, _windowSize.y, "", nullptr, nullptr);

	if (!_window) {
		std::cerr << "GLFW error - " << "cannot create window" << std::endl;
		_engine.events.unsubscribe(this);
		return;
	}

	glfwSetWindowUserPointer(_window, this);

	glfwSetKeyCallback(_window, keyCallback);
	glfwSetWindowSizeCallback(_window, windowSizeCallback);

	glfwMakeContextCurrent(_window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	glfwSwapInterval(1); // v-sync

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	_reshape(_windowSize.x, _windowSize.y);

	glCheckError();
}

void Renderer::update(double dt) {
	glfwPollEvents();

	if (glfwWindowShouldClose(_window)) {
		_engine.running = false;
		return;
	}

	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	_engine.entities.iterate<Transform, Model>([&](uint64_t id, Transform& transform, Model& model) {
		if (!model.hasShader || (!model.texture && !model.arrayObject))
			return;

		Shader& shader = _shaders[model.shader];

		// setup shader program
		glUseProgram(shader.program);

		if (shader.uniformTexture != -1)
			glUniform1i(shader.uniformTexture, 0);

		// projection matrix
		if (shader.uniformProjection != -1)
			glUniformMatrix4fv(shader.uniformProjection, 1, GL_FALSE, &(_viewMatrix)[0][0]);
		
		// view matrix
		if (shader.uniformView != -1 && _camera) {
			Transform& cameraTransform = *_engine.entities.get<Transform>(_camera);
		
			glm::mat4 matrix;
			matrix = glm::translate(matrix, -cameraTransform.position);
			matrix = glm::scale(matrix, cameraTransform.scale);
			matrix *= glm::mat4_cast(cameraTransform.rotation);
		
			glUniformMatrix4fv(shader.uniformView, 1, GL_FALSE, &(matrix)[0][0]);
		}
		
		// model matrix
		if (shader.uniformModel != -1) {
			glm::mat4 matrix;
			matrix = glm::translate(matrix, -transform.position);
			matrix = glm::scale(matrix, transform.scale);
			matrix *= glm::mat4_cast(transform.rotation);
		
			glUniformMatrix4fv(shader.uniformModel, 1, GL_FALSE, &(matrix)[0][0]);
		}

		// set texture
		if (shader.uniformTexture != -1 && model.texture) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, model.texture);
		}

		// draw buffer
		if (model.arrayObject && model.attribBuffer && model.indexCount) {
			glBindVertexArray(model.arrayObject);
			glBindBuffer(GL_ARRAY_BUFFER, model.attribBuffer);

			glDrawArrays(GL_TRIANGLES, 0, model.indexCount);
		}

		// clean up
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		glCheckError();
	});

	glUseProgram(0);
	glCheckError();

	glfwSwapBuffers(_window);
}

bool Renderer::loadMesh(uint64_t* id, const std::string& meshFile) {
	if (!_engine.entities.valid(*id))
		return false;

	_engine.entities.add<Model>(*id);
	Model& model = *_engine.entities.get<Model>(*id);

	// load model data
	tinyobj::attrib_t attributes;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string error;
	tinyobj::LoadObj(&attributes, &shapes, &materials, &error, (_path + meshFile).c_str());

	if (!error.empty()) {
		std::cerr << "cannot load mesh - " << _path + meshFile << std::endl;
		return false;
	}

	if (!attributes.vertices.size()) {
		std::cerr << "problem reading vertex data - " << _path + meshFile << std::endl;
		return false;
	}

	for (const tinyobj::shape_t& shape : shapes)
		model.indexCount += static_cast<GLsizei>(shape.mesh.indices.size());

	// create buffers
	glGenVertexArrays(1, &model.arrayObject);
	glBindVertexArray(model.arrayObject);

	glGenBuffers(1, &model.attribBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, model.attribBuffer);
	glBufferData(GL_ARRAY_BUFFER, model.indexCount * sizeof(Attributes), nullptr, GL_STATIC_DRAW);

	// buffer data
	Attributes* attributeMap = static_cast<Attributes*>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));

	glCheckError();

	size_t depth = 0;

	for (const tinyobj::shape_t& shape : shapes) {
		for (size_t i = 0; i < shape.mesh.indices.size(); i++) {
			const tinyobj::index_t& index = shape.mesh.indices[i];
		
			memcpy(&attributeMap[depth + i].vertex, &attributes.vertices[(index.vertex_index * 3)], sizeof(Attributes::vertex));

			if (attributes.normals.size())
				memcpy(&attributeMap[depth + i].normal, &attributes.normals[(index.normal_index * 3)], sizeof(Attributes::normal));

			if (attributes.texcoords.size())
				memcpy(&attributeMap[depth + i].texcoord, &attributes.texcoords[(index.texcoord_index * 2)], sizeof(Attributes::texcoord));
		}

		depth += shape.mesh.indices.size();
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);

	glCheckError();

	// assign attribute pointers
	glEnableVertexAttribArray(VERTEX_ATTRIBUTE);
	glVertexAttribPointer(VERTEX_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(Attributes), (void*)(0));

	glEnableVertexAttribArray(NORMAL_ATTRIBUTE);
	glVertexAttribPointer(NORMAL_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(Attributes), (void*)(sizeof(Attributes::vertex)));

	glEnableVertexAttribArray(TEXCOORD_ATTRIBUTE);
	glVertexAttribPointer(TEXCOORD_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, sizeof(Attributes), (void*)(sizeof(Attributes::vertex) + sizeof(Attributes::normal)));

	glCheckError();

	// clean up
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glCheckError();

	return true;
}

bool Renderer::loadTexture(uint64_t* id, const std::string& textureFile) {
	if (!_engine.entities.valid(*id))
		return false;

	_engine.entities.add<Model>(*id);
	Model& model = *_engine.entities.get<Model>(*id);

	// load data
	int x, y, n;
	uint8_t* data = stbi_load((_path + textureFile).c_str(), &x, &y, &n, 4);

	if (!data) {
		std::cerr << "cannot load image - " << _path + textureFile << std::endl;
		return false;
	}

	// buffer data
	glGenTextures(1, &model.texture);
	glBindTexture(GL_TEXTURE_2D, model.texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, 0);

	glCheckError();

	stbi_image_free(data);

	return true;
}

bool Renderer::loadShader(uint64_t* id, const std::string& vertexShader, const std::string& fragmentShader) {
	if (!_engine.entities.valid(*id))
		return false;

	_engine.entities.add<Model>(*id);
	Model& model = *_engine.entities.get<Model>(*id);

	Shader shader;

	std::string vertexSrc = readFile(_path + vertexShader);
	std::string fragmentSrc = readFile(_path + fragmentShader);

	if ((vertexSrc == "" || fragmentSrc == "") || !createProgram(&shader.program, &shader.vertexShader, &shader.fragmentShader, vertexSrc, fragmentSrc)) {
		std::cerr << "cannot create shader program" << std::endl;

		if (vertexSrc == "")
			std::cerr << _path << vertexShader << std::endl;

		if (fragmentSrc == "")
			std::cerr << _path << fragmentShader << std::endl;

		return false;
	}

	// get uniform locations
	shader.uniformModel = glGetUniformLocation(shader.program, MODEL_UNIFORM);
	shader.uniformView = glGetUniformLocation(shader.program, VIEW_UNIFORM);
	shader.uniformProjection = glGetUniformLocation(shader.program, PROJECTION_UNIFORM);
	shader.uniformTexture = glGetUniformLocation(shader.program, TEXTURE_UNIFORM);

	glCheckError();

	_shaders.push_back(shader);

	model.hasShader = true;
	model.shader = static_cast<uint32_t>(_shaders.size() - 1);

	return true;
}

void Renderer::setCamera(uint64_t id) {
	if ((id && !_engine.entities.valid(id)) || (id && !_engine.entities.has<Transform>(id)))
		return;

	if (_camera)
		_engine.entities.dereference(id);

	if (id)
		_engine.entities.reference(id);

	_camera = id;
}
