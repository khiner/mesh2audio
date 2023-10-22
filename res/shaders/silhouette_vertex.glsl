#version 330 core

uniform mat4 camera_view;
uniform mat4 projection;

layout(location = 0) in vec3 Pos;
layout(location = 1) in vec3 Normal;
// todo get rid of unused color.
layout(location = 2) in vec4 Color;
layout(location = 3) in mat4 Transform;

out mat4 vertex_transform;

void main() {
    vertex_transform = Transform; // Pass the transform along to the geometry shader.

    gl_Position = projection * camera_view * Transform * vec4(Pos, 1.0);
}
