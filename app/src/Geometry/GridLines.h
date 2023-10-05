#pragma once

#include "Geometry.h"

struct GridLines : Geometry {
    GridLines(float size = 1, uint segments = 16);
};
