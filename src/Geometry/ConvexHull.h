#pragma once

#include "MeshBuffers.h"

namespace reactphysics3d {
class ConvexMesh;
}

struct ConvexHull {
    enum Mode {
        QuickHull, // https://github.com/akuukka/quickhull
        RP3D, // reactphysics3d
    };

    static OpenMesh::PolyMesh_ArrayKernelT<> Generate(const std::vector<glm::vec3> &points, Mode mode = QuickHull);

    static reactphysics3d::ConvexMesh *GenerateConvexMesh(const std::vector<glm::vec3> &points);
};
