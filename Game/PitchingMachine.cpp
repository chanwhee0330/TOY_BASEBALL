#include "PitchingMachine.h"

namespace
{
	// 모델 좌표의 발사구 중심 (Tools/make_pitching_machine.mjs 의 BARREL 값)
	const glm::vec3 MODEL_MOUTH(0.0f, 0.22f, 0.38f);
}

bool PitchingMachine::Init(const std::filesystem::path& modelDirectory)
{
	return InitBuffer(modelDirectory / "pitching_machine.obj", mesh);
}

void PitchingMachine::Draw(GLuint shaderProgramID, const glm::mat4& viewProjection) const
{
	drawMesh(shaderProgramID, mesh, placeOnField(position, yaw), viewProjection);
}

void PitchingMachine::Delete()
{
	DeleteBuffer(mesh);
}

glm::vec3 PitchingMachine::MouthPosition() const
{
	return glm::vec3(placeOnField(position, yaw) * glm::vec4(MODEL_MOUTH, 1.0f));
}
