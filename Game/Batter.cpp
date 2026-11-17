#include "Batter.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Player.h"

namespace
{
	// bat.obj는 손잡이 끝이 원점이고 +X로 길이 2.40 단위.
	// 실제 배트(86cm)의 두 배인 1.72m로 키운다. 굵기도 같은 비율로 커진다.
	constexpr float BAT_SCALE = meters(0.86f * 2.0f) / 2.40f;
	// 배트가 향하는 방향 (선수 모델 좌표): 위로 세우고 오른쪽 어깨 너머(-X, 포수 쪽)로 눕힌다.
	const glm::vec3 BAT_DIRECTION = glm::normalize(glm::vec3(-0.5f, 1.0f, -0.25f));
}

bool Batter::Init(const std::filesystem::path& modelDirectory)
{
	return InitBuffer(modelDirectory / "player_blue.obj", mesh)
		&& InitBuffer(modelDirectory / "bat.obj", batMesh);
}

void Batter::Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const
{
	const glm::mat4 body = placeOnField(position, yaw, PLAYER_SCALE);
	drawMesh(shaderProgramID, mesh, body, viewProjection);

	// 배트 모델의 +X(손잡이 → 끝)를 BAT_DIRECTION으로 돌리는 좌표축을 만든다.
	const glm::vec3 side = glm::normalize(glm::cross(BAT_DIRECTION, glm::vec3(0, 0, 1)));
	const glm::vec3 up = glm::cross(side, BAT_DIRECTION);
	glm::mat4 grip(1.0f);
	grip[0] = glm::vec4(BAT_DIRECTION, 0.0f);
	grip[1] = glm::vec4(up, 0.0f);
	grip[2] = glm::vec4(side, 0.0f);
	// 손잡이 끝이 오른손 조금 아래로 나오게 한다 (선수 모델 좌표).
	grip[3] = glm::vec4(PLAYER_RIGHT_HAND - BAT_DIRECTION * 0.15f, 1.0f);

	// 선수 몸 좌표계(줄인 크기)에 붙인 뒤, 배트 자체는 실제 길이로 줄인다.
	const glm::mat4 bat = body * grip * glm::scale(glm::mat4(1.0f), glm::vec3(BAT_SCALE / PLAYER_SCALE));
	drawMesh(shaderProgramID, batMesh, bat, viewProjection);
}

void Batter::Delete()
{
	DeleteBuffer(mesh);
	DeleteBuffer(batMesh);
}
