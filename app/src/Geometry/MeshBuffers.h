#pragma once

#include <filesystem>
#include <iostream>

#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

using uint = unsigned int;

namespace fs = std::filesystem;

enum RenderMode {
    RenderMode_Smooth,
    RenderMode_Lines,
    RenderMode_Points,
};

struct MeshBuffers {
    using MeshType = OpenMesh::PolyMesh_ArrayKernelT<>;
    using VH = OpenMesh::VertexHandle;
    using FH = OpenMesh::FaceHandle;
    using EH = OpenMesh::EdgeHandle;
    using HH = OpenMesh::HalfedgeHandle;
    using Point = OpenMesh::Vec3f;

    MeshBuffers() {
        Mesh.request_vertex_normals();
    }
    MeshBuffers(const fs::path &file_path) { Load(file_path); }
    virtual ~MeshBuffers() = default;

    inline const std::vector<glm::vec3> &GetVertices() const { return Vertices; }
    inline const std::vector<uint> &GetIndices() const { return Indices; }
    inline const std::vector<uint> &GetLineIndices() const { return LineIndices; }
    inline const std::vector<glm::vec3> &GetNormals() const { return Normals; }
    inline const std::vector<uint> &GetIndices(RenderMode mode) const { return mode == RenderMode_Lines ? LineIndices : Indices; }
    inline const glm::vec3 &GetVertex(uint index) const { return Vertices[index]; }
    inline const glm::vec3 &GetNormal(uint index) const { return Normals[index]; }

    inline bool HasNormals() const { return !Normals.empty(); }
    inline bool Empty() const { return Vertices.empty(); }

    bool Load(const fs::path &file_path) {
        // Create an options object
        OpenMesh::IO::Options read_options; // No options used yet, but keeping this here for future use.
        // Load .obj file into OpenMesh data structure
        if (!OpenMesh::IO::read_mesh(Mesh, file_path.string(), read_options)) {
            std::cerr << "Error loading mesh: " << file_path << std::endl;
            return false;
        }
        UpdateBuffersFromMesh();
        return true;
    }

    void Save(const fs::path &file_path) const {
        if (file_path.extension() != ".obj") throw std::runtime_error("Unsupported file type: " + file_path.string());

        if (!OpenMesh::IO::write_mesh(Mesh, file_path.string())) {
            std::cerr << "Error writing mesh to file: " << file_path << std::endl;
        }
    }

    // [{min_x, min_y, min_z}, {max_x, max_y, max_z}]
    std::pair<glm::vec3, glm::vec3> ComputeBounds() const {
        glm::vec3 min_coords(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        );
        glm::vec3 max_coords(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        );

        for (const auto &vh : Mesh.vertices()) {
            const auto &point = Mesh.point(vh);
            min_coords.x = std::min(min_coords.x, point[0]);
            min_coords.y = std::min(min_coords.y, point[1]);
            min_coords.z = std::min(min_coords.z, point[2]);
            max_coords.x = std::max(max_coords.x, point[0]);
            max_coords.y = std::max(max_coords.y, point[1]);
            max_coords.z = std::max(max_coords.z, point[2]);
        }

        return {min_coords, max_coords};
    }

    // Centers the actual points to the center of gravity, not just a transform.
    void Center() {
        auto points = OpenMesh::getPointsProperty(Mesh);
        auto cog = Mesh.vertices().avg(points);
        for (const auto &vh : Mesh.vertices()) {
            const auto &point = Mesh.point(vh);
            Mesh.set_point(vh, point - cog);
        }
        UpdateVertices(); // Normals/indices are not affected.
    }

    void SetOpenMesh(const MeshType &mesh) {
        Clear();
        Mesh = mesh;
        UpdateBuffersFromMesh();
    }

    void ExtrudeProfile(const std::vector<glm::vec2> &profile_vertices, uint slices, bool closed = false);

    void Clear() {
        Mesh.clear();
        Vertices.clear();
        Normals.clear();
        Indices.clear();
        LineIndices.clear();
        Dirty = true;
    }

protected:
    MeshType Mesh; // Default OpenMesh triangles instance variable
    mutable bool Dirty{true};

    void UpdateBuffersFromMesh() {
        UpdateVertices();
        UpdateLineIndices();
        UpdateIndices();
        UpdateNormals();
    }

private:
    void UpdateVertices() {
        Vertices.clear();
        for (const auto &vh : Mesh.vertices()) {
            const auto &point = Mesh.point(vh);
            Vertices.emplace_back(point[0], point[1], point[2]);
        }
        Dirty = true;
    }

    void UpdateIndices() {
        Indices.clear();
        // Copy and triangulate the mesh to calculate the triangle indices.
        auto triangulated_mesh = Mesh;
        triangulated_mesh.triangulate();
        for (const auto &fh : triangulated_mesh.faces()) {
            auto v_it = triangulated_mesh.cfv_iter(fh);
            Indices.push_back(v_it->idx());
            Indices.push_back((++v_it)->idx());
            Indices.push_back((++v_it)->idx());
        }
        Dirty = false;
    }
    void UpdateLineIndices() {
        LineIndices.clear();
        // Get all unique polygon lines.
        for (const auto &eh : Mesh.edges()) {
            const auto heh = Mesh.halfedge_handle(eh, 0);
            LineIndices.push_back(Mesh.from_vertex_handle(heh).idx());
            LineIndices.push_back(Mesh.to_vertex_handle(heh).idx());
        }
        // Each line around each face for now.
        // for (const auto &fh : Mesh.faces()) {
        //     auto v_it = Mesh.cfv_iter(fh);
        //     for (size_t i = 0; i < Mesh.valence(fh); i++) {
        //         const auto &first_vertex = *v_it;
        //         const auto &last_vertex = *(++v_it);
        //         LineIndices.push_back(first_vertex.idx());
        //         LineIndices.push_back(last_vertex.idx());
        //     }
        // }
        Dirty = true;
    }

    void UpdateNormals() {
        Mesh.request_vertex_normals();
        Mesh.update_normals();
        Normals.clear();
        for (const auto &vh : Mesh.vertices()) {
            const auto &normal = Mesh.normal(vh);
            Normals.emplace_back(normal[0], normal[1], normal[2]);
        }
        Mesh.release_vertex_normals();
        Dirty = true;
    }

    // todo continue to get rid of all direct modifications to raw buffers.

private:
    std::vector<glm::vec3> Vertices;
    std::vector<uint> Indices;
    std::vector<glm::vec3> Normals;
    std::vector<uint> LineIndices;
};
