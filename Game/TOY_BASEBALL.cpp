#include <Windows.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "Model.h"

//--- 셰이더, 버퍼, 출력 함수
std::string filetobuf(const std::filesystem::path& filePath);
GLuint make_shader(GLenum type, const std::filesystem::path& filePath);
GLuint make_shaderProgram(const std::filesystem::path& programDirectory);
bool InitBuffer(const std::filesystem::path& modelPath);
GLvoid drawScene();
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

GLint width = 1200;
GLint height = 800;
GLuint shaderProgramID = 0;
GLuint VAO = 0;
GLuint VBO = 0;
GLsizei vertexCount = 0;

int main()
{
	// 실행 파일 옆에 복사된 셰이더와 모델을 읽는다.
	wchar_t executablePath[32768];
	const DWORD pathLength = GetModuleFileNameW(nullptr, executablePath, static_cast<DWORD>(std::size(executablePath)));
	if (pathLength == 0 || pathLength == std::size(executablePath))
	{
		std::cerr << "Could not locate the executable directory.\n";
		return EXIT_FAILURE;
	}
	const auto programDirectory = std::filesystem::path(executablePath).parent_path();

	//--- GLFW 초기화 및 OpenGL 버전 설정
	if (glfwInit() != GLFW_TRUE)
	{
		std::cerr << "GLFW initialization failed.\n";
		return EXIT_FAILURE;
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);

	GLFWwindow* window = glfwCreateWindow(width, height, "TOY BASEBALL", nullptr, nullptr);
	if (window == nullptr)
	{
		std::cerr << "Could not open an OpenGL 3.3 window.\n";
		glfwTerminate();
		return EXIT_FAILURE;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetKeyCallback(window, keyCallback);

	//--- GLEW 초기화
	glewExperimental = GL_TRUE;
	const GLenum glewResult = glewInit();
	if (glewResult != GLEW_OK)
	{
		std::cerr << "GLEW initialization failed: " << glewGetErrorString(glewResult) << '\n';
		glfwDestroyWindow(window);
		glfwTerminate();
		return EXIT_FAILURE;
	}

	shaderProgramID = make_shaderProgram(programDirectory);
	if (shaderProgramID == 0 || !InitBuffer(programDirectory / "Assets" / "Models" / "stadium.obj"))
	{
		if (shaderProgramID != 0) glDeleteProgram(shaderProgramID);
		glfwDestroyWindow(window);
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

	//--- 메인 루프
	while (glfwWindowShouldClose(window) == GLFW_FALSE)
	{
		drawScene();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &VAO);
	glDeleteProgram(shaderProgramID);
	glfwDestroyWindow(window);
	glfwTerminate();
	return EXIT_SUCCESS;
}

std::string filetobuf(const std::filesystem::path& filePath)
{
	std::ifstream file(filePath);
	if (!file)
	{
		std::wcerr << L"Shader file not found: " << filePath.c_str() << L'\n';
		return {};
	}

	std::ostringstream contents;
	contents << file.rdbuf();
	return contents.str();
}

GLuint make_shader(GLenum type, const std::filesystem::path& filePath)
{
	const std::string source = filetobuf(filePath);
	if (source.empty()) return 0;

	GLuint shader = glCreateShader(type);
	const char* sourceText = source.c_str();
	glShaderSource(shader, 1, &sourceText, nullptr);
	glCompileShader(shader);

	GLint result = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE)
	{
		char errorLog[1024]{};
		glGetShaderInfoLog(shader, sizeof(errorLog), nullptr, errorLog);
		std::wcerr << filePath.c_str() << L'\n';
		std::cerr << "Shader compilation failed: " << errorLog << '\n';
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint make_shaderProgram(const std::filesystem::path& programDirectory)
{
	const GLuint vertexShader = make_shader(GL_VERTEX_SHADER, programDirectory / "vertex.glsl");
	if (vertexShader == 0) return 0;

	const GLuint fragmentShader = make_shader(GL_FRAGMENT_SHADER, programDirectory / "fragment.glsl");
	if (fragmentShader == 0)
	{
		glDeleteShader(vertexShader);
		return 0;
	}

	const GLuint shaderID = glCreateProgram();
	glAttachShader(shaderID, vertexShader);
	glAttachShader(shaderID, fragmentShader);
	glLinkProgram(shaderID);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	GLint result = GL_FALSE;
	glGetProgramiv(shaderID, GL_LINK_STATUS, &result);
	if (result == GL_FALSE)
	{
		char errorLog[1024]{};
		glGetProgramInfoLog(shaderID, sizeof(errorLog), nullptr, errorLog);
		std::cerr << "Shader linking failed: " << errorLog << '\n';
		glDeleteProgram(shaderID);
		return 0;
	}
	return shaderID;
}

bool InitBuffer(const std::filesystem::path& modelPath)
{
	std::vector<Vertex> vertices;
	if (!read_obj_file(modelPath, vertices)) return false;
	vertexCount = static_cast<GLsizei>(vertices.size());

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

	//--- 정점 위치, 재질색, 법선, 발광색을 vertex.glsl에 전달한다.
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, emission)));
	glEnableVertexAttribArray(3);
	glBindVertexArray(0);

	std::cout << "Stadium loaded: " << vertexCount / 3 << " triangles\n";
	return true;
}

GLvoid drawScene()
{
	glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
	if (width == 0 || height == 0) return;
	glViewport(0, 0, width, height);
	glClearColor(0.10f, 0.13f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shaderProgramID);
	glBindVertexArray(VAO);

	const glm::mat4 model(1.0f);
	// 홈플레이트 바로 뒤 낮은 곳에서 2루 쪽을 내려다본다.
	// 타자와 홈이 화면 아래에 크게, 1·3루가 화면 가운데쯤, 외야 펜스가 화면 위 끝에 온다.
	const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 5.5f, 4.0f), glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0, 1, 0));
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	const float baseFov = glm::radians(54.0f);
	// 기본 창(3:2)보다 좁아지면 세로 시야를 넓혀 1·3루가 화면 밖으로 나가지 않게 한다.
	const float minAspect = 1.5f;
	const float fov = aspect < minAspect ? 2.0f * std::atan(std::tan(baseFov / 2) * minAspect / aspect) : baseFov;
	const glm::mat4 projection = glm::perspective(fov, aspect, 1.0f, 200.0f);
	const glm::mat4 transform = projection * view * model;

	GLint location = glGetUniformLocation(shaderProgramID, "modelTransform");
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(transform));
	location = glGetUniformLocation(shaderProgramID, "model");
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(model));
	location = glGetUniformLocation(shaderProgramID, "lightPos");
	glUniform3f(location, 0.0f, 55.0f, 18.0f);
	location = glGetUniformLocation(shaderProgramID, "lightColor");
	glUniform3f(location, 1.0f, 1.0f, 1.0f);
	glDrawArrays(GL_TRIANGLES, 0, vertexCount);

	glBindVertexArray(0);
}

void keyCallback(GLFWwindow* window, int key, int, int action, int)
{
	if (action == GLFW_PRESS && (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE))
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}
