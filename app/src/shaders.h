#include <filesystem>

namespace fs = std::filesystem;

struct Shader {
    static GLuint init_shaders(GLenum type, const fs::path);
    static GLuint init_program(GLuint vertexshader, GLuint fragmentshader);
};
