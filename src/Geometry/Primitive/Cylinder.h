#pragma once

#include "Geometry/GLGeometry.h"

// Extends from y = 0 to y = height.
struct Cylinder : GLGeometry {
    Cylinder(float radius = 1, float height = 1, uint slices = 32);
};
