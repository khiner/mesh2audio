#pragma once

#include "../Geometry.h"

// Icosphere.
struct Sphere : Geometry {
    Sphere(float radius = 1, int recursion_level = 3);
};
