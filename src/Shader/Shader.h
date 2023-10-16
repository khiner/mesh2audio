#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

#include <GL/glew.h>

namespace fs = std::filesystem;

struct Shader {
    Shader(GLenum type, const fs::path, std::unordered_set<std::string> uniform_names = {});

    GLuint Id;
    std::unordered_set<std::string> UniformNames;
};
