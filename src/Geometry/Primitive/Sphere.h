#pragma once

#include "Geometry/GLGeometry.h"

// Icosphere.
struct Sphere : GLGeometry {
    Sphere(float radius = 1, int recursion_level = 3);
};
