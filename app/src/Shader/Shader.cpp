#include "Shader.h"

#include <fstream>
#include <iostream>

// From https://stackoverflow.com/a/40903508/780425
static std::string ReadFile(const fs::path path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    const auto size = fs::file_size(path);
    std::string result(size, '\0');
    f.read(result.data(), size);
    return result;
}

static void HandleErrors(const GLint shader) {
    GLint length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    std::vector<GLchar> log(length + 1);
    glGetShaderInfoLog(shader, length, &length, log.data());

    std::cout << "Compile Error:\n"
              << log.data() << "\n";
}

Shader::Shader(GLenum type, const fs::path path, std::unordered_set<std::string> uniform_names)
    : UniformNames(uniform_names) {
    std::string str = ReadFile(path);
    const char *cstr = str.c_str();

    Id = glCreateShader(type);
    glShaderSource(Id, 1, &cstr, NULL);
    glCompileShader(Id);

    GLint compiled;
    glGetShaderiv(Id, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        HandleErrors(Id);
        throw std::runtime_error("Failed to compile shader");
    }
}
