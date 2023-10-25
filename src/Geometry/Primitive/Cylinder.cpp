#include "Cylinder.h"

Cylinder::Cylinder(float radius, float height, uint slices) : MeshBuffers() {
    // Add all vertices and group handles.
    std::vector<VH> bottom_face, top_face;
    for (uint i = 0; i < slices; i++) {
        const float __angle = 2.0f * float(i) / float(slices);
        const float x = __cospif(__angle);
        const float z = __sinpif(__angle);
        bottom_face.push_back(Mesh.add_vertex({x * radius, 0, z * radius}));
        top_face.push_back(Mesh.add_vertex({x * radius, height, z * radius}));
    }

    Mesh.add_face(bottom_face);
    // Quads for the sides of the cylinder.
    for (uint i = 0; i < slices; ++i) {
        Mesh.add_face({
            bottom_face[i],
            top_face[i],
            top_face[(i + 1) % slices],
            bottom_face[(i + 1) % slices],
        });
    }

    std::reverse(top_face.begin(), top_face.end()); // For consistent winding order.
    Mesh.add_face(top_face);

    UpdateBuffersFromMesh();
}
