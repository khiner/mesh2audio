#include "Arrow.h"

Arrow::Arrow(float length, float base_radius, float tip_radius, float tip_length, uint segments) : Geometry() {
    // Add all vertices and group handles.
    std::vector<VH> base_bottom_face, base_top_face, tip_face;
    for (uint i = 0; i < segments; i++) {
        const float __angle = 2.0f * float(i) / float(segments);
        const float x = __cospif(__angle);
        const float z = __sinpif(__angle);
        tip_face.push_back(Mesh.add_vertex({x * tip_radius, tip_length, z * tip_radius}));
        base_bottom_face.push_back(Mesh.add_vertex({x * base_radius, tip_length, z * base_radius}));
        base_top_face.push_back(Mesh.add_vertex({x * base_radius, tip_length + length, z * base_radius}));
    }
    auto tip_vhandle = Mesh.add_vertex({0.0f, 0.0f, 0.0f});

    // Quads for the sides of the cylinder.
    for (uint i = 0; i < segments; ++i) {
        Mesh.add_face({
            base_top_face[i],
            base_bottom_face[i],
            base_bottom_face[(i + 1) % segments],
            base_top_face[(i + 1) % segments],
        });
    }

    // N-gons for the top cap of the cylinder and the base cap of the cone.
    Mesh.add_face(base_top_face);
    Mesh.add_face(tip_face);

    // Triangles for the tip cone.
    for (uint i = 0; i < segments; ++i) Mesh.add_face(tip_face[i], tip_vhandle, tip_face[(i + 1) % segments]);

    UpdateBuffersFromMesh();
}
