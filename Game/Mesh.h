#pragma once

#include <GL/glew.h>

#include <filesystem>

#include <glm/glm.hpp>

//--- 모델 1 단위 = 3.048m (루 사이 거리 9 단위 = 27.43m)
constexpr float METERS_PER_UNIT = 3.048f;
constexpr float meters(float m) { return m / METERS_PER_UNIT; }

//--- OBJ 하나를 올린 GPU 버퍼
struct Mesh
{
	GLuint VAO = 0;
	GLuint VBO = 0;
	GLsizei vertexCount = 0;
};

bool InitBuffer(const std::filesystem::path& modelPath, Mesh& mesh);
void DeleteBuffer(Mesh& mesh);
void drawMesh(GLuint shaderProgramID, const Mesh& mesh, const glm::mat4& model, const glm::mat4& viewProjection);

// 위치로 옮기고 Y축으로 돌린 뒤 크기를 맞춘다 (각도는 도 단위).
glm::mat4 placeOnField(const glm::vec3& position, float yawDegrees, float scale = 1.0f);
