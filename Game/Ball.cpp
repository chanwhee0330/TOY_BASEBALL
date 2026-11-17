#include "Ball.h"

#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

#include "Player.h"

namespace
{
	// ball.obj는 반지름 0.30 단위. 화면에서 잘 보이도록 선수 머리(반지름 0.32)와 같은 크기로 키운다.
	constexpr float BALL_SCALE = PLAYER_HEAD_RADIUS * PLAYER_SCALE / 0.30f;
	// 변화구가 홈 앞에서 아래로 꺾이는 높이 (60cm)
	constexpr float BREAK_DROP = meters(0.6f);
	// 공이 포수 뒤 이만큼(z)까지 가면 투구를 끝낸다
	constexpr float END_Z = 2.5f;
}

bool Ball::Init(const std::filesystem::path& modelDirectory)
{
	return InitBuffer(modelDirectory / "ball.obj", mesh);
}

void Ball::Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const
{
	if (!flying) return;
	drawMesh(shaderProgramID, mesh, placeOnField(position, 0.0f, BALL_SCALE), viewProjection);
}

void Ball::Delete()
{
	DeleteBuffer(mesh);
}

void Ball::Launch(const glm::vec3& from, const glm::vec3& to, float unitsPerSecond, PitchType pitchType)
{
	start = from;
	target = to;
	speed = unitsPerSecond;
	type = pitchType;
	drop = type == PitchType::Breaking ? BREAK_DROP : 0.0f;
	progress = 0.0f;
	position = start;
	flying = true;
	reportedCrossing = false;
}

// 직구: start → target 직선.
// 변화구: 목표보다 drop만큼 높은 곳을 향하다가 -drop * s^3 만큼 떨어져, s = 1에서 정확히 target을 지난다.
// s^3이라 처음에는 거의 안 떨어지고 홈 가까이에서 갑자기 꺾인다.
glm::vec3 Ball::PathPoint(float s) const
{
	const glm::vec3 up(0.0f, 1.0f, 0.0f);
	if (s > 1.0f) return PathPoint(1.0f) + PathTangent(1.0f) * (s - 1.0f); // 홈을 지난 뒤에는 그 방향 그대로 간다
	return start + (target + up * drop - start) * s - up * (drop * s * s * s);
}

glm::vec3 Ball::PathTangent(float s) const
{
	const glm::vec3 up(0.0f, 1.0f, 0.0f);
	if (s > 1.0f) s = 1.0f;
	return (target + up * drop - start) - up * (3.0f * drop * s * s);
}

void Ball::Update(float deltaTime)
{
	if (!flying) return;

	// 경로 길이가 speed만큼 늘어나도록 진행도를 올린다.
	const float pathSpeed = glm::length(PathTangent(progress));
	if (pathSpeed > 1e-5f) progress += speed * deltaTime / pathSpeed;
	position = PathPoint(progress);

	if (!reportedCrossing && position.z >= 0.0f)
	{
		reportedCrossing = true;
		const glm::vec3 cross = PathPoint(1.0f);
		std::cout << "  crossed home plate: side " << cross.x * METERS_PER_UNIT << " m, height "
			<< (cross.y - 0.08f) * METERS_PER_UNIT << " m\n";
	}
	if (position.z > END_Z) flying = false;
}
