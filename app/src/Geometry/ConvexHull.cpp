#include "ConvexHull.h"

#include "QuickHull.hpp"
#include <reactphysics3d/reactphysics3d.h>

static rp3d::PhysicsCommon pc{};

static GeometryData ToGeometryData(const quickhull::ConvexHull<float> &hull) {
    const auto &vertices = hull.getVertexBuffer();
    const auto &indices = hull.getIndexBuffer();
    GeometryData data;
    for (const auto &vertex : vertices) data.Vertices.emplace_back(vertex.x, vertex.y, vertex.z);
    for (size_t i = 0; i < indices.size(); i++) data.Indices.emplace_back(indices[i]);
    return data;
}

reactphysics3d::ConvexMesh *ConvexHull::GenerateConvexMesh(const std::vector<glm::vec3> &points) {
    rp3d::VertexArray vertex_array(&points[0], sizeof(glm::vec3), points.size(), rp3d::VertexArray::DataType::VERTEX_FLOAT_TYPE);
    std::vector<rp3d::Message> messages;
    rp3d::ConvexMesh *convex_mesh = pc.createConvexMesh(vertex_array, messages);
    if (convex_mesh == nullptr) {
        std::cerr << "Error while creating a ConvexMesh:" << std::endl;
        for (const auto &message : messages) std::cerr << "Error: " << message.text << std::endl;
    }
    return convex_mesh;
}

GeometryData ConvexMeshToGeometryData(reactphysics3d::ConvexMesh *mesh) {
    // Copy the vertices.
    const auto &half_edge = mesh->getHalfEdgeStructure();
    const uint num_vertices = half_edge.getNbVertices();

    std::vector<glm::vec3> tri_verts; // Return value.
    tri_verts.reserve(num_vertices);
    for (uint i = 0; i < num_vertices; ++i) {
        const auto &vertex = mesh->getVertex(half_edge.getVertex(i).vertexPointIndex);
        tri_verts.emplace_back(vertex.x, vertex.y, vertex.z);
    }

    std::vector<uint> tri_indices; // Return value.
    const uint num_faces = half_edge.getNbFaces();
    for (uint i = 0; i < num_faces; ++i) {
        const auto &face = half_edge.getFace(i);
        if (face.faceVertices.size() < 3) throw std::runtime_error("Invalid face with less than 3 vertices.");

        // Triangulate the polygonal face by fanning from the first vertex.
        for (uint j = 1; j < face.faceVertices.size() - 1; ++j) {
            tri_indices.push_back(face.faceVertices[0]);
            tri_indices.push_back(face.faceVertices[j]);
            tri_indices.push_back(face.faceVertices[j + 1]);
        }
    }

    return {std::move(tri_verts), std::move(tri_indices)};
}

GeometryData ConvexHull::Generate(const std::vector<glm::vec3> &points, Mode mode) {
    if (mode == RP3D) {
        auto *convex_mesh = GenerateConvexMesh(points);
        return ConvexMeshToGeometryData(convex_mesh);
    } else {
        static quickhull::QuickHull<float> qh{}; // Could be double as well.
        return ToGeometryData(qh.getConvexHull(&points[0][0], points.size(), true, false));
    }
}
