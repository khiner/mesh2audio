#pragma once

#include "GeometryData.h"

namespace reactphysics3d {
class ConvexMesh;
}

static GeometryData ConvexMeshToGeometryData(reactphysics3d::ConvexMesh *mesh);

struct ConvexHull {
    enum Mode {
        QuickHull, // https://github.com/akuukka/quickhull
        RP3D, // reactphysics3d
    };

    static GeometryData Generate(const std::vector<glm::vec3> &points, Mode mode = QuickHull);

    static reactphysics3d::ConvexMesh *GenerateConvexMesh(const std::vector<glm::vec3> &points);
};
