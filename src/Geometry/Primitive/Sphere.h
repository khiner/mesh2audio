#pragma once

#include "Geometry/MeshBuffers.h"

// Icosphere.
struct Sphere : MeshBuffers {
    Sphere(float radius = 1, int recursion_level = 3);
};
