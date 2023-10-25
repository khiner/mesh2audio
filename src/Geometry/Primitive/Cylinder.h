#pragma once

#include "Geometry/MeshBuffers.h"

// Extends from y = 0 to y = height.
struct Cylinder : MeshBuffers {
    Cylinder(float radius = 1, float height = 1, uint slices = 32);
};
