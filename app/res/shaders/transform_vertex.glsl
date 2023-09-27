#version 330 core

// Supports per-instance arbitrary 4x4 matrix transform and color.
// Passes outputs to geometry shader for line-drawing support.

uniform mat4 camera_view;
uniform mat4 projection;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aColor;
layout (location = 3) in mat4 aTransform;

out vec3 vertex_normal;
out vec4 vertex_position;
out vec4 vertex_color;

void main() {
    vertex_position = aTransform * vec4(aPos, 1.0);
    vertex_normal = mat3(transpose(inverse(aTransform))) * aNormal;
    vertex_color = aColor;

    gl_Position = projection * camera_view * vertex_position;
}
