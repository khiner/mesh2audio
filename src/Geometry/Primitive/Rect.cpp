#include "Rect.h"

using glm::vec3;

Rect::Rect(vec3 a, vec3 b, vec3 c, vec3 d) : Geometry() {
    const std::array<vec3, 4> vertices = {a, b, c, d};
    for (const auto &vertex : vertices) Mesh.add_vertex({vertex.x, vertex.y, vertex.z});
    Mesh.add_face({Mesh.vertex_handle(0), Mesh.vertex_handle(1), Mesh.vertex_handle(2), Mesh.vertex_handle(3)});

    UpdateBuffersFromMesh();
}
