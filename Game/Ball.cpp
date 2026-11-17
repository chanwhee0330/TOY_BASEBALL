#include "Ball.h"

#include <cmath>
#include <iostream>
#include <iterator>

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
	constexpr float BALL_RADIUS = BALL_SCALE * 0.30f;
	constexpr float GRAVITY = meters(9.81f);
	constexpr float GROUND_RESTITUTION = 0.28f;
	constexpr float BOUNCE_SPEED_THRESHOLD = meters(0.65f);
	constexpr float LANDING_FRICTION = 0.45f;
	// 잔디 위에서는 약한 타구라도 몇 초 안에 감속이 눈에 띄도록 한다.
	constexpr float ROLL_DECELERATION = meters(3.4f);
	constexpr float ROLL_STOP_SPEED = meters(0.18f);
	constexpr double PERFECT_TIMING_SECONDS = 0.005;
	// 경기장 모델의 외야 펜스 안쪽 반지름과 상단 높이.
	constexpr float FENCE_RADIUS = 27.0f;
	constexpr float FENCE_TOP_Y = 1.7f;
	constexpr const char* FENCE_RESULTS[] = {
		"TRIPLE", "DOUBLE", "SINGLE", "HOME RUN!", "SINGLE", "DOUBLE", "TRIPLE"
	};

	const char* FenceResult(const glm::vec3& contact)
	{
		const float angle = glm::degrees(std::atan2(contact.x, -contact.z));
		constexpr int zoneCount = static_cast<int>(std::size(FENCE_RESULTS));
		const int zone = glm::clamp(static_cast<int>((angle + 45.0f) * zoneCount / 90.0f), 0, zoneCount - 1);
		return FENCE_RESULTS[zone];
	}
}

bool Ball::Init(const std::filesystem::path& modelDirectory)
{
	return InitBuffer(modelDirectory / "ball.obj", mesh);
}

float Ball::Radius()
{
	return BALL_RADIUS;
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
	batted = false;
	clearedFence = false;
	reportedCrossing = false;
	plateCrossingFraction = -1.0f;
}

bool Ball::TryHit(PitchType swingType, int stance, double timingError, double timingWindow)
{
	if (!flying || batted || plateCrossingFraction < 0.0f
		|| std::abs(target.y - PlateHeight(type)) > BALL_RADIUS)
	{
		return false;
	}

	constexpr float LANE_THRESHOLD = meters(0.40f);
	const int pitchLane = (target.x > LANE_THRESHOLD) - (target.x < -LANE_THRESHOLD);
	const float alignment = 1.0f - 0.5f * static_cast<float>(std::abs(stance - pitchLane));
	const float timingOffset = static_cast<float>(timingError / timingWindow);
	const float power = alignment * alignment * (1.0f - std::abs(timingOffset));

	// 타석이 공의 좌우 코스와 맞고 타이밍이 좋을수록 높고 빠르게 날아간다.
	// 빠른 스윙은 -X, 늦은 스윙은 +X로 보낸다.
	const float angle = glm::radians(44.0f * timingOffset);
	float horizontalSpeed = glm::mix(4.0f, 10.0f, power);
	float lift = glm::mix(1.3f, 4.0f, power);
	const char* result = alignment == 1.0f ? "squared up" : "weak contact";
	if (swingType == type && stance == pitchLane && std::abs(timingError) <= PERFECT_TIMING_SECONDS)
	{
		horizontalSpeed = 15.0f;
		lift = 5.4f;
		result = "perfect contact";
	}
	else if (swingType != type)
	{
		horizontalSpeed *= 0.65f;
		if (type == PitchType::Breaking)
		{
			// 높은 배트가 낮은 공의 윗부분을 눌러 땅볼을 만든다.
			lift = glm::mix(-1.0f, 0.65f, power);
			result = "ground ball";
		}
		else
		{
			// 낮은 배트가 직구의 아랫부분을 받쳐 높이 띄운다.
			lift = glm::mix(4.0f, 6.0f, power);
			horizontalSpeed *= 0.65f;
			result = "pop fly";
		}
	}
	position = target;
	velocity = glm::vec3(glm::sin(angle) * horizontalSpeed, lift,
		-glm::cos(angle) * horizontalSpeed);
	batted = true;
	std::cout << "  " << result << ", spray angle " << glm::degrees(angle) << " deg\n";
	return true;
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
	plateCrossingFraction = -1.0f;
	if (!flying) return;

	if (batted)
	{
		constexpr float contactY = GroundHeight + BALL_RADIUS;
		const bool rolling = position.y <= contactY && velocity.y <= 0.0f;
		const glm::vec3 previousPosition = position;
		if (rolling)
		{
			// 구르는 동안 지면 저항이 속력을 줄인다. 정지 시점을 넘겨 적분하지 않는다.
			const float rollingSpeed = glm::length(velocity);
			if (rollingSpeed <= ROLL_STOP_SPEED)
			{
				velocity = glm::vec3(0.0f);
				flying = false;
				return;
			}
			const float timeToStop = (rollingSpeed - ROLL_STOP_SPEED) / ROLL_DECELERATION;
			const float rollingTime = glm::min(deltaTime, timeToStop);
			const float nextSpeed = rollingSpeed - ROLL_DECELERATION * rollingTime;
			const glm::vec3 direction = velocity / rollingSpeed;
			position += direction * (0.5f * (rollingSpeed + nextSpeed) * rollingTime);
			velocity = deltaTime >= timeToStop ? glm::vec3(0.0f) : direction * nextSpeed;
		}
		else
		{
			position += velocity * deltaTime;
			position.y -= 0.5f * GRAVITY * deltaTime * deltaTime;
			velocity.y -= GRAVITY * deltaTime;
		}

		if (!clearedFence)
		{
			// 공의 앞면이 펜스에 처음 닿는 지점에서 높이를 확인한다.
			constexpr float contactRadius = FENCE_RADIUS - BALL_RADIUS;
			const float radius = glm::length(glm::vec2(position.x, position.z));
			if (radius >= contactRadius)
			{
				const float previousRadius = glm::length(glm::vec2(previousPosition.x, previousPosition.z));
				if (previousRadius < contactRadius)
				{
					const float fraction = (contactRadius - previousRadius) / (radius - previousRadius);
					const float crossingHeight = glm::mix(previousPosition.y, position.y, fraction);
					clearedFence = crossingHeight - BALL_RADIUS > FENCE_TOP_Y;
					if (!clearedFence)
					{
						position = glm::mix(previousPosition, position, fraction);
						position.y = glm::max(position.y, contactY);
						std::cout << "  " << FenceResult(position) << '\n';
						flying = false;
						return;
					}
					std::cout << "  HOME RUN!\n";
				}
			}
		}
		if (!rolling && position.y <= contactY)
		{
			position.y = contactY;
			if (clearedFence)
			{
				flying = false;
				return;
			}

			// 착지 충격에 비례해 수평 속도를 잃고, 작은 반동은 구름 운동으로 바꾼다.
			const float impactSpeed = -velocity.y;
			const glm::vec3 horizontalVelocity(velocity.x, 0.0f, velocity.z);
			const float horizontalSpeed = glm::length(horizontalVelocity);
			const float speedRatio = horizontalSpeed > 0.0f
				? glm::max(0.0f, 1.0f - LANDING_FRICTION * impactSpeed / horizontalSpeed)
				: 0.0f;
			velocity = horizontalVelocity * speedRatio;
			const float reboundSpeed = impactSpeed * GROUND_RESTITUTION;
			if (reboundSpeed >= BOUNCE_SPEED_THRESHOLD) velocity.y = reboundSpeed;
		}
		if (velocity == glm::vec3(0.0f)) flying = false;
		return;
	}

	// 경로 길이가 speed만큼 늘어나도록 진행도를 올린다.
	const float previousZ = position.z;
	const float pathSpeed = glm::length(PathTangent(progress));
	if (pathSpeed > 1e-5f) progress += speed * deltaTime / pathSpeed;
	position = PathPoint(progress);

	if (!reportedCrossing && position.z >= 0.0f)
	{
		reportedCrossing = true;
		plateCrossingFraction = -previousZ / (position.z - previousZ);
		const glm::vec3 cross = PathPoint(1.0f);
		std::cout << "  crossed home plate: side " << cross.x * METERS_PER_UNIT << " m, height "
			<< (cross.y - GroundHeight) * METERS_PER_UNIT << " m\n";
	}
	if (position.z > END_Z) flying = false;
}
