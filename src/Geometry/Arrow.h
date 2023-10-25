#pragma once

#include "GLGeometry.h"

struct Arrow : GLGeometry {
    Arrow(float length, float base_radius, float tip_radius, float tip_length, uint slices = 16);
};
