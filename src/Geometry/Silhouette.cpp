#include "Silhouette.h"

#include <OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh>
#include <glm/glm.hpp>

using glm::vec3, glm::vec4, glm::mat3, glm::mat4;

static glm::vec3 ToGlm(const OpenMesh::Vec3f &v) { return {v[0], v[1], v[2]}; }

std::vector<MeshType::EdgeHandle> FindSilhouetteEdges(const MeshType &mesh, const mat4 &mesh_transform, const glm::vec3 &camera_pos) {
    std::vector<MeshType::EdgeHandle> silhouette_edges;
    mat3 inv_transform = mat3(glm::transpose(glm::inverse(mesh_transform)));
    for (const auto &eh : mesh.edges()) {
        if (mesh.is_boundary(eh)) continue;

        const auto &heh = mesh.halfedge_handle(eh, 0);
        const auto &fh1 = mesh.face_handle(heh);
        const auto &fh2 = mesh.opposite_face_handle(heh);

        const vec3 fn1 = inv_transform * ToGlm(mesh.normal(fh1));
        const vec3 fn2 = inv_transform * ToGlm(mesh.normal(fh2));
        const vec3 fc1 = mesh_transform * vec4{ToGlm(mesh.calc_face_centroid(fh1)), 1};
        const vec3 fc2 = mesh_transform * vec4{ToGlm(mesh.calc_face_centroid(fh2)), 1};

        const float dp1 = glm::dot(fn1, camera_pos - fc1);
        const float dp2 = glm::dot(fn2, camera_pos - fc2);
        if ((dp1 > 0 && dp2 < 0) || (dp1 < 0 && dp2 > 0)) {
            silhouette_edges.push_back(eh);
        }
    }

    return silhouette_edges;
}
