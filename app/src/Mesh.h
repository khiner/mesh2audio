#pragma once

#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <vector>

using glm::vec2, glm::vec3;
using std::vector;
namespace fs = std::filesystem;

struct ImVec2;

namespace gl {
struct Mesh {
    void Init();
    void Destroy();
    void Load(fs::path file_path); // Load a 3D mesh from a .obj file, or a 2D profile from a .svg file.
    void Bind() const;

    void InvertY(); // Invert the y-coordinates of the current 3D mesh.

    bool HasProfile() const { return !control_points.empty(); }

    // Generate an axisymmetric 3D mesh by rotating the current 2D profile about the y-axis.
    // _This will have no effect if `Load(path)` was not called first to load a 2D profile._
    void ExtrudeProfile(int num_radial_slices = 100);
    void NormalizeProfile(); // Normalize the current 2D profile so that the largest dimension is 1.0:

    void RenderProfile() const; // Render the current 2D profile as a closed line shape (using ImGui).
    ImVec2 GetControlPoint(int i, const ImVec2& offset, const float scale) const;

    vector<vec3> vertices, normals;
    vector<unsigned int> indices;
    unsigned int vertex_array, vertex_buffer, normal_buffer, index_buffer;

private:
    // Non-empty if the mesh was generated from a 2D profile:
    vector<ImVec2> control_points;
};
} // namespace gl
