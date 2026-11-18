#include "Pitcher.h"

#include "Player.h"

bool Pitcher::Init(const std::filesystem::path& modelDirectory)
{
	return InitBuffer(modelDirectory / "player_red.obj", mesh)
		&& InitBuffer(modelDirectory / "player_blue.obj", blueMesh);
}

void Pitcher::Draw(GLuint shaderProgramID, const glm::mat4& viewProjection, bool blue) const
{
	drawMesh(shaderProgramID, blue ? blueMesh : mesh, placeOnField(position, yaw, PLAYER_SCALE), viewProjection);
}

void Pitcher::Delete()
{
	DeleteBuffer(mesh);
	DeleteBuffer(blueMesh);
}
