#include "Rect.h"

using glm::vec3;

Rect::Rect(vec3 a, vec3 b, vec3 c, vec3 d, vec3 normal) : Geometry() {
    Vertices.insert(Vertices.end(), {a, b, c, b, a, d});
    TriangleIndices.insert(TriangleIndices.end(), {0, 1, 2, 3, 4, 5});
    Normals.insert(Normals.end(), {normal, normal, normal, normal, normal, normal});
}
