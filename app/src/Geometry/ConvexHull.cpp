#include "ConvexHull.h"

#include "QuickHull.hpp"
#include <reactphysics3d/reactphysics3d.h>

static rp3d::PhysicsCommon pc{};

static OpenMesh::PolyMesh_ArrayKernelT<> ToOpenMesh(const quickhull::ConvexHull<float> &hull) {
    OpenMesh::PolyMesh_ArrayKernelT<> open_mesh;

    const auto &vertices = hull.getVertexBuffer();
    const auto &indices = hull.getIndexBuffer();
    for (const auto &vertex : vertices) open_mesh.add_vertex({vertex.x, vertex.y, vertex.z});

    for (size_t i = 0; i < indices.size(); i += 3) {
        open_mesh.add_face({open_mesh.vertex_handle(indices[i]), open_mesh.vertex_handle(indices[i + 1]), open_mesh.vertex_handle(indices[i + 2])});
    }
    return open_mesh;
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

static OpenMesh::PolyMesh_ArrayKernelT<> ConvexMeshToOpenMesh(reactphysics3d::ConvexMesh *mesh) {
    // Copy the vertices.
    const auto &half_edge = mesh->getHalfEdgeStructure();
    const uint num_vertices = half_edge.getNbVertices();

    OpenMesh::PolyMesh_ArrayKernelT<> open_mesh; // Return value.
    open_mesh.reserve(num_vertices, half_edge.getNbHalfEdges(), half_edge.getNbFaces()); // vertices, edges, faces.
    for (uint i = 0; i < num_vertices; ++i) {
        const auto &vertex = mesh->getVertex(half_edge.getVertex(i).vertexPointIndex);
        open_mesh.add_vertex({vertex.x, vertex.y, vertex.z});
    }

    const uint num_faces = half_edge.getNbFaces();
    for (uint i = 0; i < num_faces; ++i) {
        const auto &face = half_edge.getFace(i);
        if (face.faceVertices.size() < 3) throw std::runtime_error("Invalid face with less than 3 vertices.");

        std::vector<MeshBuffers::VH> open_mesh_face(face.faceVertices.size());
        for (size_t j = 0; j < face.faceVertices.size(); ++j) {
            const auto &vertex = face.faceVertices[j];
            open_mesh_face[j] = open_mesh.vertex_handle(vertex);
        }
        open_mesh.add_face(open_mesh_face);
    }

    return open_mesh;
}

OpenMesh::PolyMesh_ArrayKernelT<> ConvexHull::Generate(const std::vector<glm::vec3> &points, Mode mode) {
    if (mode == RP3D) {
        auto *convex_mesh = GenerateConvexMesh(points);
        return ConvexMeshToOpenMesh(convex_mesh);
    } else {
        static quickhull::QuickHull<float> qh{}; // Could be double as well.
        return ToOpenMesh(qh.getConvexHull(&points[0][0], points.size(), true, false));
    }
}
