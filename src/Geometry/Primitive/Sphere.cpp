#include "Sphere.h"

#include <unordered_map>

using glm::vec3;

static void AddVertex(std::vector<glm::vec3> &vertices, const vec3 &vertex) { vertices.push_back(glm::normalize(vertex)); }

uint GetMidIndex(uint p1, uint p2, std::vector<glm::vec3> &vertices, std::unordered_map<uint, uint> &cache) {
    const uint key = (std::min(p1, p2) << 16) + std::max(p1, p2);
    auto found = cache.find(key);
    if (found != cache.end()) return found->second;

    AddVertex(vertices, (vertices[p1] + vertices[p2]) / 2.0f);
    cache[key] = vertices.size() - 1;
    return cache[key];
}

Sphere::Sphere(float radius, int recursion_level) : MeshBuffers() {
    static const float t = (1.0f + sqrt(5.0f)) / 2.0f;
    // Initial icosahedron vertices and face indices.
    // clang-format off
    std::vector<vec3> vertices = {
        {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
        {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
        {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1},
    };
    for (auto &vertex : vertices) vertex = glm::normalize(vertex);

    std::vector<uint> indices = {
        0, 11, 5,  0, 5, 1,   0, 1, 7,    0, 7, 10,  0, 10, 11,
        1, 5, 9,   5, 11, 4,  11, 10, 2,  10, 7, 6,  7, 1, 8,
        3, 9, 4,   3, 4, 2,   3, 2, 6,    3, 6, 8,   3, 8, 9,
        4, 9, 5,   2, 4, 11,  6, 2, 10,   8, 6, 7,   9, 8, 1,
    };
    // clang-format on

    std::unordered_map<uint, uint> mid_index_cache;
    for (int i = 0; i < recursion_level; i++) {
        std::vector<uint> new_indices;
        for (size_t j = 0; j < indices.size(); j += 3) {
            uint a = indices[j], b = indices[j + 1], c = indices[j + 2];
            uint ab = GetMidIndex(a, b, vertices, mid_index_cache);
            uint bc = GetMidIndex(b, c, vertices, mid_index_cache);
            uint ca = GetMidIndex(c, a, vertices, mid_index_cache);
            new_indices.insert(new_indices.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
        }
        indices.assign(new_indices.begin(), new_indices.end());
    }

    for (auto &v : vertices) v *= radius;

    for (auto &v : vertices) Mesh.add_vertex({v.x, v.y, v.z});
    for (uint i = 0; i < indices.size(); i += 3) {
        Mesh.add_face(Mesh.vertex_handle(indices[i]), Mesh.vertex_handle(indices[i + 1]), Mesh.vertex_handle(indices[i + 2]));
    }

    UpdateBuffersFromMesh();
}
