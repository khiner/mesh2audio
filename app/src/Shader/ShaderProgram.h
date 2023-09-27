#pragma once

#include <unordered_map>
#include <vector>

#include "Shader.h"

struct ShaderProgram {
    ShaderProgram(std::vector<const Shader *> &&);
    ~ShaderProgram() = default;

    void Use();

    inline GLuint GetUniform(const std::string &name) const { return Uniforms.at(name); }

    GLuint Id;
    std::vector<const Shader *> Shaders;
    std::unordered_map<std::string, GLuint> Uniforms;
};
