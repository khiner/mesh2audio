#version 330 core

uniform int num_lights;

layout (location = 0) in vec3 Pos;
layout (location = 1) in vec3 Normal;
layout (location = 2) in vec4 Color;
layout (location = 3) in mat4 Transform;

const int max_num_lights = 5; // Must be a constant. (Can't be a uniform.)
struct Light {
    vec4 position;
    vec4 direction;
    vec4 color;
    mat4 view_projection;
};
layout (std140) uniform LightBlock {
    Light lights[max_num_lights];
};

void main() {
    // For simplicity, we'll use only the first light for shadow mapping.
    gl_Position = lights[0].view_projection * Transform * vec4(Pos, 1.0);
}
