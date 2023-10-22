#pragma once

#include "Geometry/Geometry.h"

// Extends from y = 0 to y = height.
struct Cylinder : Geometry {
    Cylinder(float radius = 1, float height = 1, uint slices = 32);
};
