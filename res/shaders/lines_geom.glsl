#version 330 core

// Emits a quad around each line.

uniform float line_width;

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

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
    // Compute line direction and perpendicular vector in screen space.
    vec2 dir = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w - gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
    vec4 offset = normalize(vec4(-dir.y, dir.x, 0.0, 0.0)) * line_width * 0.5;

    EmitWithAttributes(0, offset);
    EmitWithAttributes(1, offset);
    EmitWithAttributes(0, -offset);
    EmitWithAttributes(1, -offset);

    EndPrimitive();
}
