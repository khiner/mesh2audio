#include "ShaderProgram.h"
#include <iostream>
#include <vector>

static void HandleErrors(const GLint program) {
    GLint length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    std::vector<GLchar> log(length + 1);
    glGetProgramInfoLog(program, length, &length, log.data());

    std::cout << "Compile Error:\n"
              << log.data() << "\n";
}

ShaderProgram::ShaderProgram(std::vector<const Shader *> &&shaders)
    : Shaders(std::move(shaders)) {
    Id = glCreateProgram();
    for (const auto *shader : Shaders) glAttachShader(Id, shader->Id);

    glLinkProgram(Id);

    GLint linked;
    glGetProgramiv(Id, GL_LINK_STATUS, &linked);
    if (!linked) {
        HandleErrors(Id);
        throw std::runtime_error("Shader program linking failed");
    }

    std::unordered_set<std::string> uniforms;
    for (const auto *shader : Shaders) {
        uniforms.insert(shader->UniformNames.begin(), shader->UniformNames.end());
    }

    // Initialize uniform locations
    for (const auto &uniform : uniforms) {
        Uniforms[uniform] = glGetUniformLocation(Id, uniform.c_str());
    }
}

void ShaderProgram::Use() { glUseProgram(Id); }
