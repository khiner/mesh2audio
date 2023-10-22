#pragma once

#include <filesystem>

#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

using uint = unsigned int;

namespace fs = std::filesystem;

enum class RenderMode {
    Flat,
    Smooth,
    Lines,
    Points,
    Silhouette,
};

inline static glm::vec3 ToGlm(const OpenMesh::Vec3f &v) { return {v[0], v[1], v[2]}; }

struct MeshBuffers {
    using MeshType = OpenMesh::PolyMesh_ArrayKernelT<>;
    using VH = OpenMesh::VertexHandle;
    using FH = OpenMesh::FaceHandle;
    using EH = OpenMesh::EdgeHandle;
    using HH = OpenMesh::HalfedgeHandle;
    using Point = OpenMesh::Vec3f;

    MeshBuffers() {
        Mesh.request_face_normals();
        Mesh.request_vertex_normals();
    }
    MeshBuffers(const fs::path &file_path) {
        Mesh.request_face_normals();
        Mesh.request_vertex_normals();
        Load(file_path);
    }
    virtual ~MeshBuffers() {
        Mesh.release_vertex_normals();
        Mesh.release_face_normals();
    }

    virtual void PrepareRender(RenderMode mode) {
        if (ActiveRenderMode == mode) return;
        ActiveRenderMode = mode;
        UpdateBuffersFromMesh();
    }

    inline const MeshType &GetMesh() const { return Mesh; }

    inline uint NumVertices() const { return Mesh.n_vertices(); }
    inline uint NumFaces() const { return Mesh.n_faces(); }
    inline uint NumIndices() const { return Indices.size(); }

    inline const float *GetVertices() const { return (const float *)Mesh.points(); }
    inline glm::vec3 GetVertex(uint index) const { return ToGlm(Mesh.point(VH(index))); }
    inline glm::vec3 GetVertexNormal(uint index) const { return ToGlm(Mesh.normal(VH(index))); }
    inline glm::vec3 GetFaceNormal(uint index) const { return ToGlm(Mesh.normal(FH(index))); }
    inline glm::vec3 GetFaceCenter(uint index) const { return ToGlm(Mesh.calc_face_centroid(FH(index))); }

    uint FindVertextNearestTo(const glm::vec3 point) const;
    inline bool Empty() const { return Vertices.empty(); }

    std::vector<uint> GenerateTriangleIndices() const;
    std::vector<uint> GenerateTriangulatedFaceIndices() const;
    std::vector<uint> GenerateLineIndices() const;

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
            const auto &p = Mesh.point(vh);
            min.x = std::min(min.x, p[0]);
            min.y = std::min(min.y, p[1]);
            min.z = std::min(min.z, p[2]);
            max.x = std::max(max.x, p[0]);
            max.y = std::max(max.y, p[1]);
            max.z = std::max(max.z, p[2]);
        }

        return {min, max};
    }

    // Centers the actual points to the center of gravity, not just a transform.
    void Center() {
        const auto points = OpenMesh::getPointsProperty(Mesh);
        const auto cog = Mesh.vertices().avg(points);
        for (const auto &vh : Mesh.vertices()) Mesh.set_point(vh, Mesh.point(vh) - cog);
        // Normals/indices are not affected.
        UpdateVertices();
        UpdateFaces();
    }

    void SetOpenMesh(const MeshType &mesh) {
        Clear();
        Mesh.release_vertex_normals();
        Mesh.release_face_normals();
        Mesh = mesh;
        Mesh.request_face_normals();
        Mesh.request_vertex_normals();
        UpdateBuffersFromMesh();
    }

    void ExtrudeProfile(const std::vector<glm::vec2> &profile_vertices, uint slices, bool closed = false);

    void Clear() {
        Mesh.clear();
        Vertices.clear();
        Normals.clear();
        Indices.clear();

        FaceNormals.clear();
        FaceCenters.clear();

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

        UpdateFaces();
        UpdateEdgeToFaces();
    }

    void UpdateVertices() {
        Vertices.clear();
        if (ActiveRenderMode == RenderMode::Flat) {
            for (const auto &fh : Mesh.faces()) {
                for (const auto &vh : Mesh.fv_range(fh)) Vertices.emplace_back(ToGlm(Mesh.point(vh)));
            }
        } else {
            for (const auto &vh : Mesh.vertices()) Vertices.emplace_back(ToGlm(Mesh.point(vh)));
        }
        Dirty = true;
    }

    void UpdateIndices() {
        Indices =
            ActiveRenderMode == RenderMode::Lines || ActiveRenderMode == RenderMode::Silhouette ?
            GenerateLineIndices() :
            ActiveRenderMode == RenderMode::Flat ?
            GenerateTriangulatedFaceIndices() :
            GenerateTriangleIndices();
        Dirty = true;
    }

    void UpdateNormals() {
        Normals.clear();

        Mesh.update_normals();
        if (ActiveRenderMode == RenderMode::Flat) {
            for (const auto &fh : Mesh.faces()) {
                // Duplicate the normal for each vertex.
                const auto n = ToGlm(Mesh.normal(fh));
                for (size_t i = 0; i < Mesh.valence(fh); ++i) Normals.emplace_back(n);
            }
        } else {
            for (const auto &vh : Mesh.vertices()) Normals.emplace_back(ToGlm(Mesh.normal(vh)));
        }

        Dirty = true;
    }

    void UpdateFaces() {
        FaceNormals.clear();
        FaceCenters.clear();
        for (const auto &fh : Mesh.faces()) {
            FaceNormals.emplace_back(ToGlm(Mesh.normal(fh)));
            FaceCenters.emplace_back(ToGlm(Mesh.calc_face_centroid(fh)));
        }
        Dirty = true;
    }

    void UpdateEdgeToFaces() {
        EdgeToFaces.clear();
        for (const auto &eh : Mesh.edges()) {
            if (Mesh.is_boundary(eh)) {
                EdgeToFaces.push_back(glm::ivec2(-1));
            } else {
                const auto &heh = Mesh.halfedge_handle(eh, 0);
                const auto &fh1 = Mesh.face_handle(heh);
                const auto &fh2 = Mesh.opposite_face_handle(heh);
                EdgeToFaces.push_back(glm::ivec2(fh1.idx(), fh2.idx()));
            }
        }
        Dirty = true;
    }

    // Used for rendering. Note that `Vertices` and `Normals` depend on the active render mode, and may contain duplicates.
    std::vector<glm::vec3> Vertices;
    std::vector<glm::vec3> Normals;
    std::vector<uint> Indices;

    // Currently only used for silhouette rendering.
    std::vector<glm::vec3> FaceNormals;
    std::vector<glm::vec3> FaceCenters;
    std::vector<glm::ivec2> EdgeToFaces; // Maps edge index to its bordering face indices (or -1,-1 if boundary).
};
