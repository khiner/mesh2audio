#pragma once

#include <vector>

#include <glm/vec3.hpp>

using uint = unsigned int;

struct TriangleBuffers {
    TriangleBuffers() = default;
    TriangleBuffers(std::vector<glm::vec3> &&vertices, std::vector<uint> &&indices)
        : Vertices(std::move(vertices)), Indices(std::move(indices)) {}

    inline const std::vector<glm::vec3> &GetVertices() const { return Vertices; }
    inline const std::vector<uint> &GetIndices() const { return Indices; }

protected:
    std::vector<glm::vec3> Vertices;
    std::vector<uint> Indices;
};
