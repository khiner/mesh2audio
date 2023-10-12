#include "MeshBuffers.h"

void MeshBuffers::ExtrudeProfile(const std::vector<glm::vec2> &profile_vertices, uint slices, bool closed) {
    Clear();
    if (profile_vertices.size() < 3) return;

    Mesh.request_vertex_normals();
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
    std::vector<OpenMesh::VertexHandle> top_face;
    std::vector<OpenMesh::VertexHandle> bottom_face;
    for (uint slice = 0; slice < slices; slice++) {
        const float __angle = 2 * float(slice) / slices;
        const float c = __cospif(__angle);
        const float s = __sinpif(__angle);
        // Exclude the top/bottom vertices, which will be connected later.
        for (int i = start_index; i < end_index; i++) {
            const auto &p = profile_vertices[i];
            auto handle = Mesh.add_vertex({p.x * c, p.y, p.x * s});
            Mesh.set_normal(handle, {c, 0.f, s});

            if (!closed && i == end_index - 1) top_face.push_back(handle);
            else if (!closed && i == start_index) bottom_face.push_back(handle);
        }
    }

    for (uint slice = 0; slice < slices; slice++) {
        for (int i = 0; i < profile_size_no_connect - 1; i++) {
            const uint base_index = slice * profile_size_no_connect + i;
            const uint next_base_index = ((slice + 1) % slices) * profile_size_no_connect + i;
            Mesh.add_face(Mesh.vertex_handle(base_index), Mesh.vertex_handle(next_base_index), Mesh.vertex_handle(next_base_index + 1), Mesh.vertex_handle(base_index + 1));
        }
    }
    std::reverse(bottom_face.begin(), bottom_face.end()); // For consistent winding order.
    if (!closed) {
        Mesh.add_face(top_face);
        Mesh.add_face(bottom_face);
    }

    // Mesh.triangulate();

    Center();
    // SVG coordinates are upside-down relative to our 3D rendering coordinates.
    // However, they're correctly oriented top-to-bottom for 2D ImGui rendering, so we only invert the y-axis (the up/down axis).
    for (const auto &vh : Mesh.vertices()) {
        const auto &point = Mesh.point(vh);
        Mesh.set_point(vh, {point[0], -point[1], point[2]});
    }
    UpdateBuffersFromMesh();
}
