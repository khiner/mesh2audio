#version 330 core

// Supports per-instance arbitrary 4x4 matrix transform and color.
// Passes outputs to directly to fragment shader.

uniform mat4 camera_view;
uniform mat4 projection;

layout(location = 0) in vec3 Pos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec4 Color;
layout(location = 3) in mat4 Transform;

out vec4 frag_in_position;
out vec3 frag_in_normal;
out vec4 frag_in_color;

void main() {
    frag_in_position = Transform * vec4(Pos, 1.0);
    frag_in_normal = mat3(transpose(inverse(Transform))) * Normal;
    frag_in_color = Color;

    gl_Position = projection * camera_view * frag_in_position;
}
