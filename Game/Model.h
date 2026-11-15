#pragma once

#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 normal;
	glm::vec3 emission;
};

bool read_obj_file(const std::filesystem::path& filename, std::vector<Vertex>& vertices);
