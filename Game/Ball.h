#pragma once

#include <filesystem>

#include <glm/glm.hpp>

#include "Mesh.h"

//--- 공: 투구 기계 발사구에서 나와 홈플레이트를 지나간다.
// 직구는 곧게 날아가고, 변화구는 처음엔 조금 높게 가다가 홈 앞에서 아래로 꺾인다.
class Ball
{
public:
	enum class PitchType { Fastball, Breaking };
	static constexpr float GroundHeight = 0.08f;
	static constexpr float PlateHeight(PitchType type)
	{
		return GroundHeight + meters(type == PitchType::Breaking ? 0.50f : 0.85f);
	}

	bool Init(const std::filesystem::path& modelDirectory);
	void Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const;
	void Delete();

	// start에서 target(홈플레이트 위 통과점)으로 speed(단위/초)만큼 빠르게 던진다.
	void Launch(const glm::vec3& start, const glm::vec3& target, float speed, PitchType type);
	void Update(float deltaTime);
	// 홈 근처에 있는 공을 맞히면 타구의 초기 속도로 전환한다.
	bool TryHit(PitchType swingType, int stance, double timingError, double timingWindow);
	float PlateCrossingFraction() const { return plateCrossingFraction; }

	bool InFlight() const { return flying; }
	bool IsFieldable() const { return flying && batted && !clearedFence; }
	void ResolveFielding(bool touchedDefense);
	const glm::vec3& Position() const { return position; }
	static float Radius();

private:
	glm::vec3 PathPoint(float s) const;   // 진행도 s(0 = 발사구, 1 = 홈 통과점)의 위치
	glm::vec3 PathTangent(float s) const; // PathPoint를 s로 미분한 값

	Mesh mesh;
	bool flying = false;
	bool batted = false;
	bool clearedFence = false;
	bool reportedCrossing = false;
	PitchType type = PitchType::Fastball;
	glm::vec3 start{0.0f};
	glm::vec3 target{0.0f};
	float drop = 0.0f;     // 변화구가 꺾이는 높이
	float progress = 0.0f; // 진행도 s
	float speed = 0.0f;
	float plateCrossingFraction = -1.0f;
	glm::vec3 position{0.0f};
	glm::vec3 velocity{0.0f};
};
