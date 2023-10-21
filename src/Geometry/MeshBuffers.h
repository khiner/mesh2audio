#pragma once

#include <filesystem>

#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

using uint = unsigned int;

namespace fs = std::filesystem;

enum class RenderMode {
    Flat,
    Smooth,
    Lines,
    Points,
};

struct MeshBuffers {
    using MeshType = OpenMesh::PolyMesh_ArrayKernelT<>;
    using VH = OpenMesh::VertexHandle;
    using FH = OpenMesh::FaceHandle;
    using EH = OpenMesh::EdgeHandle;
    using HH = OpenMesh::HalfedgeHandle;
    using Point = OpenMesh::Vec3f;

    MeshBuffers() {
    }
    MeshBuffers(const fs::path &file_path) { Load(file_path); }
    virtual ~MeshBuffers() = default;

    void PrepareRender(RenderMode mode) {
        if (ActiveRenderMode == mode) return;
        ActiveRenderMode = mode;
        UpdateBuffersFromMesh();
    }

    inline const MeshType &GetMesh() const { return Mesh; }

    inline const std::vector<glm::vec3> &GetVertices() const { return UniqueVertices; }
    inline const glm::vec3 &GetVertex(uint index) const { return UniqueVertices[index]; }
    inline const glm::vec3 &GetNormal(uint index) const { return UniqueNormals[index]; }

    inline uint NumVertices() const { return UniqueVertices.size(); }
    inline uint NumIndices() const { return Indices.size(); }

    uint FindVertextNearestTo(const glm::vec3 point) const;
    inline bool Empty() const { return Vertices.empty(); }

    std::vector<uint> GenerateTriangleIndices() {
        auto triangulated_mesh = Mesh; // `triangulate` is in-place, so we need to make a copy.
        triangulated_mesh.triangulate();
        std::vector<uint> indices;
        for (const auto &fh : triangulated_mesh.faces()) {
            auto v_it = triangulated_mesh.cfv_iter(fh);
            indices.insert(indices.end(), {uint(v_it->idx()), uint((++v_it)->idx()), uint((++v_it)->idx())});
        }
        return indices;
    }

    std::vector<uint> GenerateTriangulatedFaceIndices() {
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

    std::vector<uint> GenerateLineIndices() {
        std::vector<uint> indices;
        for (const auto &eh : Mesh.edges()) {
            const auto heh = Mesh.halfedge_handle(eh, 0);
            indices.push_back(Mesh.from_vertex_handle(heh).idx());
            indices.push_back(Mesh.to_vertex_handle(heh).idx());
        }
        return indices;
    }

    bool Load(const fs::path &file_path) {
        OpenMesh::IO::Options read_options; // No options used yet, but keeping this here for future use.
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
        static const float min_float = std::numeric_limits<float>::lowest();
        static const float max_float = std::numeric_limits<float>::max();

        glm::vec3 min(max_float), max(min_float);
        for (const auto &vh : Mesh.vertices()) {
            const auto &point = Mesh.point(vh);
            min.x = std::min(min.x, point[0]);
            min.y = std::min(min.y, point[1]);
            min.z = std::min(min.z, point[2]);
            max.x = std::max(max.x, point[0]);
            max.y = std::max(max.y, point[1]);
            max.z = std::max(max.z, point[2]);
        }

        return {min, max};
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
        Dirty = true;
    }

protected:
    MeshType Mesh;
    RenderMode ActiveRenderMode{RenderMode::Flat};
    mutable bool Dirty{true};

    void UpdateBuffersFromMesh() {
        UpdateVertices();
        UpdateNormals();
        UpdateIndices();
    }

    void UpdateVertices() {
        UniqueVertices.clear();
        Vertices.clear();

        for (const auto &vh : Mesh.vertices()) {
            const auto &point = Mesh.point(vh);
            UniqueVertices.emplace_back(point[0], point[1], point[2]);
        }

        if (ActiveRenderMode == RenderMode::Flat) {
            for (const auto &fh : Mesh.faces()) {
                for (const auto &vh : Mesh.fv_range(fh)) {
                    const auto &point = Mesh.point(vh);
                    Vertices.emplace_back(point[0], point[1], point[2]);
                }
            }
        } else {
            Vertices = UniqueVertices;
        }
        Dirty = true;
    }

    void UpdateIndices() {
        Indices =
            ActiveRenderMode == RenderMode::Lines ? GenerateLineIndices() :
            ActiveRenderMode == RenderMode::Flat  ? GenerateTriangulatedFaceIndices() :
                                                    GenerateTriangleIndices();
        Dirty = true;
    }

    void UpdateNormals() {
        UniqueNormals.clear();
        Normals.clear();

        Mesh.request_face_normals();
        Mesh.request_vertex_normals();
        Mesh.update_normals();
        for (const auto &vh : Mesh.vertices()) {
            const auto &normal = Mesh.normal(vh);
            UniqueNormals.emplace_back(normal[0], normal[1], normal[2]);
        }
        if (ActiveRenderMode == RenderMode::Flat) {
            for (const auto &fh : Mesh.faces()) {
                const auto &normal = Mesh.normal(fh);
                // Duplicate the normal for each vertex.
                for (size_t i = 0; i < Mesh.valence(fh); ++i) {
                    Normals.emplace_back(normal[0], normal[1], normal[2]);
                }
            }
        } else {
            Normals = UniqueNormals;
        }
        Mesh.release_vertex_normals();
        Mesh.release_face_normals();

        Dirty = true;
    }

    // todo instead of duplicating vertices, map indices (based on active render mode) to unique vertices.
    std::vector<glm::vec3> UniqueVertices, UniqueNormals;

    // Used for rendering. Note that `Vertices` and `Normals` depend on the active render mode, and may contain duplicates.
    std::vector<glm::vec3> Vertices;
    std::vector<glm::vec3> Normals;
    std::vector<uint> Indices;
};
