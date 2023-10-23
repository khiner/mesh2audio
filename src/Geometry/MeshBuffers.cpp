#include "MeshBuffers.h"

#include <glm/geometric.hpp>

uint MeshBuffers::FindVertextNearestTo(const glm::vec3 point) const {
    uint nearest_index = 0;
    float nearest_distance = std::numeric_limits<float>::max();
    for (uint i = 0; i < NumVertices(); i++) {
        const float distance = glm::distance(point, GetVertex(i));
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest_index = i;
        }
    }
    return nearest_index;
}

std::vector<uint> MeshBuffers::GenerateTriangleIndices() const {
    auto triangulated_mesh = Mesh; // `triangulate` is in-place, so we need to make a copy.
    triangulated_mesh.triangulate();
    std::vector<uint> indices;
    for (const auto &fh : triangulated_mesh.faces()) {
        auto v_it = triangulated_mesh.cfv_iter(fh);
        indices.insert(indices.end(), {uint(v_it->idx()), uint((++v_it)->idx()), uint((++v_it)->idx())});
    }
    return indices;
}

std::vector<uint> MeshBuffers::GenerateTriangulatedFaceIndices() const {
    std::vector<uint> indices;
    uint index = 0;
    for (const auto &fh : Mesh.faces()) {
        auto valence = Mesh.valence(fh);
        for (uint i = 0; i < valence - 2; ++i) {
            indices.insert(indices.end(), {index, index + i + 1, index + i + 2});
        }
        index += valence;
    }
    return indices;
}

std::vector<uint> MeshBuffers::GenerateLineIndices() const {
    std::vector<uint> indices;
    indices.reserve(Mesh.n_edges() * 2);
    for (const auto &eh : Mesh.edges()) {
        const auto heh = Mesh.halfedge_handle(eh, 0);
        indices.push_back(Mesh.from_vertex_handle(heh).idx());
        indices.push_back(Mesh.to_vertex_handle(heh).idx());
    }
    return indices;
}

void MeshBuffers::ExtrudeProfile(const std::vector<glm::vec2> &profile_vertices, uint slices, bool closed) {
    Clear();
    if (profile_vertices.size() < 3) return;

    // The profile vertices are ordered clockwise, with the first vertex corresponding to the top/outside of the surface,
    // and last vertex corresponding the the bottom/inside of the surface.
    // If the profile is not closed (default), these top/bottom vertices will be connected in the middle of the extruded geometry,
    // creating a continuous connected solid "bridge" between all rotated slices.
    // todo bring back closing quads.
    const int n = profile_vertices.size();
    const int start_index = closed ? 0 : 1;
    const int end_index = n - (closed ? 0 : 1);
    const int profile_size_no_connect = end_index - start_index;
    const int num_vertices = slices * profile_size_no_connect + (closed ? 0 : 2);

    Mesh.reserve(num_vertices, 0, 0); // vertices, edges, faces. todo edges & faces.
    std::vector<VH> top_face, bottom_face;
    for (uint slice = 0; slice < slices; slice++) {
        const float __angle = 2 * float(slice) / slices;
        const float c = __cospif(__angle);
        const float s = __sinpif(__angle);
        // Exclude the top/bottom vertices, which will be connected later.
        for (int i = start_index; i < end_index; i++) {
            const auto &p = profile_vertices[i];
            auto vh = Mesh.add_vertex({p.x * c, p.y, p.x * s});
            if (!closed && i == end_index - 1) top_face.push_back(vh);
            else if (!closed && i == start_index) bottom_face.push_back(vh);
        }
    }

    for (uint slice = 0; slice < slices; slice++) {
        for (int i = 0; i < profile_size_no_connect - 1; i++) {
            const uint base_index = slice * profile_size_no_connect + i;
            const uint next_base_index = ((slice + 1) % slices) * profile_size_no_connect + i;
            Mesh.add_face(Mesh.vertex_handle(base_index + 1), Mesh.vertex_handle(next_base_index + 1), Mesh.vertex_handle(next_base_index), Mesh.vertex_handle(base_index));
        }
    }
    if (!closed) {
        std::reverse(top_face.begin(), top_face.end()); // For consistent winding order.
        Mesh.add_face(top_face);
        Mesh.add_face(bottom_face);
    }

    Center();

    // SVG coordinates are upside-down relative to our 3D rendering coordinates.
    // However, they're correctly oriented top-to-bottom for 2D ImGui rendering, so we only invert the y-axis (the up/down axis).
    for (const auto &vh : Mesh.vertices()) {
        const auto &p = Mesh.point(vh);
        Mesh.set_point(vh, {p[0], -p[1], p[2]});
    }
    UpdateBuffersFromMesh();
}
