#include "Mesh.h"

#include <cstddef>
#include <iostream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Model.h"

bool InitBuffer(const std::filesystem::path& modelPath, Mesh& mesh)
{
	std::vector<Vertex> vertices;
	if (!read_obj_file(modelPath, vertices)) return false;
	mesh.vertexCount = static_cast<GLsizei>(vertices.size());

	glGenVertexArrays(1, &mesh.VAO);
	glBindVertexArray(mesh.VAO);
	glGenBuffers(1, &mesh.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

	//--- 정점 위치, 재질색, 법선, 발광색을 vertex.glsl에 전달한다.
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, emission)));
	glEnableVertexAttribArray(3);
	glBindVertexArray(0);

	std::cout << modelPath.filename().string() << " loaded: " << mesh.vertexCount / 3 << " triangles\n";
	return true;
}

void DeleteBuffer(Mesh& mesh)
{
	if (mesh.VBO != 0) glDeleteBuffers(1, &mesh.VBO);
	if (mesh.VAO != 0) glDeleteVertexArrays(1, &mesh.VAO);
	mesh = {};
}

void drawMesh(GLuint shaderProgramID, const Mesh& mesh, const glm::mat4& model, const glm::mat4& viewProjection)
{
	const glm::mat4 transform = viewProjection * model;
	GLint location = glGetUniformLocation(shaderProgramID, "modelTransform");
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(transform));
	location = glGetUniformLocation(shaderProgramID, "model");
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(model));

	glBindVertexArray(mesh.VAO);
	glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
}

glm::mat4 placeOnField(const glm::vec3& position, float yawDegrees, float scale)
{
	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	model = glm::rotate(model, glm::radians(yawDegrees), glm::vec3(0, 1, 0));
	return glm::scale(model, glm::vec3(scale));
}
