#pragma once

#include <filesystem>

#include <glm/glm.hpp>

#include "Mesh.h"

//--- 투수: 마운드 위 투구 기계 바로 뒤에 서서 기계(홈 쪽)를 바라본다. (수비 팀, 빨강)
// 공은 투구 기계가 쏘므로 손에 들지 않고, 기계 뒤 상하좌우 버튼을 조작한다.
class Pitcher
{
public:
	bool Init(const std::filesystem::path& modelDirectory);
	void Draw(GLuint shaderProgramID, const glm::mat4& viewProjection, bool blue) const;
	void Delete();
	const glm::vec3& Position() const { return position; }

private:
	Mesh mesh;
	Mesh blueMesh;
	// 선수 모델은 원점이 발밑이고 +Z(홈 쪽)를 바라본다.
	// 투구 기계(z = -5.95)의 깔때기 뒤 끝(z = -6.27)에 모자챙이 닿지 않는 자리.
	// 마운드 경사면 위라 바닥 높이가 꼭대기(0.163)보다 조금 낮다.
	glm::vec3 position{0.0f, 0.146f, -6.47f};
	float yaw = 0.0f; // 기계와 홈을 바라본다
};
