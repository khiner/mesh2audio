#pragma once

#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <vector>

using glm::vec2, glm::vec3;
using std::vector;
namespace fs = std::filesystem;

namespace gl {
struct Mesh {
    void Init();
    void Destroy();
    void Load(fs::path file_path); // Load a 3D mesh from a .obj file, or a 2D profile from a .svg file.
    void Bind() const;

    void SetProfile(const vector<vec2> &path);
    // Generate an axisymmetric 3D mesh by rotating the current 2D profile about the y-axis.
    // _This will have no effect if `Load(path)` was not called first to load a 2D profile._
    void ExtrudeProfile(int num_radial_slices = 100);
    void NormalizeProfile(); // Normalize the current 2D profile so that the largest dimension is 1.0:
    void InvertProfileY(); // Invert the y-coordinates of the current 2D profile.
    void CenterProfileY(); // Center the y-coordinates of the current 2D profile around 0.0.

    vector<vec2> profile_vertices; // Non-empty if the mesh was generated from a 2D profile.
    vector<vec3> vertices, normals;
    vector<unsigned int> indices;

    unsigned int vertex_array, vertex_buffer, normal_buffer, index_buffer;

private:
    void CubicBez(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tol, int level = 0);
};
} // namespace gl
