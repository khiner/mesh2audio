#pragma once

#include <filesystem>

#include <glm/vec2.hpp>

#include "GLBuffer.h"

namespace fs = std::filesystem;

enum RenderType_ {
    RenderType_Smooth,
    RenderType_Lines,
    RenderType_Points,
};
using RenderType = int;

struct Geometry {
    Geometry(uint num_vertices = 0, uint num_normals = 0, uint num_indices = 0);
    Geometry(fs::path file_path);
    ~Geometry();

    void Load(fs::path file_path);

    void EnableVertexAttributes() const;

    void Render(RenderType render_type = RenderType_Smooth) const;

    void Clear();
    void Save(fs::path file_path) const; // Export the mesh to a .obj file.

    bool Empty() const { return Vertices.empty(); }

    void CenterVertices();

    void SetPosition(const glm::vec3 &);
    void SetTransform(const glm::mat4 &);
    void SetColor(const glm::vec4 &);

    void ComputeNormals(); // If `Normals` is empty, compute the normals for each triangle.
    void ComputeLineIndices();

    void ExtrudeProfile(const std::vector<glm::vec2> &profile_vertices, uint slices, bool closed = false);

    VertexBuffer Vertices;
    NormalBuffer Normals;
    IndexBuffer TriangleIndices, LineIndices;
    ColorBuffer Colors;
    TransformBuffer Transforms;

    uint VertexArrayId;

private:
    void BindData(RenderType render_type = RenderType_Smooth) const;
};
