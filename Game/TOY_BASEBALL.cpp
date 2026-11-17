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
GLvoid drawScene(double now, int stance);
bool initShadowMap();
void drawShadowCasters(const glm::mat4& viewProjection, double now, int stance);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

GLint width = 1200;
GLint height = 800;
GLuint shaderProgramID = 0;
Mesh stadium;
Pitcher pitcher;
PitchingMachine pitchingMachine;
Batter batter;
Ball ball;

constexpr GLsizei SHADOW_MAP_SIZE = 2048;
GLuint shadowFramebuffer = 0;
GLuint shadowDepthTexture = 0;
// 내야와 외야 양쪽 높은 곳에서 경기장을 비춘다. 첫 조명이 주 그림자를 만든다.
constexpr GLfloat lightPositions[] = {
	-8.0f, 21.0f, 6.0f,
	13.0f, 19.0f, 5.0f,
	-17.0f, 22.0f, -23.0f,
	17.0f, 22.0f, -23.0f
};

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
	if (!initShadowMap())
	{
		std::cerr << "Could not initialize the stadium shadow map.\n";
	}

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
		// 고속 직구가 한 프레임 사이에 타격 영역을 건너뛰지 않도록 나눠 검사한다.
		const int steps = static_cast<int>(std::ceil(deltaTime * 240.0f));
		const int stance = glfwGetKey(window, GLFW_KEY_RIGHT) - glfwGetKey(window, GLFW_KEY_LEFT);
		if (steps > 0)
		{
			const float stepTime = deltaTime / steps;
			for (int step = 1; step <= steps; ++step)
			{
				ball.Update(stepTime);
				batter.CheckContact(now - (steps - step) * stepTime, stepTime, stance, ball);
			}
		}
		drawScene(now, stance);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	DeleteBuffer(stadium);
	pitcher.Delete();
	pitchingMachine.Delete();
	batter.Delete();
	ball.Delete();
	glDeleteFramebuffers(1, &shadowFramebuffer);
	glDeleteTextures(1, &shadowDepthTexture);
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

bool initShadowMap()
{
	glGenTextures(1, &shadowDepthTexture);
	glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
		0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &shadowFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (!complete)
	{
		glDeleteFramebuffers(1, &shadowFramebuffer);
		glDeleteTextures(1, &shadowDepthTexture);
		shadowFramebuffer = 0;
		shadowDepthTexture = 0;
	}
	return complete;
}

void drawShadowCasters(const glm::mat4& viewProjection, double now, int stance)
{
	drawMesh(shaderProgramID, stadium, glm::mat4(1.0f), viewProjection);
	pitcher.Draw(shaderProgramID, viewProjection);
	pitchingMachine.Draw(shaderProgramID, viewProjection);
	batter.Draw(shaderProgramID, viewProjection, now, stance);
}

GLvoid drawScene(double now, int stance)
{
	GLFWwindow* window = glfwGetCurrentContext();
	glfwGetFramebufferSize(window, &width, &height);
	if (width == 0 || height == 0) return;
	glUseProgram(shaderProgramID);

	// 주 조명에서 보이는 가장 가까운 표면의 깊이를 먼저 기록한다.
	const glm::mat4 lightView = glm::lookAt(glm::make_vec3(lightPositions),
		glm::vec3(0.0f, 0.0f, -12.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 lightProjection = glm::perspective(glm::radians(110.0f), 1.0f, 2.0f, 90.0f);
	const glm::mat4 lightViewProjection = lightProjection * lightView;
	const GLint depthPass = glGetUniformLocation(shaderProgramID, "depthPass");
	if (shadowFramebuffer != 0)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
		glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
		glClear(GL_DEPTH_BUFFER_BIT);
		glUniform1i(depthPass, GL_TRUE);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.5f, 2.0f);
		drawShadowCasters(lightViewProjection, now, stance);
		glDisable(GL_POLYGON_OFFSET_FILL);
		glUniform1i(depthPass, GL_FALSE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glViewport(0, 0, width, height);
	glClearColor(0.10f, 0.13f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
	glUniform1i(glGetUniformLocation(shaderProgramID, "shadowMap"), 0);
	glUniform1i(glGetUniformLocation(shaderProgramID, "useShadowMap"), shadowFramebuffer != 0);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "lightViewProjection"), 1, GL_FALSE,
		glm::value_ptr(lightViewProjection));
	glUniform3fv(glGetUniformLocation(shaderProgramID, "lightPositions[0]"), 4, lightPositions);
	const GLint shadowLocation = glGetUniformLocation(shaderProgramID, "receiveBallShadow");
	glUniform1i(shadowLocation, ball.InFlight());
	if (ball.InFlight())
	{
		glUniform3fv(glGetUniformLocation(shaderProgramID, "ballPosition"), 1, glm::value_ptr(ball.Position()));
		glUniform1f(glGetUniformLocation(shaderProgramID, "ballRadius"), Ball::Radius());
	}
	drawMesh(shaderProgramID, stadium, glm::mat4(1.0f), viewProjection);
	glUniform1i(shadowLocation, GL_FALSE);
	pitcher.Draw(shaderProgramID, viewProjection);
	pitchingMachine.Draw(shaderProgramID, viewProjection);
	batter.Draw(shaderProgramID, viewProjection, now, stance);
	ball.Draw(shaderProgramID, viewProjection);

	glBindTexture(GL_TEXTURE_2D, 0);
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
