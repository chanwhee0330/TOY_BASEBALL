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
#include <string_view>
#include <vector>

#include "Ball.h"
#include "Batter.h"
#include "Mesh.h"
#include "Pitcher.h"
#include "PitchingMachine.h"
#include "Player.h"

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
const char* const resultModels[] = {
	"result_out.obj", "result_single.obj", "result_double.obj", "result_triple.obj", "result_home_run.obj",
	"result_walk.obj"
};
Mesh scoreboardResults[std::size(resultModels)];
constexpr std::string_view scoreCharacters = "0123456789RHEB-";
Mesh scoreGlyphs[scoreCharacters.size()];
struct TeamScore
{
	int inningRuns[3]{};
	int runs = 0;
	int hits = 0;
	int errors = 0;
	int walks = 0;
};
TeamScore teamScores[2]; // 빨강, 파랑
int halfInning = 0;
double scoreShownAt = -10.0;
double scoreDisplayDuration = 2.0;
constexpr double runScoreDuration = 0.5;
double lastRunAt = -10.0;
constexpr int walkResult = 5;
constexpr int lampOffsets[] = {0, 3, 5};
constexpr int lampCounts[] = {3, 2, 2};
const char* const lampNames[] = {"ball", "strike", "out"};
Mesh countLamps[7];
int countByRow[3]{};
bool swungAtPitch = false;
Mesh occupiedBaseIndicators[3];
int displayedResult = -1;
double resultShownAt = 0.0;
Mesh fielder;
Mesh blueFielder;
Mesh outPocket;
Mesh runner;
Mesh redRunner;
Mesh fireworkSpark;
bool redBatting = false;
constexpr double changeLeadIn = 0.45;
constexpr double changeRunTime = 2.7;
constexpr double changeDuration = changeLeadIn + 2 * changeRunTime;
double changeStart = -changeDuration;
double patrolPause = 0.0;

bool changingSides(double now)
{
	return now - changeStart < changeDuration;
}
Pitcher pitcher;
PitchingMachine pitchingMachine;
Batter batter;
Ball ball;

const glm::vec3 bases[] = {
	{0, 0.12f, 0}, {6.14f, 0.13f, -6.36f}, {0, 0.13f, -12.73f},
	{-6.14f, 0.13f, -6.36f}, {0, 0.12f, 0}
};
constexpr double secondsPerBase = 1.25;
struct BaseRunner
{
	int startBase;
	int destination;
	bool scored = false;
};

std::vector<BaseRunner> baseRunners;
std::vector<BaseRunner> retiringRunners;
double runnerStart = 0.0;
bool resultApplied = false;
float fieldViewBlend = 0.0f;

bool runnerMoving(double now)
{
	// 마지막에 추가된 타자 주자가 가장 많은 루를 달린다.
	return !baseRunners.empty() && now - runnerStart < baseRunners.back().destination * secondsPerBase;
}

constexpr float walkZoomInTime = 0.55f;
constexpr float walkFaceHoldTime = 0.5f;
constexpr float walkZoomOutTime = 0.5f;

bool showingWalkCloseup(double now)
{
	return displayedResult == walkResult
		&& now - runnerStart < walkZoomInTime + walkFaceHoldTime + walkZoomOutTime;
}

void drawHomeRunFireworks(const glm::mat4& viewProjection, double elapsed)
{
	const glm::vec3 centers[] = {{-9, 9, -20}, {8, 10, -22}, {0, 13, -26}, {-6, 11, -24}, {7, 12, -20}};
	const glm::vec3 colors[] = {{1, 0.38f, 0.2f}, {0.3f, 0.73f, 1}, {1, 0.85f, 0.25f},
		{1, 0.38f, 0.72f}, {0.45f, 1, 0.58f}};
	const GLint colorLocation = glGetUniformLocation(shaderProgramID, "sparkColor");
	for (std::size_t burst = 0; burst < std::size(centers); ++burst)
	{
		const float age = static_cast<float>(elapsed - 0.32 - burst * 0.78);
		if (age < 0.0f || age >= 1.3f) continue;
		const float fade = 1.0f - glm::smoothstep(0.65f, 1.3f, age);
		glUniform3fv(colorLocation, 1, glm::value_ptr(colors[burst] * fade));
		constexpr int sparkCount = 26;
		for (int index = 0; index < sparkCount; ++index)
		{
			const float y = 1.0f - 2.0f * (index + 0.5f) / sparkCount;
			const float ring = std::sqrt(1.0f - y * y);
			const float angle = index * 2.399963f + static_cast<float>(burst);
			const glm::vec3 direction(ring * std::cos(angle), y, ring * std::sin(angle));
			const glm::vec3 position = centers[burst] + direction * (0.35f + 4.6f * age)
				- glm::vec3(0, 1.55f * age * age, 0);
			drawMesh(shaderProgramID, fireworkSpark, placeOnField(position, 0.0f, 0.42f * fade), viewProjection);
		}
	}
	glUniform3f(colorLocation, 0, 0, 0);
}

void drawSurprisedPitcher(const glm::mat4& viewProjection)
{
	const GLint colorLocation = glGetUniformLocation(shaderProgramID, "sparkColor");
	const auto feature = [&](const glm::vec3& offset, const glm::vec3& size, const glm::vec3& color)
	{
		const glm::vec3 position = pitcher.Position() + PLAYER_SCALE * offset;
		const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), position), PLAYER_SCALE * size);
		glUniform3fv(colorLocation, 1, glm::value_ptr(color));
		drawMesh(shaderProgramID, fireworkSpark, model, viewProjection);
	};
	const glm::vec3 dark(0.13f, 0.045f, 0.035f);
	for (float side : {-1.0f, 1.0f})
	{
		feature({side * 0.105f, 1.785f, 0.301f}, {0.24f, 0.105f, 0.32f}, {1.0f, 0.95f, 0.86f});
		feature({side * 0.105f, 1.79f, 0.328f}, {0.095f, 0.055f, 0.17f}, dark);
	}
	feature({0, 1.63f, 0.257f}, {0.16f, 0.115f, 0.22f}, dark);
	// 모자챙 옆 관자놀이에 파란 물방울과 반사광을 붙인다.
	feature({0.255f, 1.83f, 0.222f}, {0.10f, 0.23f, 0.085f}, {0.22f, 0.70f, 0.97f});
	feature({0.265f, 1.85f, 0.245f}, {0.035f, 0.085f, 0.035f}, {0.85f, 0.97f, 1.0f});
	glUniform3f(colorLocation, 0, 0, 0);
}

void advanceRunners(int awardedBases, double now, bool walk = false)
{
	std::erase_if(baseRunners, [](const BaseRunner& runner) { return runner.destination == 4; });
	int forcedThrough = 0;
	if (walk)
	{
		bool occupied[4]{};
		for (const BaseRunner& runner : baseRunners) occupied[runner.destination] = true;
		while (forcedThrough < 3 && occupied[forcedThrough + 1]) ++forcedThrough;
	}
	for (BaseRunner& runner : baseRunners)
	{
		runner.startBase = runner.destination;
		if (!walk || runner.startBase <= forcedThrough)
			runner.destination = std::min(runner.startBase + awardedBases, 4);
	}
	baseRunners.push_back({0, awardedBases});
	runnerStart = now;
}

void recordOut(double now)
{
	if (++countByRow[2] == 3)
	{
		countByRow[2] = 0;
		retiringRunners = baseRunners;
		baseRunners.clear();
		changeStart = now;
		patrolPause += changeDuration;
		++halfInning;
		scoreShownAt = now;
		scoreDisplayDuration = 2.0;
		redBatting = !redBatting;
		batter.ResetSwing();
		pitchingMachine.SetKeys(redBatting ? PITCH_KEYS_2P : PITCH_KEYS_1P);
		glfwSetWindowTitle(glfwGetCurrentContext(),
		    redBatting ? "TOY BASEBALL - Red batting" : "TOY BASEBALL - Blue batting");
	}
}

TeamScore& battingScore()
{
	return teamScores[redBatting ? 0 : 1];
}

void recordRuns(double now)
{
	for (BaseRunner& runner : baseRunners)
	{
		if (runner.destination != 4 || runner.scored
			|| now - runnerStart < (4 - runner.startBase) * secondsPerBase) continue;
		runner.scored = true;
		TeamScore& score = battingScore();
		++score.runs;
		const int inning = halfInning / 2;
		if (inning < 3) ++score.inningRuns[inning];
		scoreShownAt = now;
		scoreDisplayDuration = runScoreDuration;
		lastRunAt = now;
	}
}

void drawRunningFigure(const glm::mat4& viewProjection, bool red, glm::vec3 position,
	const glm::vec3& direction, double elapsed, bool moving)
{
	const float yaw = glm::degrees(std::atan2(direction.x, direction.z));
	const float tilt = moving ? 12.0f * std::sin(26.0f * static_cast<float>(elapsed)) : 0.0f;
	position.y += 0.55f * PLAYER_SCALE * std::abs(std::sin(glm::radians(tilt)));
	const glm::mat4 model = glm::rotate(placeOnField(position, yaw, PLAYER_SCALE),
		glm::radians(tilt), glm::vec3(0, 0, 1));
	drawMesh(shaderProgramID, red ? redRunner : runner, model, viewProjection);
}

void drawRunner(const glm::mat4& viewProjection, double now, const BaseRunner& baseRunner)
{
	const int runnerDestination = baseRunner.destination;
	const double elapsed = now - runnerStart;
	const bool moving = elapsed < (runnerDestination - baseRunner.startBase) * secondsPerBase;
	if (runnerDestination == 4 && !moving) return;
	const float progress = static_cast<float>(std::min(baseRunner.startBase + elapsed / secondsPerBase,
		static_cast<double>(runnerDestination)));
	const int leg = std::min(static_cast<int>(progress), runnerDestination - 1);
	const glm::vec3 position = glm::mix(bases[leg], bases[leg + 1], progress - leg);
	drawRunningFigure(viewProjection, redBatting, position, bases[leg + 1] - bases[leg], elapsed, moving);
}

void drawRunners(const glm::mat4& viewProjection, double now)
{
	for (const BaseRunner& baseRunner : baseRunners) drawRunner(viewProjection, now, baseRunner);
}

struct FielderPatrol
{
	glm::vec3 center;
	glm::vec3 travel;
	double period;
	double phase;
};

// 발밑 중심 좌표, 왕복 변위, 왕복 주기(초), 시작 위상.
// 내야수는 가까운 1루–2루 또는 3루–2루 베이스라인과 평행하게 움직인다.
const FielderPatrol fielderPatrols[] = {
	{{ 5.4f, 0.04f,  -9.6f}, {0.65f, 0,  0.65f}, 3.7, 0.0}, // 1루수
	{{ 2.9f, 0.04f, -12.1f}, {0.78f, 0,  0.78f}, 4.2, 1.0}, // 2루수
	{{-5.4f, 0.04f,  -9.6f}, {0.65f, 0, -0.65f}, 3.9, 2.4}, // 3루수
	{{-3.8f, 0.04f, -12.1f}, {0.78f, 0, -0.78f}, 4.4, 3.1}, // 유격수
	{{-10.5f, 0.02f, -18.0f}, {1.70f, 0, 0.00f}, 5.1, 0.7}, // 좌익수
	{{  0.0f, 0.02f, -22.7f}, {2.10f, 0, 0.00f}, 5.6, 2.0}, // 중견수
	{{ 10.5f, 0.02f, -18.0f}, {1.70f, 0, 0.00f}, 5.3, 3.7}  // 우익수
};

constexpr float pocketScale = 0.55f;
constexpr float pocketDistance = 1.05f;

struct FielderPose
{
	glm::vec3 position;
	glm::vec3 forward;
	glm::vec3 pocketPosition;
};

FielderPose getFielderPose(const FielderPatrol& patrol, double now)
{
	constexpr double fullTurn = 6.283185307179586;
	const double patrolTime = now - patrolPause + std::max(0.0, changeDuration - (now - changeStart));
	const float offset = static_cast<float>(std::sin(fullTurn * patrolTime / patrol.period + patrol.phase));
	const glm::vec3 position = patrol.center + offset * patrol.travel;
	const glm::vec3 forward = glm::normalize(glm::vec3(-position.x, 0.0f, -position.z));
	return {position, forward, position + pocketDistance * forward + glm::vec3(0.0f, 0.002f, 0.0f)};
}

void drawDugoutRun(const glm::mat4& viewProjection, double now, const glm::vec3& fieldPosition,
	int order, bool offense = false)
{
	const double elapsed = now - changeStart - changeLeadIn;
	const bool leaving = elapsed < changeRunTime;
	const bool red = leaving ? redBatting != offense : redBatting == offense;
	const double legTime = leaving ? elapsed : elapsed - changeRunTime;
	const double delay = order * 0.065;
	const float progress = glm::clamp(static_cast<float>((legTime - delay) / (changeRunTime - delay)), 0.0f, 1.0f);
	// 더그아웃은 빨강이 3루 쪽(-X), 파랑이 1루 쪽(+X)을 쓴다.
	const glm::vec3 dugout(red ? -8.15f : 8.15f, 0.02f, -4.0f);
	const glm::vec3 start = leaving ? fieldPosition : dugout;
	const glm::vec3 end = leaving ? dugout : fieldPosition;
	drawRunningFigure(viewProjection, red, glm::mix(start, end, progress),
		end - start, std::max(0.0, legTime - delay), progress > 0 && progress < 1);
}

void drawFielders(const glm::mat4& viewProjection, double now)
{
	int order = 0;
	for (const FielderPatrol& patrol : fielderPatrols)
	{
		const FielderPose pose = getFielderPose(patrol, now);
		const float yaw = glm::degrees(std::atan2(pose.forward.x, pose.forward.z));
		drawMesh(shaderProgramID, outPocket, placeOnField(pose.pocketPosition, yaw, pocketScale), viewProjection);
		if (changingSides(now))
			drawDugoutRun(viewProjection, now, pose.position, order++);
		else
			drawMesh(shaderProgramID, redBatting ? blueFielder : fielder, placeOnField(pose.position, yaw, PLAYER_SCALE), viewProjection);
	}
	if (changingSides(now))
	{
		drawDugoutRun(viewProjection, now, pitcher.Position(), order);
		drawDugoutRun(viewProjection, now, batter.Position(), 0, true);
		if (now - changeStart < changeLeadIn + changeRunTime)
		{
			int runnerOrder = 1;
			for (const BaseRunner& baseRunner : retiringRunners)
			{
				if (baseRunner.destination <= 3)
					drawDugoutRun(viewProjection, now, bases[baseRunner.destination], runnerOrder++, true);
			}
		}
	}
	else
		pitcher.Draw(shaderProgramID, viewProjection, redBatting);
}

bool ballTouchesCylinder(const glm::vec3& ballPosition, const glm::vec3& base, float radius, float height)
{
	const glm::vec3 relative = ballPosition - base;
	const float horizontalGap = glm::max(0.0f, glm::length(glm::vec2(relative.x, relative.z)) - radius);
	const float verticalGap = relative.y - glm::clamp(relative.y, 0.0f, height);
	return horizontalGap * horizontalGap + verticalGap * verticalGap <= Ball::Radius() * Ball::Radius();
}

bool touchesDefense(const glm::vec3& ballPosition, double now)
{
	for (const FielderPatrol& patrol : fielderPatrols)
	{
		const FielderPose pose = getFielderPose(patrol, now);
		const glm::vec3 arms = pose.position + PLAYER_SCALE * (0.4f * pose.forward + glm::vec3(0, 0.75f, 0));
		if (ballTouchesCylinder(ballPosition, pose.pocketPosition, 1.27f * pocketScale, 0.13f * pocketScale)
			|| ballTouchesCylinder(ballPosition, pose.position, 0.44f * PLAYER_SCALE, 2.205f * PLAYER_SCALE)
			|| ballTouchesCylinder(ballPosition, arms, 0.58f * PLAYER_SCALE, 0.61f * PLAYER_SCALE))
		{
			return true;
		}
	}
	return false;
}

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

	GLFWwindow* window = glfwCreateWindow(width, height, "TOY BASEBALL - Blue batting", nullptr, nullptr);
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
	bool resultsLoaded = shaderProgramID != 0;
	for (std::size_t index = 0; resultsLoaded && index < std::size(resultModels); ++index)
	{
		resultsLoaded = InitBuffer(modelDirectory / resultModels[index], scoreboardResults[index]);
	}
	for (std::size_t index = 0; resultsLoaded && index < std::size(occupiedBaseIndicators); ++index)
	{
		const auto filename = "scoreboard_base_" + std::to_string(index + 1) + ".obj";
		resultsLoaded = InitBuffer(modelDirectory / filename, occupiedBaseIndicators[index]);
	}
	for (int row = 0; resultsLoaded && row < 3; ++row)
	{
		for (int index = 0; resultsLoaded && index < lampCounts[row]; ++index)
		{
			const auto filename = std::string("count_") + lampNames[row] + "_" + std::to_string(index + 1) + ".obj";
			resultsLoaded = InitBuffer(modelDirectory / filename, countLamps[lampOffsets[row] + index]);
		}
	}
	for (std::size_t index = 0; resultsLoaded && index < std::size(scoreGlyphs); ++index)
	{
		const auto filename = "score_glyph_" + std::to_string(index) + ".obj";
		resultsLoaded = InitBuffer(modelDirectory / filename, scoreGlyphs[index]);
	}
	if (!resultsLoaded
		|| !InitBuffer(modelDirectory / "stadium.obj", stadium)
		|| !InitBuffer(modelDirectory / "fielder_red.obj", fielder)
		|| !InitBuffer(modelDirectory / "fielder_blue.obj", blueFielder)
		|| !InitBuffer(modelDirectory / "hole.obj", outPocket)
		|| !InitBuffer(modelDirectory / "runner_blue.obj", runner)
		|| !InitBuffer(modelDirectory / "runner_red.obj", redRunner)
		|| !InitBuffer(modelDirectory / "ball.obj", fireworkSpark)
		|| !pitcher.Init(modelDirectory)
		|| !pitchingMachine.Init(modelDirectory)
		|| !batter.Init(modelDirectory)
		|| !ball.Init(modelDirectory))
	{
		DeleteBuffer(stadium);
		for (Mesh& result : scoreboardResults) DeleteBuffer(result);
		for (Mesh& indicator : occupiedBaseIndicators) DeleteBuffer(indicator);
		for (Mesh& lamp : countLamps) DeleteBuffer(lamp);
		for (Mesh& glyph : scoreGlyphs) DeleteBuffer(glyph);
		DeleteBuffer(fielder);
		DeleteBuffer(blueFielder);
		DeleteBuffer(runner);
		DeleteBuffer(redRunner);
		DeleteBuffer(fireworkSpark);
		DeleteBuffer(outPocket);
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

	// 빨강(1P)이 먼저 수비하고 파랑(2P)이 먼저 공격한다.
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

		recordRuns(now);
		const bool wasInFlight = ball.InFlight();
		if (!runnerMoving(now) && !changingSides(now) && !showingWalkCloseup(now)
			&& now - lastRunAt >= runScoreDuration)
			pitchingMachine.Update(window, deltaTime, ball);
		if (!wasInFlight && ball.InFlight()) swungAtPitch = false;
		const bool pitchInFlight = ball.InFlight() && !ball.WasBatted();
		if (!ball.WasBatted()) resultApplied = false;
		// 고속 직구가 한 프레임 사이에 타격 영역을 건너뛰지 않도록 나눠 검사한다.
		const int steps = static_cast<int>(std::ceil(deltaTime * 240.0f));
		const PitchKeys& battingKeys = redBatting ? PITCH_KEYS_1P : PITCH_KEYS_2P;
		const int stance = glfwGetKey(window, battingKeys.right) - glfwGetKey(window, battingKeys.left);
		if (steps > 0)
		{
			const float stepTime = deltaTime / steps;
			for (int step = 1; step <= steps; ++step)
			{
				const double stepEnd = now - (steps - step) * stepTime;
				ball.Update(stepTime);
				batter.CheckContact(stepEnd, stepTime, stance, ball);
				if (ball.IsFieldable()) ball.ResolveFielding(touchesDefense(ball.Position(), stepEnd));
			}
		}
		if (pitchInFlight && !ball.InFlight() && !ball.WasBatted())
		{
			if (swungAtPitch || ball.IsStrikePitch())
			{
				if (++countByRow[1] == 3)
				{
					countByRow[0] = countByRow[1] = 0;
					recordOut(now);
					displayedResult = 0;
					resultShownAt = now;
				}
			}
			else if (++countByRow[0] == 4)
			{
				countByRow[0] = countByRow[1] = 0;
				advanceRunners(1, now, true);
				++battingScore().walks;
				displayedResult = walkResult;
			}
		}
		if (!resultApplied && ball.BasesAwarded() >= 0)
		{
			countByRow[0] = countByRow[1] = 0;
			resultApplied = true;
			displayedResult = ball.BasesAwarded();
			resultShownAt = now;
			if (ball.BasesAwarded() > 0)
			{
				++battingScore().hits;
				advanceRunners(ball.BasesAwarded(), now);
			}
			else recordOut(now);
		}
		const bool showField = (ball.WasBatted() && ball.InFlight()) || runnerMoving(now)
			|| changingSides(now) || showingWalkCloseup(now) || now - lastRunAt < runScoreDuration;
		constexpr float cameraTransitionTime = 0.45f;
		const float cameraStep = deltaTime / cameraTransitionTime;
		fieldViewBlend = glm::clamp(fieldViewBlend + (showField ? cameraStep : -cameraStep), 0.0f, 1.0f);
		drawScene(now, stance);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	DeleteBuffer(stadium);
	for (Mesh& result : scoreboardResults) DeleteBuffer(result);
	for (Mesh& indicator : occupiedBaseIndicators) DeleteBuffer(indicator);
	for (Mesh& lamp : countLamps) DeleteBuffer(lamp);
	for (Mesh& glyph : scoreGlyphs) DeleteBuffer(glyph);
	DeleteBuffer(fielder);
	DeleteBuffer(blueFielder);
	DeleteBuffer(runner);
	DeleteBuffer(redRunner);
	DeleteBuffer(fireworkSpark);
	DeleteBuffer(outPocket);
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
	drawFielders(viewProjection, now);
	if (!showingWalkCloseup(now)) pitchingMachine.Draw(shaderProgramID, viewProjection);
	if (!runnerMoving(now) && !changingSides(now)) batter.Draw(shaderProgramID, viewProjection, now, stance, redBatting);
	drawRunners(viewProjection, now);
}

void drawScoreText(std::string_view label, glm::vec3 center, float size, const glm::mat4& viewProjection)
{
	const float advance = 0.79f * size;
	center.x -= (advance * label.size() - 0.15f * size) * 0.5f;
	for (char character : label)
	{
		const auto index = scoreCharacters.find(character);
		if (index != std::string_view::npos)
		{
			glm::vec3 glyphPosition = center;
			const float glyphHeight = character == '-' ? 0.14f * size : size;
			glyphPosition.y += (size - glyphHeight) * 0.5f;
			const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), glyphPosition),
				glm::vec3(size, glyphHeight, size));
			drawMesh(shaderProgramID, scoreGlyphs[index], model, viewProjection);
		}
		center.x += advance;
	}
}

void drawScoreboard(const glm::mat4& viewProjection)
{
	// 경기장 생성 스크립트의 전광판 앞면 좌표.
	constexpr float boardY = 5.56f;
	constexpr float boardZ = -34.55f;
	constexpr std::string_view headings = "123RHEB";
	for (int column = 0; column < 7; ++column)
	{
		const float x = -4.75f + column * 1.19f;
		drawScoreText(headings.substr(column, 1), {x, boardY + 1.95f, boardZ}, 0.49f, viewProjection);
		for (int row = 0; row < 2; ++row)
		{
			const TeamScore& score = teamScores[1 - row];
			const int values[] = {score.inningRuns[0], score.inningRuns[1], score.inningRuns[2],
				score.runs, score.hits, score.errors, score.walks};
			const float y = boardY + 1.25f - row * 0.63f;
			drawScoreText(std::to_string(values[column]), {x, y, boardZ}, 0.47f, viewProjection);
		}
	}
	drawScoreText("B", {-5.83f, boardY + 1.25f, boardZ}, 0.47f, viewProjection);
	drawScoreText("R", {-5.83f, boardY + 0.62f, boardZ}, 0.47f, viewProjection);
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
	const float cameraBlend = glm::smoothstep(0.0f, 1.0f, fieldViewBlend);
	glm::vec3 eye = glm::mix(glm::vec3(0, 1.5f, 2), glm::vec3(0, 8.75f, 8.25f), cameraBlend);
	glm::vec3 target = glm::mix(glm::vec3(0, 0, -5), glm::vec3(0, 0, -13.5f), cameraBlend);
	const float runElapsed = static_cast<float>(now - runnerStart);
	const bool homeRun = displayedResult == 4 && runnerMoving(now);
	const bool walkCloseup = showingWalkCloseup(now);
	float nearPlane = 1.0f;
	float zoom = 0.0f;
	if (walkCloseup)
	{
		const float returnStart = walkZoomInTime + walkFaceHoldTime;
		const float closeup = glm::smoothstep(0.0f, walkZoomInTime, runElapsed)
			* (1.0f - glm::smoothstep(returnStart, returnStart + walkZoomOutTime, runElapsed));
		const glm::vec3 face = pitcher.Position() + glm::vec3(0, 1.78f * PLAYER_SCALE, 0);
		eye = glm::mix(eye, face + glm::vec3(0.29f, 0.14f, 1.2f), closeup);
		target = glm::mix(target, face, closeup);
		nearPlane = glm::mix(nearPlane, 0.06f, closeup);
		zoom = closeup;
	}
	else if (homeRun)
	{
		const float duration = static_cast<float>(4 * secondsPerBase);
		const float orbitBlend = glm::smoothstep(0.0f, 0.55f, runElapsed)
			* (1.0f - glm::smoothstep(duration - 0.55f, duration, runElapsed));
		const float angle = glm::two_pi<float>() * glm::smoothstep(0.4f, duration - 0.4f, runElapsed);
		const glm::vec3 center(0, 2.5f, -13.5f);
		const glm::vec3 orbitEye = center + glm::vec3(33.0f * std::sin(angle), 21.0f, 33.0f * std::cos(angle));
		eye = glm::mix(eye, orbitEye, orbitBlend);
		target = glm::mix(target, center, orbitBlend);
	}
	const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	const float baseFov = glm::radians(glm::mix(54.0f, 17.0f, zoom));
	// 기본 창(3:2)보다 좁아지면 세로 시야를 넓혀 1·3루가 화면 밖으로 나가지 않게 한다.
	const float minAspect = 1.5f;
	const float fov = aspect < minAspect ? 2.0f * std::atan(std::tan(baseFov / 2) * minAspect / aspect) : baseFov;
	const glm::mat4 projection = glm::perspective(fov, aspect, nearPlane, 200.0f);
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
	drawScoreboard(viewProjection);
	for (int row = 0; row < 3; ++row)
	{
		for (int index = 0; index < countByRow[row]; ++index)
			drawMesh(shaderProgramID, countLamps[lampOffsets[row] + index], glm::mat4(1.0f), viewProjection);
	}
	// 아웃은 주루가 없으므로 잠깐 표시한다.
	if ((displayedResult == 0 && now - resultShownAt < 1.25)
		|| (displayedResult > 0 && runnerMoving(now)))
	{
		drawMesh(shaderProgramID, scoreboardResults[displayedResult], glm::mat4(1.0f), viewProjection);
	}
	bool occupiedBases[3]{};
	for (const BaseRunner& baseRunner : baseRunners)
	{
		const int reachedBase = std::min(baseRunner.destination,
			baseRunner.startBase + static_cast<int>((now - runnerStart) / secondsPerBase));
		if (reachedBase >= 1 && reachedBase <= 3) occupiedBases[reachedBase - 1] = true;
	}
	for (std::size_t index = 0; index < std::size(occupiedBases); ++index)
	{
		if (occupiedBases[index])
		{
			drawMesh(shaderProgramID, occupiedBaseIndicators[index],
				glm::mat4(1.0f), viewProjection);
		}
	}
	drawFielders(viewProjection, now);
	if (!walkCloseup) pitchingMachine.Draw(shaderProgramID, viewProjection);
	if (!runnerMoving(now) && !changingSides(now)) batter.Draw(shaderProgramID, viewProjection, now, stance, redBatting);
	drawRunners(viewProjection, now);
	ball.Draw(shaderProgramID, viewProjection);
	if (walkCloseup) drawSurprisedPitcher(viewProjection);
	if (homeRun) drawHomeRunFireworks(viewProjection, runElapsed);
	if (fieldViewBlend > 0.0f && now - scoreShownAt < scoreDisplayDuration)
	{
		const float hudAspect = std::max(1.0f, aspect);
		const glm::mat4 hudProjection = glm::ortho(-hudAspect, hudAspect, -1.0f, 1.0f);
		const std::string score = "R " + std::to_string(teamScores[0].runs) + " - "
			+ std::to_string(teamScores[1].runs) + " B";
		glDisable(GL_DEPTH_TEST);
		drawScoreText(score, {0, -0.17f, 0}, std::min(0.34f, 2.1f * hudAspect / score.size()), hudProjection);
		glEnable(GL_DEPTH_TEST);
	}

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
	if (changingSides(glfwGetTime())) return;
	const PitchKeys& battingKeys = redBatting ? PITCH_KEYS_1P : PITCH_KEYS_2P;
	if (key == battingKeys.fastball || key == battingKeys.breaking)
	{
		if (batter.StartSwing(glfwGetTime(), key == battingKeys.breaking) && ball.InFlight() && !ball.WasBatted())
			swungAtPitch = true;
	}
}
