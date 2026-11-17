#include "Pitcher.h"

#include "Player.h"

bool Pitcher::Init(const std::filesystem::path& modelDirectory)
{
	return InitBuffer(modelDirectory / "player_red.obj", mesh);
}

void Pitcher::Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const
{
	drawMesh(shaderProgramID, mesh, placeOnField(position, yaw, PLAYER_SCALE), viewProjection);
}

void Pitcher::Delete()
{
	DeleteBuffer(mesh);
}
