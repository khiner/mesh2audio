#version 330 core

// Emits quad lines around every triangle.

uniform float line_width;

layout (lines) in;
layout (triangle_strip, max_vertices = 4) out;

in vec3 vertex_normal[];
in vec4 vertex_position[];
in vec4 vertex_color[];

out vec4 frag_in_position;
out vec3 frag_in_normal;
out vec4 frag_in_color;

void EmitWithAttributes(int i, vec4 offset) {
    frag_in_normal = vertex_normal[i];
    frag_in_position = vertex_position[i] + offset;
    frag_in_color = vertex_color[i];
    gl_Position = gl_in[i].gl_Position + offset;
    EmitVertex();
}

void main() {
    // Draw line as quad.
    vec2 dir = vertex_position[1].xy - vertex_position[0].xy;
    vec4 offset = normalize(vec4(-dir.y, dir.x, 0.0, 0.0)) * line_width * 0.5;

    EmitWithAttributes(0, offset);
    EmitWithAttributes(1, offset);
    EmitWithAttributes(1, -offset);
    EmitWithAttributes(0, -offset);
    
    EndPrimitive();
}
