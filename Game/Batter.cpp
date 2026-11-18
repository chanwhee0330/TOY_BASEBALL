#include "Batter.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "Ball.h"
#include "Player.h"

namespace
{
	// bat.obj는 손잡이 끝이 원점이고 +X로 길이 2.40 단위.
	// 실제 배트(86cm)의 두 배인 1.72m로 키운다. 굵기도 같은 비율로 커진다.
	constexpr float BAT_SCALE = meters(0.86f * 2.0f) / 2.40f;
	// 배트가 향하는 방향 (선수 모델 좌표): 위로 세우고 오른쪽 어깨 너머(-X, 포수 쪽)로 눕힌다.
	const glm::vec3 BAT_DIRECTION = glm::normalize(glm::vec3(-0.5f, 1.0f, -0.25f));
	const glm::vec3 SWING_BAT_DIRECTION = glm::normalize(glm::vec3(BAT_DIRECTION.x, 0.0f, BAT_DIRECTION.z));
	// 변화구 스윙은 기본 스윙에서 배트 끝을 15도 더 내린다.
	const float LOW_SWING_ANGLE = glm::radians(15.0f);
	const glm::vec3 LOW_SWING_BAT_DIRECTION =
		SWING_BAT_DIRECTION * glm::cos(LOW_SWING_ANGLE) - glm::vec3(0, glm::sin(LOW_SWING_ANGLE), 0);
	constexpr double SWING_TIME = 0.20;
	constexpr double BAT_LOWER_TIME = 0.05;
	constexpr double FALL_TIME = 0.20;
	constexpr double DOWN_TIME = 0.20;
	constexpr double RECOVERY_TIME = 0.30;
	constexpr double RECOVERY_START = SWING_TIME + FALL_TIME + DOWN_TIME;
	constexpr double SWING_DURATION = RECOVERY_START + RECOVERY_TIME;
	constexpr double HIT_DURATION = SWING_TIME + RECOVERY_TIME;
	constexpr double CONTACT_CENTER = 0.10;
	constexpr double CONTACT_HALF_WINDOW = 0.055;
	constexpr float SWING_ANGLE = 360.0f;
	constexpr float STANCE_STEP = meters(0.45f);
}

bool Batter::Init(const std::filesystem::path& modelDirectory)
{
	return InitBuffer(modelDirectory / "player_blue.obj", mesh)
		&& InitBuffer(modelDirectory / "player_red.obj", redMesh)
		&& InitBuffer(modelDirectory / "bat.obj", batMesh);
}

bool Batter::StartSwing(double now, bool aimLow)
{
	if (now - swingStartTime >= (connected ? HIT_DURATION : SWING_DURATION))
	{
		swingStartTime = now;
		lowSwing = aimLow;
		connected = false;
		return true;
	}
	return false;
}

void Batter::CheckContact(double stepEnd, float stepTime, int stance, Ball& ball)
{
	const float crossingFraction = ball.PlateCrossingFraction();
	if (connected || crossingFraction < 0.0f) return;

	const double crossingTime = stepEnd - (1.0 - crossingFraction) * stepTime;
	const double timingError = swingStartTime + CONTACT_CENTER - crossingTime;
	if (std::abs(timingError) > CONTACT_HALF_WINDOW) return;

	const auto swingType = lowSwing ? Ball::PitchType::Breaking : Ball::PitchType::Fastball;
	connected = ball.TryHit(swingType, stance, timingError, CONTACT_HALF_WINDOW);
	if (connected) ball.Update((1.0f - crossingFraction) * stepTime);
}

void Batter::Draw(GLuint shaderProgramID, const glm::mat4& viewProjection, double now, int stance, bool red) const
{
	const double elapsed = now - swingStartTime;
	float swingOffset = 0.0f;
	float fallAmount = 0.0f;
	glm::vec3 batDirection = BAT_DIRECTION;
	const double recoveryStart = connected ? SWING_TIME : RECOVERY_START;
	if (elapsed >= 0.0 && elapsed < recoveryStart + RECOVERY_TIME)
	{
		const float swingProgress = static_cast<float>(elapsed / SWING_TIME);
		swingOffset = SWING_ANGLE * glm::smoothstep(0.0f, 1.0f, swingProgress);

		// 한 바퀴 돌고 나면 등 쪽으로 넘어져 잠깐 멈췄다가 일어난다.
		const float fallProgress = static_cast<float>((elapsed - SWING_TIME) / FALL_TIME);
		const float recoveryProgress = static_cast<float>((elapsed - recoveryStart) / RECOVERY_TIME);
		const float recovered = glm::smoothstep(0.0f, 1.0f, recoveryProgress);
		if (!connected)
		{
			fallAmount = glm::smoothstep(0.0f, 1.0f, fallProgress) * (1.0f - recovered);
		}

		// 일어날 때 배트를 준비 자세로 들어 올린다.
		const float batProgress = static_cast<float>(elapsed / BAT_LOWER_TIME);
		const float lowered = glm::smoothstep(0.0f, 1.0f, batProgress) * (1.0f - recovered);
		const glm::vec3& swingDirection = lowSwing ? LOW_SWING_BAT_DIRECTION : SWING_BAT_DIRECTION;
		batDirection = glm::normalize(glm::mix(BAT_DIRECTION, swingDirection, lowered));
	}

	// 타석 위치를 축으로 몸을 돌리면 손에 붙인 배트도 함께 회전한다.
	// 선수 모델의 앞은 +Z이므로 로컬 X축으로 뒤쪽(-Z)을 향해 넘어뜨린다.
	const glm::vec3 bodyPosition = position + glm::vec3(STANCE_STEP * stance, meters(0.30f) * fallAmount, 0.0f);
	const glm::mat4 body = glm::rotate(placeOnField(bodyPosition, yaw + swingOffset, PLAYER_SCALE),
		glm::radians(-90.0f * fallAmount), glm::vec3(1, 0, 0));
	drawMesh(shaderProgramID, red ? redMesh : mesh, body, viewProjection);

	// 배트 모델의 +X(손잡이 → 끝)를 현재 배트 방향으로 돌리는 좌표축을 만든다.
	const glm::vec3 side = glm::normalize(glm::cross(batDirection, glm::vec3(0, 0, 1)));
	const glm::vec3 up = glm::cross(side, batDirection);
	glm::mat4 grip(1.0f);
	grip[0] = glm::vec4(batDirection, 0.0f);
	grip[1] = glm::vec4(up, 0.0f);
	grip[2] = glm::vec4(side, 0.0f);
	// 손잡이 끝이 오른손 조금 아래로 나오게 한다 (선수 모델 좌표).
	grip[3] = glm::vec4(PLAYER_RIGHT_HAND - batDirection * 0.15f, 1.0f);

	// 선수 몸 좌표계(줄인 크기)에 붙인 뒤, 배트 자체는 실제 길이로 줄인다.
	const glm::mat4 bat = body * grip * glm::scale(glm::mat4(1.0f), glm::vec3(BAT_SCALE / PLAYER_SCALE));
	drawMesh(shaderProgramID, batMesh, bat, viewProjection);
}

void Batter::Delete()
{
	DeleteBuffer(mesh);
	DeleteBuffer(redMesh);
	DeleteBuffer(batMesh);
}
