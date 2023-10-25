#pragma once

#include "Geometry/MeshBuffers.h"

struct Arrow : MeshBuffers {
    Arrow(float length, float base_radius, float tip_radius, float tip_length, uint slices = 16);
};
