#pragma once

#include <filesystem>

#include <glm/glm.hpp>

#include "Mesh.h"

//--- 투구 기계: 마운드 위에서 홈을 바라본다. 투수는 기계 뒤에서 상하좌우 버튼으로 조작한다.
// 모델(pitching_machine.obj)은 원점이 바닥 가운데이고 +Z(홈 쪽)로 공을 쏜다. 크기는 1 단위 그대로.
class PitchingMachine
{
public:
	bool Init(const std::filesystem::path& modelDirectory);
	void Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const;
	void Delete();

	// 공이 나오는 발사구 중심 (월드 좌표). 투구를 만들 때 공의 출발점으로 쓴다.
	glm::vec3 MouthPosition() const;

private:
	Mesh mesh;
	// 마운드 중심(z = -6.05)보다 조금 앞. 바닥판이 마운드 꼭대기(y = 0.163)에 놓인다.
	glm::vec3 position{0.0f, 0.163f, -5.95f};
	float yaw = 0.0f; // 홈을 바라본다
};
