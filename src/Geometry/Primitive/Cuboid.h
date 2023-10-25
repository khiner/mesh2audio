#pragma once

#include "Geometry/GLGeometry.h"

struct Cuboid : GLGeometry {
    Cuboid(glm::vec3 half_extents = {1, 1, 1});
};
