#include "Wireframe.h"

#include "Primitive/Cylinder.h"

#include <glm/gtx/quaternion.hpp>

using MB = MeshBuffers;

using glm::vec3, glm::vec4, glm::mat4;

namespace WireframeTopology {
struct Cylinder : TransformedGeometry {
    Cylinder(const MeshType &wireframe_mesh, const EH &edge, float radius, uint slices) {
        const auto heh = wireframe_mesh.halfedge_handle(edge, 0);
        const vec3 from = ToGlm(Mesh.point(Mesh.from_vertex_handle(heh)));
        const vec3 to = ToGlm(Mesh.point(Mesh.to_vertex_handle(heh)));

        std::vector<VH> bottom_face, top_face;
        for (uint i = 0; i < slices; i++) {
            const float __angle = 2.0f * float(i) / float(slices);
            const float x = __cospif(__angle);
            const float z = __sinpif(__angle);
            bottom_face.push_back(Mesh.add_vertex({x * radius, 0, z * radius}));
            top_face.push_back(Mesh.add_vertex({x * radius, 1.f, z * radius}));
        }

        Mesh.add_face(bottom_face);
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

        // Create the transform matrix.
        Transform = glm::translate(I, from);

        vec3 dir = to - from;
        const float height = glm::length(dir);
        dir = glm::normalize(dir);
        const vec3 axis = glm::cross(Up, dir);
        if (glm::length(axis) > 0) Transform = glm::rotate(Transform, acos(glm::dot(Up, dir)), axis);

        Transform = glm::scale(Transform, vec3{1, height, 1});

        UpdateBuffersFromMesh();
    }
};

struct Junction : TransformedGeometry {
    Junction(MeshType &wireframe_mesh, const VH &vertex, float radius, uint slices) {
        // Assume the input mesh is a cuboid and the input vertex is a corner vertex.
        // Each incoming edge represents one of the three orthogonal directions.

        // Iterate over all incoming halfedges to gather the direction vectors.
        std::vector<vec3> directions;
        for (auto vih_it = wireframe_mesh.vih_iter(vertex); vih_it.is_valid(); ++vih_it) {
            const auto heh = *vih_it;
            const vec3 from = ToGlm(wireframe_mesh.point(wireframe_mesh.from_vertex_handle(heh)));
            const vec3 to = ToGlm(wireframe_mesh.point(wireframe_mesh.to_vertex_handle(heh)));
            directions.push_back(glm::normalize(to - from));
        }

        // Create three cylindrical openings in the local mesh for each direction.
        for (const vec3 &dir : directions) {
            // Calculate the rotation required to align the default cylinder orientation (along Y-axis) to the direction vector.
            const vec3 axis = glm::cross(Up, dir);
            const float angle = glm::acos(glm::dot(Up, dir));
            // Create a local transformation matrix for this cylindrical opening.
            const mat4 local_transform = glm::rotate(I, angle, axis);
            // Add a cylinder at this opening.
            CreateCylinder(local_transform, radius, slices);
        }

        UpdateBuffersFromMesh();
    }

    void CreateCylinder(const mat4 &transform, float radius, uint slices) {
        std::vector<VH> bottom_face, top_face;
        for (uint i = 0; i < slices; i++) {
            const float __angle = 2.0f * float(i) / float(slices);
            const float x = __cospif(__angle);
            const float z = __sinpif(__angle);
            const vec3 bottom_pos = transform * vec4(x * radius, 0, z * radius, 1);
            const vec3 top_pos = transform * vec4(x * radius, 1.f, z * radius, 1);
            bottom_face.push_back(Mesh.add_vertex(ToPoint(bottom_pos)));
            top_face.push_back(Mesh.add_vertex(ToPoint(top_pos)));
        }

        Mesh.add_face(bottom_face);
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
    }
};
} // namespace WireframeTopology

Wireframe::Wireframe(Mesh *parent_mesh) : ParentMesh(parent_mesh) {
    static const float CylinderRadius = 1.f;
    static const uint CylinderSlices = 32;

    MeshBuffers::MeshType &parent_mesh_inner = parent_mesh->GetTriangles().GetMesh();
    for (const auto &vh : parent_mesh_inner.vertices()) {
        Meshes.emplace_back(WireframeTopology::Junction{parent_mesh_inner, vh, CylinderRadius, CylinderSlices}, ParentMesh);
    }
    // for (const auto &eh : parent_mesh_inner.edges()) {
    //     Meshes.emplace_back(WireframeTopology::Cylinder{parent_mesh_inner, eh, CylinderRadius, CylinderSlices}, ParentMesh);
    // }
}
