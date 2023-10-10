#pragma once

#include "TriangleBuffers.h"

struct GeometryBuffers : TriangleBuffers {
protected:
    std::vector<glm::vec3> Normals;
    std::vector<uint> LineIndices;
};
