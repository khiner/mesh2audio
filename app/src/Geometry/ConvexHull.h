#pragma once

#include "TriangleBuffers.h"

namespace reactphysics3d {
class ConvexMesh;
}

struct ConvexHull {
    enum Mode {
        QuickHull, // https://github.com/akuukka/quickhull
        RP3D, // reactphysics3d
    };

    static TriangleBuffers Generate(const std::vector<glm::vec3> &points, Mode mode = QuickHull);

    static reactphysics3d::ConvexMesh *GenerateConvexMesh(const std::vector<glm::vec3> &points);
};
