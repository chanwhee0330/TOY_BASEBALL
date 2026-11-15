#include "Model.h"

#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

struct Material
{
	glm::vec3 color{1.0f};
	glm::vec3 emission{0.0f};
};

bool read_mtl_file(const std::filesystem::path& filename,
	std::unordered_map<std::string, Material>& materials)
{
	std::ifstream file(filename);
	if (!file)
	{
		std::wcerr << L"MTL file not found: " << filename.c_str() << L'\n';
		return false;
	}

	Material* material = nullptr;
	std::string line;
	while (std::getline(file, line))
	{
		std::istringstream row(line);
		std::string type;
		row >> type;
		if (type == "newmtl")
		{
			std::string name;
			if (!(row >> name)) return false;
			material = &materials[name];
		}
		else if (type == "Kd" || type == "Ke")
		{
			if (material == nullptr) return false;
			glm::vec3& color = type == "Kd" ? material->color : material->emission;
			if (!(row >> color.r >> color.g >> color.b)) return false;
		}
	}
	return !file.bad();
}

bool read_obj_index(std::string_view token, size_t count, size_t& index)
{
	const auto result = std::from_chars(token.data(), token.data() + token.size(), index);
	if (result.ec != std::errc{} || result.ptr != token.data() + token.size()
		|| index == 0 || index > count)
	{
		return false;
	}
	--index;
	return true;
}

bool read_obj_file(const std::filesystem::path& filename, std::vector<Vertex>& vertices)
{
	std::ifstream file(filename);
	if (!file)
	{
		std::wcerr << L"OBJ file not found: " << filename.c_str() << L'\n';
		return false;
	}

	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::unordered_map<std::string, Material> materials;
	Material material;
	std::vector<Vertex> modelVertices;
	size_t lineNumber = 0;
	auto fail = [&](const wchar_t* message)
	{
		std::wcerr << filename.c_str() << L'(' << lineNumber << L"): " << message << L'\n';
		return false;
	};

	std::string line;
	while (std::getline(file, line))
	{
		++lineNumber;
		std::istringstream row(line);
		std::string type;
		row >> type;
		if (type == "v" || type == "vn")
		{
			glm::vec3 value;
			if (!(row >> value.x >> value.y >> value.z)) return fail(L"Invalid vertex or normal.");
			(type == "v" ? positions : normals).push_back(value);
		}
		else if (type == "mtllib")
		{
			std::string materialFile;
			if (!(row >> materialFile) || !read_mtl_file(filename.parent_path() / materialFile, materials))
				return fail(L"Could not read the material file.");
		}
		else if (type == "usemtl")
		{
			std::string name;
			row >> name;
			auto found = materials.find(name);
			if (found == materials.end()) return fail(L"Unknown material.");
			material = found->second;
		}
		else if (type == "f")
		{
			// 경기장 면은 삼각형이며 v/vt/vn에서 위치와 법선을 사용한다.
			for (int corner = 0; corner < 3; ++corner)
			{
				std::string token;
				if (!(row >> token)) return fail(L"A face must have three vertices.");
				const size_t firstSlash = token.find('/');
				const size_t secondSlash = token.rfind('/');
				if (firstSlash == secondSlash) return fail(L"A face vertex must have a normal.");

				size_t positionIndex = 0;
				size_t normalIndex = 0;
				const std::string_view indices(token);
				if (!read_obj_index(indices.substr(0, firstSlash), positions.size(), positionIndex)
					|| !read_obj_index(indices.substr(secondSlash + 1), normals.size(), normalIndex))
				{
					return fail(L"Invalid position or normal index.");
				}

				modelVertices.push_back({
					positions[positionIndex], material.color, normals[normalIndex], material.emission
				});
			}
			std::string extra;
			if (row >> extra && extra.front() != '#') return fail(L"A face must have three vertices.");
		}
	}

	if (file.bad() || modelVertices.empty()) return fail(L"No readable triangles.");
	vertices.swap(modelVertices);
	return true;
}
