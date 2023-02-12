#include <glm/glm.hpp>

using glm::mat4;
struct Transform {
    static mat4 scale(const float &sx, const float &sy, const float &sz);
    static mat4 translate(const float &tx, const float &ty, const float &tz);
};
