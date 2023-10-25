#pragma once

#include "Geometry/GLGeometry.h"

struct Rect : GLGeometry {
    Rect(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d);
};
