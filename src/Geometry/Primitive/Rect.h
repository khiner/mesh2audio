#pragma once

#include "Geometry/Geometry.h"

struct Rect : Geometry {
    Rect(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d);
};
