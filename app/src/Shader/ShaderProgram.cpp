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

ShaderProgram::ShaderProgram(const std::vector<GLuint> &shader_ids, const std::vector<std::string> &uniforms) {
    Id = glCreateProgram();
    for (GLuint shader_id : shader_ids) glAttachShader(Id, shader_id);

    glLinkProgram(Id);

    GLint linked;
    glGetProgramiv(Id, GL_LINK_STATUS, &linked);
    if (!linked) {
        HandleErrors(Id);
        throw std::runtime_error("Shader program linking failed");
    }

    Use();

    // Initialize uniform locations
    for (const auto &uniform : uniforms) {
        Uniforms[uniform] = glGetUniformLocation(Id, uniform.c_str());
    }
}

void ShaderProgram::Use() { glUseProgram(Id); }
