#pragma once

#include <GL/glew.h>
#include <filesystem>

namespace fs = std::filesystem;

struct Shader {
    Shader(GLenum type, const fs::path);
    GLuint Id;
};
