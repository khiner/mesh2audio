#version 330 core

// Vertex array layout inputs to the vertex shader.
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// Uniform variables.
uniform mat4 model_view;
uniform mat4 projection;

// Outputs to pass data to the fragment shader.
out vec3 vertex_normal;
out vec4 vertex_position;

void main() {
    gl_Position = projection * model_view * vec4(aPos, 1.0);
    vertex_normal = aNormal;
    vertex_position = vec4(aPos, 1.0);
}
