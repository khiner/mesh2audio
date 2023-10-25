#pragma once

#include "Geometry.h"

namespace reactphysics3d {
class ConvexMesh;
}

struct ConvexHull {
    enum Mode {
        QuickHull, // https://github.com/akuukka/quickhull
        RP3D, // reactphysics3d
    };

    static OpenMesh::PolyMesh_ArrayKernelT<> Generate(const float *points, size_t num_points, Mode mode = QuickHull);

    static reactphysics3d::ConvexMesh *GenerateConvexMesh(const float *points, size_t num_points);
};
