#pragma once

#include <vector>

#include <filesystem>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace fs = std::filesystem;

using uint = unsigned int;
using std::vector;

struct Geometry {
    Geometry();
    Geometry(uint num_vertices, uint num_normals, uint num_indices);
    Geometry(fs::path file_path);

    ~Geometry();

    void Load(fs::path file_path);

    void Bind() const; // Bind mesh and set up vertex attributes.
    void Clear();
    void Save(fs::path file_path) const; // Export the mesh to a .obj file.

    bool Empty() const { return Vertices.empty(); }

    void Flip(bool x, bool y, bool z);
    void Rotate(const glm::vec3 &axis, float angle);
    void Scale(const glm::vec3 &);
    void Center();
    void Translate(const glm::vec3 &);

    void ComputeNormals(); // If `Normals` is empty, compute the normals for each triangle.
    void UpdateBounds(); // Updates the bounding box (`Min` and `Max`).
    void ExtrudeProfile(const vector<glm::vec2> &profile_vertices, uint slices, bool closed = false);

    vector<glm::vec3> Vertices, Normals;
    vector<uint> Indices;
    std::vector<glm::mat4> InstanceModels;
    glm::vec3 Min, Max; // The bounding box of the mesh.

    uint VertexArray, VertexBuffer, NormalBuffer, IndexBuffer;
    uint InstanceModelBuffer;
};
