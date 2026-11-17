#pragma once

#include <filesystem>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Ball.h"
#include "Mesh.h"

//--- 투구 조작 키 (제안서: 1P는 W·A·S·D, 2P는 방향키. 수비하는 쪽이 투구를 조작한다)
struct PitchKeys
{
	int left;     // 누르면 바로 왼쪽 끝으로 조준
	int right;    // 누르면 바로 오른쪽 끝으로 조준
	int fastball; // 눌렀다 놓으면 직구. 누른 시간이 길수록 빠르다
	int breaking; // 눌렀다 놓으면 아래로 꺾이는 변화구
	const char* name;
};
inline constexpr PitchKeys PITCH_KEYS_1P{GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_W, GLFW_KEY_S, "1P"};
inline constexpr PitchKeys PITCH_KEYS_2P{GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN, "2P"};

//--- 투구 기계: 마운드 위에서 홈을 바라본다. 투수는 기계 뒤에서 상하좌우 버튼으로 조작한다.
// 모델(pitching_machine.obj)은 원점이 바닥 가운데이고 +Z(홈 쪽)로 공을 쏜다. 크기는 1 단위 그대로.
class PitchingMachine
{
public:
	bool Init(const std::filesystem::path& modelDirectory);
	void Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const;
	void Delete();

	// 수비하는 플레이어의 키로 바꾼다 (공수 교대 때 부른다).
	void SetKeys(const PitchKeys& keys) { this->keys = keys; }
	// 키 입력을 읽어 조준하고, 직구·변화구 키를 놓는 순간 ball을 쏜다.
	void Update(GLFWwindow* window, float deltaTime, Ball& ball);

	// 공이 나오는 발사구 중심 (월드 좌표). 기계는 움직이지 않고, 조준은 공이 날아가는 방향만 바꾼다.
	glm::vec3 MouthPosition() const;

private:
	Mesh mesh;
	PitchKeys keys = PITCH_KEYS_1P;
	// 마운드 중심(z = -6.05)보다 조금 앞. 바닥판이 마운드 꼭대기(y = 0.163)에 놓인다.
	glm::vec3 position{0.0f, 0.163f, -5.95f};
	float yaw = 0.0f; // 홈을 바라본다 (고정)

	float aimX = 0.0f;       // 홈플레이트 위 조준 위치 (좌우, x)
	int chargingKey = 0;     // 누르고 있는 구종 키 (0이면 없음)
	float chargeTime = 0.0f; // 구종 키를 누르고 있던 시간
};
