#include "Arrow.h"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>

using glm::vec3;
using std::vector;

Arrow::Arrow(float length, float base_radius, float tip_radius, float tip_length, uint segments) : Geometry() {
    // Cylinder base.
    for (uint i = 0; i <= segments; ++i) {
        const float ratio = 2 * float(i) / segments;
        const float x = __cospif(ratio) * base_radius;
        const float z = __sinpif(ratio) * base_radius;

        Vertices.push_back({x, tip_length, z});
        Normals.push_back({x, 0.0f, z});

        Vertices.push_back({x, tip_length + length, z});
        Normals.push_back({x, 0.0f, z});
    }
    for (uint i = 0; i < segments * 2; i += 2) {
        Indices.insert(Indices.end(), {i, i + 1, i + 2, i + 1, i + 3, i + 2});
    }
    const int base_vertex_count = Vertices.size();

    // Cone tip.
    for (uint i = 0; i <= segments; ++i) {
        const float ratio = 2 * float(i) / segments;
        const float x = __cospif(ratio) * tip_radius;
        const float z = __sinpif(ratio) * tip_radius;

        Vertices.push_back({x, tip_length, z});
        Normals.push_back({x, tip_length, z});
    }

    // Tip point.
    Vertices.push_back({0.0f, 0.0f, 0.0f});
    Normals.push_back({0.0f, -1.0f, 0.0f});

    // Tip indices.
    for (uint i = 0; i < segments; ++i) {
        Indices.insert(Indices.end(), {base_vertex_count + i, base_vertex_count + i + 1, base_vertex_count + segments + 1});
    }

    Bind();
}
