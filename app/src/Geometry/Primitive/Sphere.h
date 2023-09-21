#pragma once

#include "../Geometry.h"

// Icosphere.
struct Sphere : Geometry {
    Sphere(float radius, int recursion_level = 3);
};
