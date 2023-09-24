#pragma once

#include <GL/glew.h>
#include <filesystem>

namespace fs = std::filesystem;

struct Shader {
    static GLuint InitShader(GLenum type, const fs::path);
    static GLuint InitProgram(GLuint vertexshader, GLuint geometryshader, GLuint fragmentshader);
};
