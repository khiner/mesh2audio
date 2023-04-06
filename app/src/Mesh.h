#pragma once

#include <glm/vec3.hpp>

#include "MeshProfile.h"

using glm::vec3;

struct ImVec2;

struct Mesh {
    // Load a 3D mesh from a .obj file, or a 2D profile from a .svg file.
    Mesh(fs::path file_path);
    ~Mesh();

    void Bind() const;
    void InvertY(); // Invert the y-coordinates of the current 3D mesh.
    void RenderProfile() const;
    // Generate an axisymmetric 3D mesh by rotating the current 2D profile about the y-axis.
    // _This will have no effect if `Load(path)` was not called first to load a 2D profile._
    void ExtrudeProfile(int num_radial_slices = 100);

    int NumIndices() const { return indices.size(); }

    vector<vec3> vertices, normals;
    vector<unsigned int> indices;
    unsigned int vertex_array, vertex_buffer, normal_buffer, index_buffer;

private:
    // Non-empty if the mesh was generated from a 2D profile:
    std::unique_ptr<MeshProfile> profile;
};
