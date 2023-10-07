#pragma once

#include <vector>

#include <glm/vec3.hpp>

using uint = unsigned int;

struct GeometryData {
    std::vector<glm::vec3> Vertices;
    std::vector<uint> Indices;
};
