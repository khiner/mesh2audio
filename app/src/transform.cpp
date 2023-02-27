#include "transform.h"

mat4 Transform::scale(const float &x, const float &y, const float &z) {
    return glm::transpose(mat4(
        x, 0, 0, 0,
        0, y, 0, 0,
        0, 0, z, 0,
        0, 0, 0, 1
    ));
}

mat4 Transform::translate(const float &x, const float &y, const float &z) {
    return glm::transpose(mat4(
        1, 0, 0, x,
        0, 1, 0, y,
        0, 0, 1, z,
        0, 0, 0, 1
    ));
}
