#pragma once

#include "Geometry/MeshBuffers.h"

struct Cuboid : MeshBuffers {
    Cuboid(glm::vec3 half_extents = {1, 1, 1});
};
