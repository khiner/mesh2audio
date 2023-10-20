#include "ShaderProgram.h"

#include <format>
#include <iostream>

ShaderProgram::ShaderProgram(std::vector<const Shader *> &&shaders)
    : Shaders(std::move(shaders)) {
    Id = glCreateProgram();
    for (const auto *shader : Shaders) glAttachShader(Id, shader->Id);

    glLinkProgram(Id);

    GLint linked;
    glGetProgramiv(Id, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length;
        glGetProgramiv(Id, GL_INFO_LOG_LENGTH, &length);

        std::vector<GLchar> log(length + 1);
        glGetProgramInfoLog(Id, length, &length, log.data());
        throw std::runtime_error(std::format("Shader program linking failed. Log:\n{}", log.data()));
    }

    std::unordered_set<std::string> uniforms;
    for (const auto *shader : Shaders) {
        uniforms.insert(shader->UniformNames.begin(), shader->UniformNames.end());
    }

    // Initialize uniform locations
    for (const auto &uniform : uniforms) {
        int location = glGetUniformLocation(Id, uniform.c_str());
        if (location == -1) throw std::runtime_error(std::format("Uniform {} not found", uniform));

        Uniforms[uniform] = location;
    }
}

void ShaderProgram::Use() { glUseProgram(Id); }
