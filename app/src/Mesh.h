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
    void Load(fs::path object_path);
    // Generate an axisymmetric 3D mesh by rotating the provided path about the y-axis.
    void ExtrudeXYPath(const vector<vec2> &path, int num_radial_slices);

    std::vector<vec3> vertices, normals;
    std::vector<unsigned int> indices;

    unsigned int vertex_array, vertex_buffer, normal_buffer, index_buffer;

private:
    void Bind() const;
};
} // namespace gl
