#pragma once

#include <filesystem>

#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GeometryData.h"

inline static const glm::mat4 Identity(1.f);
inline static const glm::vec3 Origin{0.f}, Up{0.f, 1.f, 0.f};

namespace fs = std::filesystem;

enum RenderMode {
    RenderMode_Smooth,
    RenderMode_Lines,
    RenderMode_Points,
};

// todo use OpenMesh as the main mesh data structure, and derive all fields from it.
struct Geometry {
    Geometry(uint num_vertices = 0, uint num_normals = 0, uint num_indices = 0);
    Geometry(fs::path file_path);
    virtual ~Geometry() = default;

    void Save(fs::path file_path) const; // Export the mesh to a .obj file.
    void Load(fs::path file_path);

    inline bool Empty() const { return Vertices.empty(); }
    inline const std::vector<glm::vec3> &GetVertices() const { return Vertices; }
    inline const std::vector<glm::vec3> &GetNormals() const { return Normals; }
    inline const std::vector<uint> &GetIndices(RenderMode mode) const { return mode == RenderMode_Lines ? LineIndices : TriangleIndices; }
    inline const glm::vec3 &GetVertex(uint index) const { return Vertices[index]; }
    inline const glm::vec3 &GetNormal(uint index) const { return Normals[index]; }

    void ClearNormals() {
        Normals.clear();
        Dirty = true;
    }

    void Center() {
        const auto [min, max] = ComputeBounds();
        for (glm::vec3 &v : Vertices) v -= (min + max) / 2.0f;
        Dirty = true;
    }

    void Clear() {
        Vertices.clear();
        Normals.clear();
        TriangleIndices.clear();
        LineIndices.clear();
        Dirty = true;
    }

    void SetGeometryData(const GeometryData &geom_data) {
        Clear();
        Vertices.assign(geom_data.Vertices.begin(), geom_data.Vertices.end());
        TriangleIndices.assign(geom_data.Indices.begin(), geom_data.Indices.end());
        ComputeNormals();
    }

    std::pair<glm::vec3, glm::vec3> ComputeBounds() const; // [{min_x, min_y, min_z}, {max_x, max_y, max_z}]
    void ComputeNormals(); // If `Normals` is empty, compute the normals for each triangle.

    void ExtrudeProfile(const std::vector<glm::vec2> &profile_vertices, uint slices, bool closed = false);

    void EnableVertexAttributes() const;
    void Generate();
    void Delete() const;

    void BindData(RenderMode) const;
    void PrepareRender(RenderMode);

    GLuint VertexBufferId, NormalBufferId, TriangleIndexBufferId, LineIndexBufferId;

protected:
    std::vector<glm::vec3> Vertices, Normals;
    std::vector<uint> TriangleIndices, LineIndices;

    mutable bool Dirty{true};
};
