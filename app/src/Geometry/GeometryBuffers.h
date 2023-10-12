#pragma once

#include "TriangleBuffers.h"

// todo Use OpenMesh as the main mesh data structure (half-edge polygons), and derive all buffer fields from it.
struct GeometryBuffers : TriangleBuffers {
protected:
    std::vector<glm::vec3> Normals;
    std::vector<uint> LineIndices;
};
