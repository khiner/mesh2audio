#version 330 core

// Inputs from vertex shader
layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out; // Max 6 vertices per quad, 3 edges -> 18 vertices

uniform float line_width;
uniform int draw_lines;

in vec3 vertex_normal[];
in vec4 vertex_position[];
in vec4 instance_color[];

out vec3 geom_vertex_normal;
out vec4 geom_vertex_position;
out vec4 geom_instance_color;

void EmitLineQuad(vec4 p0, vec4 p1) {
    vec4 dir = p1 - p0;
    vec4 offset = normalize(vec4(-dir.y, dir.x, 0.0, 0.0)) * line_width * 0.5;

    geom_vertex_normal = vertex_normal[0];
    geom_vertex_position = p0 + offset;
    geom_instance_color = instance_color[0];
    gl_Position = gl_in[0].gl_Position + offset;
    EmitVertex();

    geom_vertex_normal = vertex_normal[1];
    geom_vertex_position = p1 + offset;
    geom_instance_color = instance_color[1];
    gl_Position = gl_in[1].gl_Position + offset;
    EmitVertex();

    geom_vertex_normal = vertex_normal[1];
    geom_vertex_position = p1 - offset;
    geom_instance_color = instance_color[1];
    gl_Position = gl_in[1].gl_Position - offset;
    EmitVertex();

    geom_vertex_normal = vertex_normal[0];
    geom_vertex_position = p0 - offset;
    geom_instance_color = instance_color[0];
    gl_Position = gl_in[0].gl_Position - offset;
    EmitVertex();
    
    // Reuse the first two vertices to form a quad
    gl_Position = gl_in[0].gl_Position + offset;
    EmitVertex();
    
    gl_Position = gl_in[1].gl_Position + offset;
    EmitVertex();

    EndPrimitive();
}

void main() {
    if (draw_lines == 0) {
        // No-op path. just forward attributes along to the fragment shader.
        for (int i = 0; i < 3; ++i) {
            geom_vertex_normal = vertex_normal[i];
            geom_vertex_position = vertex_position[i];
            geom_instance_color = instance_color[i];

            gl_Position = gl_in[i].gl_Position;

            EmitVertex();
        }
        EndPrimitive();
    } else {
        // Draw lines as quads.
        EmitLineQuad(vertex_position[0], vertex_position[1]);
        EmitLineQuad(vertex_position[1], vertex_position[2]);
        EmitLineQuad(vertex_position[2], vertex_position[0]);
    }
}
