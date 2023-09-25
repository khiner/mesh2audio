#pragma once

#include <unordered_map>
#include <vector>

#include "Shader.h"

struct ShaderProgram {
    ShaderProgram(const std::vector<GLuint> &shader_ids, const std::vector<std::string> &uniforms = {});
    ~ShaderProgram() = default;

    void Use();

    inline GLuint GetUniform(const std::string &name) const { return Uniforms.at(name); }

    GLuint Id;
    std::unordered_map<std::string, GLuint> Uniforms;
};
