#version 330 core

// Emits a quad around each line.

uniform float line_width;
uniform mat4 camera_view;
uniform mat4 projection;

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
    // Transform vertices to screen space and compute line direction and perpendicular vector.
    vec4 screen_pos0 = projection * camera_view * vertex_position[0];
    vec4 screen_pos1 = projection * camera_view * vertex_position[1];
    vec2 dir = screen_pos1.xy / screen_pos1.w - screen_pos0.xy / screen_pos0.w;
    vec4 offset = normalize(vec4(-dir.y, dir.x, 0.0, 0.0)) * line_width * 0.5;

    EmitWithAttributes(0, offset);
    EmitWithAttributes(1, offset);
    EmitWithAttributes(0, -offset);
    EmitWithAttributes(1, -offset);

    EndPrimitive();
}
