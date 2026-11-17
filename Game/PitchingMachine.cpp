#include "PitchingMachine.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
	// 모델 좌표의 발사구 중심 (Tools/make_pitching_machine.mjs 의 BARREL 값)
	const glm::vec3 MODEL_MOUTH(0.0f, 0.22f, 0.38f);

	// 조준: 홈플레이트(폭 0.367) 양옆으로 공 반지름만큼 더 갈 수 있다
	constexpr float AIM_LIMIT = 0.28f;

	// 속도 (단위/초). 짧게 누르면 느리고, MAX_CHARGE초 이상 누르면 가장 빠르다.
	constexpr float MAX_CHARGE = 1.2f;
	constexpr float FASTBALL_MIN = 6.0f, FASTBALL_MAX = 12.0f; // 약 66 ~ 132 km/h
	constexpr float BREAKING_RATIO = 0.75f;                    // 변화구는 직구의 3/4 빠르기

	float toKmh(float unitsPerSecond) { return unitsPerSecond * METERS_PER_UNIT * 3.6f; }
}

bool PitchingMachine::Init(const std::filesystem::path& modelDirectory)
{
	return InitBuffer(modelDirectory / "pitching_machine.obj", mesh);
}

void PitchingMachine::Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const
{
	drawMesh(shaderProgramID, mesh, placeOnField(position, yaw), viewProjection);
}

void PitchingMachine::Delete()
{
	DeleteBuffer(mesh);
}

glm::vec3 PitchingMachine::MouthPosition() const
{
	return glm::vec3(placeOnField(position, yaw) * glm::vec4(MODEL_MOUTH, 1.0f));
}

void PitchingMachine::Update(GLFWwindow* window, float deltaTime, Ball& ball)
{
	const auto down = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

	// 공이 날아가는 동안에는 다음 투구를 받지 않는다.
	if (ball.InFlight())
	{
		chargingKey = 0;
		return;
	}

	// 왼쪽·오른쪽 조준 (화면 기준: 왼쪽이 -X, 3루 쪽).
	// 누르는 즉시 그쪽 끝으로 가고, 던질 때까지 유지된다. 아무것도 안 누르면 가운데로 던진다.
	if (down(keys.left)) aimX = -AIM_LIMIT;
	if (down(keys.right)) aimX = AIM_LIMIT;

	// 구종 키를 누르기 시작하면 시간을 재고, 놓는 순간 던진다.
	if (chargingKey == 0)
	{
		if (down(keys.fastball)) chargingKey = keys.fastball;
		else if (down(keys.breaking)) chargingKey = keys.breaking;
		chargeTime = 0.0f;
		return;
	}
	if (down(chargingKey))
	{
		chargeTime = std::min(chargeTime + deltaTime, MAX_CHARGE);
		return;
	}

	const bool breaking = chargingKey == keys.breaking;
	const float power = chargeTime / MAX_CHARGE;
	float speed = FASTBALL_MIN + (FASTBALL_MAX - FASTBALL_MIN) * power;
	if (breaking) speed *= BREAKING_RATIO;
	chargingKey = 0;

	const auto pitchType = breaking ? Ball::PitchType::Breaking : Ball::PitchType::Fastball;
	ball.Launch(MouthPosition(), glm::vec3(aimX, Ball::PlateHeight(pitchType), 0.0f), speed, pitchType);
	std::cout << keys.name << " pitch: " << (breaking ? "breaking ball" : "fastball") << ' '
		<< static_cast<int>(toKmh(speed)) << " km/h, aim " << aimX * METERS_PER_UNIT << " m\n";
	aimX = 0.0f; // 다음 투구는 다시 가운데에서 시작한다
}
