#pragma once

#include "Geometry/MeshBuffers.h"

struct Rect : MeshBuffers {
    Rect(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d);
};
