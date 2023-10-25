#pragma once

#include "Geometry.h"

struct Arrow : Geometry {
    Arrow(float length, float base_radius, float tip_radius, float tip_length, uint slices = 16);
};
