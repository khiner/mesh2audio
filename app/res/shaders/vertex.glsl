#version 330 core

// Vertex array layout inputs to the vertex shader.
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in mat4 aInstanceModel;
layout (location = 6) in vec4 aInstanceColor;

// Uniform variables.
uniform mat4 model_view;
uniform mat4 projection;

// Outputs to pass data to the fragment shader.
out vec3 vertex_normal;
out vec4 vertex_position;
out vec4 instance_color;

void main() {
    vertex_position = aInstanceModel * vec4(aPos, 1.0);
    gl_Position = projection * model_view * vertex_position;
    // This simplified normal calculation assumes the `aInstanceModel` transformation only scales and rotates,
    // which is currently true in this application.
    // If we introduce scaling or skewing, we'll need to use the more general form below.
    vertex_normal = mat3(aInstanceModel) * aNormal;
    // vertex_normal = mat3(transpose(inverse(aInstanceModel))) * aNormal;

    instance_color = aInstanceColor;
}
