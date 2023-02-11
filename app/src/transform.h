#include <glm/glm.hpp>

using glm::mat3, glm::mat4, glm::vec3, glm::vec4;

namespace gl {
class Transform {
public:
    static void left(float degrees, vec3 &eye, vec3 &up);
    static void up(float degrees, vec3 &eye, vec3 &up);
    static mat3 rotate(const float degrees, const vec3 &axis);
    static mat4 scale(const float &sx, const float &sy, const float &sz);
    static mat4 translate(const float &tx, const float &ty, const float &tz);
    static vec3 upvector(const vec3 &up, const vec3 &zvec);
};
} // namespace gl
