#include "GridLines.h"

GridLines::GridLines(float size, uint segments) : Geometry() {
    Vertices.append({{1, 1, 0}, {-1, -1, 0}, {-1, 1, 0}, {-1, -1, 0}, {1, 1, 0}, {1, -1, 0}});
    TriangleIndices.append({0, 1, 2, 3, 4, 5});
    Colors.clear();
}
