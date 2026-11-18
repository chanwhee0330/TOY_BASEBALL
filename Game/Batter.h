#pragma once

#include <filesystem>

#include <glm/glm.hpp>

#include "Mesh.h"

class Ball;

//--- 타자: 3루 쪽(왼쪽) 타석에 서서 홈플레이트를 바라본다. (공격 팀, 파랑)
// 오른손으로 배트를 쥐고 어깨 뒤로 세워 든다.
class Batter
{
public:
	bool Init(const std::filesystem::path& modelDirectory);
	bool StartSwing(double now, bool lowSwing);
	void CheckContact(double stepEnd, float stepTime, int stance, Ball& ball);
	void Draw(GLuint shaderProgramID, const glm::mat4& viewProjection, double now, int stance, bool red) const;
	void Delete();
	void ResetSwing() { swingStartTime = -1.0; connected = false; }
	const glm::vec3& Position() const { return position; }

private:
	Mesh mesh;
	Mesh redMesh;
	Mesh batMesh;
	// 선수 모델은 원점이 발밑이고 +Z(홈 쪽)를 바라본다.
	// 3루 쪽 타석 한가운데 (홈플레이트 중심에서 1.32m 옆).
	glm::vec3 position{-0.433f, 0.08f, 0.0f};
	float yaw = 90.0f; // 홈플레이트(+X)를 바라본다
	double swingStartTime = -1.0;
	bool lowSwing = false;
	bool connected = false;
};
