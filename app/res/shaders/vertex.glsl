#version 330 core

uniform mat4 camera_view;
uniform mat4 projection;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in mat4 aInstanceModel;
layout (location = 6) in vec4 aInstanceColor;

out vec3 vertex_normal;
out vec4 vertex_position;
out vec4 instance_color;

void main() {
    vertex_position = aInstanceModel * vec4(aPos, 1.0);
    vertex_normal = mat3(transpose(inverse(aInstanceModel))) * aNormal;
    gl_Position = projection * camera_view * vertex_position;

    instance_color = aInstanceColor;
}
