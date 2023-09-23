#pragma once

#include <vector>

#include <filesystem>

#include <glm/vec2.hpp>

#include "GLBuffer.h"

inline static const glm::mat4 Identity(1.f);
inline static const glm::vec3 Origin{0.f}, Up{0.f, 1.f, 0.f};

namespace fs = std::filesystem;

using uint = unsigned int;
using std::vector;

struct Geometry {
    Geometry(uint num_vertices = 0, uint num_normals = 0, uint num_indices = 0);
    Geometry(fs::path file_path);

    ~Geometry();

    void Load(fs::path file_path);

    void BindData() const;

    void Clear();
    void Save(fs::path file_path) const; // Export the mesh to a .obj file.

    bool Empty() const { return Vertices.empty(); }

    void Flip(bool x, bool y, bool z);
    void Rotate(const glm::vec3 &axis, float angle);
    void Scale(const glm::vec3 &);
    void Center();
    void Translate(const glm::vec3 &);

    void SetColor(const glm::vec4 &);

    void ComputeNormals(); // If `Normals` is empty, compute the normals for each triangle.
    void UpdateBounds(); // Updates the bounding box (`Min` and `Max`).
    void ExtrudeProfile(const vector<glm::vec2> &profile_vertices, uint slices, bool closed = false);

    VertexBuffer Vertices;
    NormalBuffer Normals;
    IndexBuffer Indices;
    InstanceModelsBuffer InstanceModels;
    InstanceColorsBuffer InstanceColors;
    glm::vec3 Min, Max; // The bounding box of the mesh.

    uint VertexArray;
};
