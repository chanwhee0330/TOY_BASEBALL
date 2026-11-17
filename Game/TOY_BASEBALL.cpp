#include <Windows.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
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

#include "Ball.h"
#include "Batter.h"
#include "Mesh.h"
#include "Pitcher.h"
#include "PitchingMachine.h"

//--- 셰이더, 출력 함수
std::string filetobuf(const std::filesystem::path& filePath);
GLuint make_shader(GLenum type, const std::filesystem::path& filePath);
GLuint make_shaderProgram(const std::filesystem::path& programDirectory);
GLvoid drawScene();
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

GLint width = 1200;
GLint height = 800;
GLuint shaderProgramID = 0;
Mesh stadium;
Pitcher pitcher;
PitchingMachine pitchingMachine;
Batter batter;
Ball ball;

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
	const auto modelDirectory = programDirectory / "Assets" / "Models";
	if (shaderProgramID == 0
		|| !InitBuffer(modelDirectory / "stadium.obj", stadium)
		|| !pitcher.Init(modelDirectory)
		|| !pitchingMachine.Init(modelDirectory)
		|| !batter.Init(modelDirectory)
		|| !ball.Init(modelDirectory))
	{
		DeleteBuffer(stadium);
		pitcher.Delete();
		pitchingMachine.Delete();
		batter.Delete();
		ball.Delete();
		if (shaderProgramID != 0) glDeleteProgram(shaderProgramID);
		glfwDestroyWindow(window);
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

	// 지금은 1P(W·A·S·D)가 투구한다. 공수 교대를 만들면 수비하는 쪽 키로 SetKeys를 바꾼다.
	pitchingMachine.SetKeys(PITCH_KEYS_1P);
	std::cout << "Pitching (1P): A/D = aim left/right, hold and release W = fastball, S = breaking ball\n";

	//--- 메인 루프
	double lastTime = glfwGetTime();
	while (glfwWindowShouldClose(window) == GLFW_FALSE)
	{
		// 프레임 사이 시간. 창을 끌 때처럼 오래 멈췄다 돌아와도 공이 순간이동하지 않게 줄인다.
		const double now = glfwGetTime();
		const float deltaTime = static_cast<float>((std::min)(now - lastTime, 0.05)); // Windows.h의 min 매크로를 피하려고 괄호로 감싼다
		lastTime = now;

		pitchingMachine.Update(window, deltaTime, ball);
		ball.Update(deltaTime);
		drawScene();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	DeleteBuffer(stadium);
	pitcher.Delete();
	pitchingMachine.Delete();
	batter.Delete();
	ball.Delete();
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

GLvoid drawScene()
{
	GLFWwindow* window = glfwGetCurrentContext();
	glfwGetFramebufferSize(window, &width, &height);
	if (width == 0 || height == 0) return;
	glViewport(0, 0, width, height);
	glClearColor(0.10f, 0.13f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shaderProgramID);

	// 홈플레이트 바로 뒤 낮은 곳에서 2루 쪽을 내려다본다.
	// 타자와 홈이 화면 아래에 크게, 1·3루가 화면 가운데쯤, 외야 펜스가 화면 위 끝에 온다.
	const glm::mat4 view = glm::lookAt(glm::vec3(0, 1.5, 2), glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0, 1, 0));
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	const float baseFov = glm::radians(54.0f);
	// 기본 창(3:2)보다 좁아지면 세로 시야를 넓혀 1·3루가 화면 밖으로 나가지 않게 한다.
	const float minAspect = 1.5f;
	const float fov = aspect < minAspect ? 2.0f * std::atan(std::tan(baseFov / 2) * minAspect / aspect) : baseFov;
	const glm::mat4 projection = glm::perspective(fov, aspect, 1.0f, 200.0f);
	const glm::mat4 viewProjection = projection * view;

	GLint location = glGetUniformLocation(shaderProgramID, "lightPos");
	glUniform3f(location, 0.0f, 55.0f, 18.0f);
	location = glGetUniformLocation(shaderProgramID, "lightColor");
	glUniform3f(location, 1.0f, 1.0f, 1.0f);

	drawMesh(shaderProgramID, stadium, glm::mat4(1.0f), viewProjection);
	pitcher.Draw(shaderProgramID, viewProjection);
	pitchingMachine.Draw(shaderProgramID, viewProjection);
	// 왼쪽만 누르면 -1, 오른쪽만 누르면 1, 나머지는 0이다.
	const int stance = glfwGetKey(window, GLFW_KEY_RIGHT) - glfwGetKey(window, GLFW_KEY_LEFT);
	batter.Draw(shaderProgramID, viewProjection, glfwGetTime(), stance);
	ball.Draw(shaderProgramID, viewProjection);

	glBindVertexArray(0);
}

void keyCallback(GLFWwindow* window, int key, int, int action, int)
{
	if (action != GLFW_PRESS) return;

	if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE)
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
	else if (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN)
	{
		batter.StartSwing(glfwGetTime(), key == GLFW_KEY_DOWN);
	}
}
