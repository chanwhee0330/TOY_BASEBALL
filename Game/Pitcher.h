#pragma once

#include <filesystem>

#include <glm/glm.hpp>

#include "Mesh.h"

//--- 투수: 마운드 투수판 위에 서서 홈을 바라본다. (수비 팀, 빨강)
// 오른손에 공을 쥐고 있다.
class Pitcher
{
public:
	bool Init(const std::filesystem::path& modelDirectory);
	void Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const;
	void Delete();

private:
	Mesh mesh;
	Mesh ballMesh;
	// 선수 모델은 원점이 발밑이고 +Z(홈 쪽)를 바라본다.
	// 마운드 투수판 위 (투수판 윗면 y = 0.173).
	glm::vec3 position{0.0f, 0.173f, -6.05f};
	float yaw = 0.0f; // 홈을 바라본다
};
