#include "Pitcher.h"

#include "Player.h"

namespace
{
	// ball.obj는 반지름 0.30 단위. 화면에서 잘 보이도록 선수 머리(반지름 0.32)와 같은 크기로 키운다.
	constexpr float BALL_SCALE = PLAYER_HEAD_RADIUS * PLAYER_SCALE / 0.30f;
}

bool Pitcher::Init(const std::filesystem::path& modelDirectory)
{
	return InitBuffer(modelDirectory / "player_red.obj", mesh)
		&& InitBuffer(modelDirectory / "ball.obj", ballMesh);
}

void Pitcher::Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const
{
	const glm::mat4 body = placeOnField(position, yaw, PLAYER_SCALE);
	drawMesh(shaderProgramID, mesh, body, viewProjection);

	// 공이 커서 몸통과 겹치지 않게 오른손 앞쪽으로 내밀어 든다 (선수 모델 좌표).
	const glm::vec3 hand = glm::vec3(body * glm::vec4(PLAYER_RIGHT_HAND + glm::vec3(-0.05f, 0.0f, 0.75f), 1.0f));
	drawMesh(shaderProgramID, ballMesh, placeOnField(hand, 0.0f, BALL_SCALE), viewProjection);
}

void Pitcher::Delete()
{
	DeleteBuffer(mesh);
	DeleteBuffer(ballMesh);
}
