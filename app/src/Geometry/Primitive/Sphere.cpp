#include "Sphere.h"

#include <unordered_map>

using glm::vec3;

void AddVertex(VertexBuffer &vertices, const vec3 &vertex) { vertices.push_back(glm::normalize(vertex)); }

uint GetMidIndex(uint p1, uint p2, VertexBuffer &vertices, std::unordered_map<uint, uint> &cache) {
    const uint key = (std::min(p1, p2) << 16) + std::max(p1, p2);
    auto found = cache.find(key);
    if (found != cache.end()) return found->second;

    AddVertex(vertices, (vertices[p1] + vertices[p2]) / 2.0f);
    cache[key] = vertices.size() - 1;
    return cache[key];
}

Sphere::Sphere(float radius, int recursion_level) : Geometry() {
    static const float t = (1.0f + sqrt(5.0f)) / 2.0f;
    // Initial icosahedron vertices and face indices.
    // clang-format off
    static const std::array<vec3, 12> InitialVertices = {
    vec3{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
        {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
        {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1},
    };
    static const std::array<uint, 60> InitialIndices = {
        0, 11, 5,  0, 5, 1,   0, 1, 7,    0, 7, 10,  0, 10, 11,
        1, 5, 9,   5, 11, 4,  11, 10, 2,  10, 7, 6,  7, 1, 8,
        3, 9, 4,   3, 4, 2,   3, 2, 6,    3, 6, 8,   3, 8, 9,
        4, 9, 5,   2, 4, 11,  6, 2, 10,   8, 6, 7,   9, 8, 1,
    };
    // clang-format on

    for (auto &vertex : InitialVertices) AddVertex(Vertices, vertex);
    TriangleIndices.assign(InitialIndices.begin(), InitialIndices.end());

    std::unordered_map<uint, uint> mid_index_cache;
    for (int i = 0; i < recursion_level; i++) {
        std::vector<uint> new_indices;
        for (size_t j = 0; j < TriangleIndices.size(); j += 3) {
            uint a = TriangleIndices[j], b = TriangleIndices[j + 1], c = TriangleIndices[j + 2];
            uint ab = GetMidIndex(a, b, Vertices, mid_index_cache);
            uint bc = GetMidIndex(b, c, Vertices, mid_index_cache);
            uint ca = GetMidIndex(c, a, Vertices, mid_index_cache);
            new_indices.insert(new_indices.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
        }
        TriangleIndices.assign(new_indices.begin(), new_indices.end());
    }

    Normals.assign(Vertices.begin(), Vertices.end());
    Vertices *= radius;
    ComputeLineIndices();
}
