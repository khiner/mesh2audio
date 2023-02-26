#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <vector>

using glm::vec3;
namespace fs = std::filesystem;

namespace gl {
struct Mesh {
    void generate_buffers();
    void destroy_buffers();
    void parse_and_bind();

    fs::path object_path;
    std::vector<vec3> vertices, normals;
    std::vector<unsigned int> indices;

    unsigned int vertex_array, vertex_buffer, normal_buffer, index_buffer;
};
} // namespace gl
