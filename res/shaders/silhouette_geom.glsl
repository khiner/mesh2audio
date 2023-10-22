#version 410 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform float line_width;
uniform mat4 camera_view;
uniform mat4 vertex_transform[2];

layout(std430, binding = 0) buffer FaceNormalData {
    vec3 face_normals[];
};

layout(std430, binding = 1) buffer FaceCentroidData {
    vec3 face_centroids[];
};

layout(std430, binding = 2) buffer EdgeToFaceData {
    ivec2 edge_to_faces[];
};

void Emit(int i, vec4 offset) {
    gl_Position = gl_in[i].gl_Position + offset;
    EmitVertex();
}

void main() {
    mat3 inv_transform = mat3(transpose(inverse(vertex_transform[0]))); // todo precompute inverse transform and send to vertex shader.
    vec3 camera_pos = inverse(camera_view)[3].xyz;

    // Get the face normals and centroids.
    vec3 fn1 = inv_transform * face_normals[int(edge_to_faces[gl_PrimitiveIDIn].x)];
    vec3 fn2 = inv_transform * face_normals[int(edge_to_faces[gl_PrimitiveIDIn].y)];
    vec3 fc1 = vertex_transform[0] * vec4(face_centroids[int(edge_to_faces[gl_PrimitiveIDIn].x)], 1.0);
    vec3 fc2 = vertex_transform[1] * vec4(face_centroids[int(edge_to_faces[gl_PrimitiveIDIn].y)], 1.0);

    float dp1 = dot(fn1, camera_pos - fc1);
    float dp2 = dot(fn2, camera_pos - fc2);
    if ((dp1 > 0 && dp2 < 0) || (dp1 < 0 && dp2 > 0)) {
        // This is a silhouette edge. Emit vertices for rendering.

        // Compute line direction and perpendicular vector in screen space.
        vec2 dir = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w - gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
        vec4 offset = normalize(vec4(-dir.y, dir.x, 0.0, 0.0)) * line_width * 0.5;

        Emit(0, offset);
        Emit(1, offset);
        Emit(0, -offset);
        Emit(1, -offset);

        EndPrimitive();
    }
}
