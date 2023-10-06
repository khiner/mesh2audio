#include "Rect.h"

Rect::Rect(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) : Geometry() {
    Vertices.append({a, b, c, b, a, d});
    TriangleIndices.append({0, 1, 2, 3, 4, 5});
    Colors.clear();
}
