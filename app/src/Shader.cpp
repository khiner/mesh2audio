#include "Shader.h"

#include <fstream>
#include <iostream>
#include <string>

// From https://stackoverflow.com/a/40903508/780425
static std::string read_file(const fs::path path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    const auto sz = fs::file_size(path);
    std::string result(sz, '\0');
    f.read(result.data(), sz);
    return result;
}

static void shader_errors(const GLint shader) {
    GLint length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    GLchar *log = new GLchar[length + 1];
    glGetShaderInfoLog(shader, length, &length, log);

    std::cout << "Compile Error, Log Below\n"
              << log << "\n";
    delete[] log;
}

static void program_errors(const GLint program) {
    GLint length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    GLchar *log = new GLchar[length + 1];
    glGetProgramInfoLog(program, length, &length, log);

    std::cout << "Compile Error, Log Below\n"
              << log << "\n";
    delete[] log;
}

GLuint Shader::InitShader(GLenum type, const fs::path path) {
    GLuint shader = glCreateShader(type);

    std::string str = read_file(path);
    const char *cstr = str.c_str();

    glShaderSource(shader, 1, &cstr, NULL);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        shader_errors(shader);
        throw 3;
    }
    return shader;
}

GLuint Shader::InitProgram(GLuint vertexshader, GLuint fragmentshader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexshader);
    glAttachShader(program, fragmentshader);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked) {
        glUseProgram(program);
    } else {
        program_errors(program);
        throw 4;
    }
    return program;
}
