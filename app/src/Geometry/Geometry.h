#pragma once

#include <filesystem>

#include <glm/vec2.hpp>

#include "GLBuffer.h"
#include "GeometryData.h"

inline static const glm::mat4 Identity(1.f);
inline static const glm::vec3 Origin{0.f}, Up{0.f, 1.f, 0.f};

namespace fs = std::filesystem;

struct Geometry {
    Geometry(uint num_vertices = 0, uint num_normals = 0, uint num_indices = 0);
    Geometry(fs::path file_path);
    virtual ~Geometry() = default;

    void Save(fs::path file_path) const; // Export the mesh to a .obj file.
    void Load(fs::path file_path);

    bool Empty() const { return Vertices.empty(); }

    void Center();
    void Clear();
    void SetGeometryData(const GeometryData &);

    std::pair<glm::vec3, glm::vec3> ComputeBounds(); // [{min_x, min_y, min_z}, {max_x, max_y, max_z}]
    void ComputeNormals(); // If `Normals` is empty, compute the normals for each triangle.
    void ComputeLineIndices();

    void ExtrudeProfile(const std::vector<glm::vec2> &profile_vertices, uint slices, bool closed = false);

    VertexBuffer Vertices;
    NormalBuffer Normals;
    IndexBuffer TriangleIndices, LineIndices;
};
