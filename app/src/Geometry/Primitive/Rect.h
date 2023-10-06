#pragma once

#include "../Geometry.h"

struct Rect : Geometry {
    Rect(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 normal = {0, 0, 1});
};
