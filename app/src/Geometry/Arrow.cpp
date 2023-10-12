#include "Arrow.h"

#include <vector>

using glm::vec3;
using std::vector;

Arrow::Arrow(float length, float base_radius, float tip_radius, float tip_length, uint segments) : Geometry() {
    std::vector<vec3> vertices;
    std::vector<vec3> normals;
    std::vector<uint> indices; // triangle indices, but sides and cap should be quads.

    // Cylinder base.
    for (uint i = 0; i <= segments; ++i) {
        const float __angle = 2 * float(i) / segments;
        const float x = __cospif(__angle) * base_radius;
        const float z = __sinpif(__angle) * base_radius;

        vertices.push_back({x, tip_length, z});
        normals.push_back({x, 0.0f, z});

        vertices.push_back({x, tip_length + length, z});
        normals.push_back({x, 0.0f, z});
    }
    for (uint i = 0; i < segments * 2; i += 2) {
        indices.insert(indices.end(), {i, i + 1, i + 2, i + 1, i + 3, i + 2});
    }

    // Add top cap of the cylinder base
    vertices.push_back({0.0f, tip_length + length, 0.0f});
    normals.push_back({0.0f, 1.0f, 0.0f});
    const uint base_cap_center_index = vertices.size() - 1;
    for (uint i = 0; i < segments; ++i) {
        indices.insert(indices.end(), {2 * i + 1, base_cap_center_index, 2 * ((i + 1) % segments) + 1});
    }

    const int base_vertex_count = vertices.size();

    // Cone tip.
    for (uint i = 0; i <= segments; ++i) {
        const float __angle = 2 * float(i) / segments;
        const float x = __cospif(__angle) * tip_radius;
        const float z = __sinpif(__angle) * tip_radius;

        vertices.push_back({x, tip_length, z});
        normals.push_back({x, tip_length, z});
    }

    // Tip point.
    vertices.push_back({0.0f, 0.0f, 0.0f});
    normals.push_back({0.0f, -1.0f, 0.0f});

    // Tip indices.
    for (uint i = 0; i < segments; ++i) {
        indices.insert(indices.end(), {base_vertex_count + i, base_vertex_count + i + 1, base_vertex_count + segments + 1});
    }

    for (auto &vertex : vertices) {
        Mesh.add_vertex({vertex.x, vertex.y, vertex.z});
        Mesh.set_normal(Mesh.vertex_handle(Mesh.n_vertices() - 1), {vertex.x, vertex.y, vertex.z});
    }
    for (uint i = 0; i < indices.size(); i += 3) {
        Mesh.add_face(Mesh.vertex_handle(indices[i]), Mesh.vertex_handle(indices[i + 1]), Mesh.vertex_handle(indices[i + 2]));
    }

    UpdateBuffersFromMesh();
}
