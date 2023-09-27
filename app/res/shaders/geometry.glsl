#version 330 core

uniform float line_width;
uniform int draw_lines;

layout (triangles) in;
layout (triangle_strip, max_vertices = 12) out;

in vec3 vertex_normal[];
in vec4 vertex_position[];
in vec4 instance_color[];

out vec3 geom_vertex_normal;
out vec4 geom_vertex_position;
out vec4 geom_instance_color;

void EmitVertexWithAttributes(int i, vec4 offset) {
    geom_vertex_normal = vertex_normal[i];
    geom_vertex_position = vertex_position[i] + offset;
    geom_instance_color = instance_color[i];
    gl_Position = gl_in[i].gl_Position + offset;
    EmitVertex();
}

void EmitLineQuad(int i0, int i1) {
    vec4 dir = vertex_position[i1] - vertex_position[i0];
    vec4 offset = normalize(vec4(-dir.y, dir.x, 0.0, 0.0)) * line_width * 0.5;

    EmitVertexWithAttributes(i0, offset);
    EmitVertexWithAttributes(i1, offset);
    EmitVertexWithAttributes(i1, -offset);
    EmitVertexWithAttributes(i0, -offset);
    
    EndPrimitive();
}

void main() {
    if (draw_lines == 0) {
        // No-op path, just forward attributes along to the fragment shader.
        for (int i = 0; i < 3; ++i) {
            EmitVertexWithAttributes(i, vec4(0.0));
        }
        EndPrimitive();
    } else {
        // Draw triangle lines as quads.
        EmitLineQuad(0, 1);
        EmitLineQuad(1, 2);
        EmitLineQuad(2, 0);
    }
}
