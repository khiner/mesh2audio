#include <iostream>
#include <numbers>

#include "transform.h"

const float PI = std::numbers::pi;

mat4 Transform::scale(const float &sx, const float &sy, const float &sz) {
    return glm::transpose(mat4(
        sx, 0, 0, 0,
        0, sy, 0, 0,
        0, 0, sz, 0,
        0, 0, 0, 1
    ));
}

mat4 Transform::translate(const float &tx, const float &ty, const float &tz) {
    return glm::transpose(mat4(
        1, 0, 0, tx,
        0, 1, 0, ty,
        0, 0, 1, tz,
        0, 0, 0, 1
    ));
}
