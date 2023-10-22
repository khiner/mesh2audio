#pragma once

#include <GL/glew.h>
#include <glm/mat4x4.hpp>

#include "MeshBuffers.h"

template<typename DataType, GLenum Target>
struct GLBuffer {
    void Generate() { glGenBuffers(1, &Id); }
    void Delete() const { glDeleteBuffers(1, &Id); }
    void Bind() const { glBindBuffer(Target, Id); }
    void Unbind() const { glBindBuffer(Target, 0); }

    void SetData(const std::vector<DataType> &data, GLenum usage = GL_STATIC_DRAW) const {
        Bind();
        glBufferData(Target, data.size() * sizeof(DataType), data.data(), usage);
    }

    uint Id = 0;
};

inline static const glm::mat4 I(1.f);
inline static const glm::vec3 Origin{0.f}, Up{0.f, 1.f, 0.f};

// todo use OpenMesh as the main mesh data structure, and derive all fields from it.
struct Geometry : MeshBuffers {
    Geometry() : MeshBuffers() {}
    Geometry(const fs::path &file_path) : MeshBuffers() {
        Load(file_path);
        Center();
    }

    virtual ~Geometry() = default;

    void EnableVertexAttributes() const;
    void Generate();
    void Delete() const;

    void BindData(RenderMode) const; // Only rebinds the data if it has changed.

private:
    GLBuffer<glm::vec3, GL_ARRAY_BUFFER> VertexBuffer;
    GLBuffer<glm::vec3, GL_ARRAY_BUFFER> NormalBuffer;
    GLBuffer<uint, GL_ELEMENT_ARRAY_BUFFER> IndexBuffer;

    mutable RenderMode LastBoundRenderMode = RenderMode::Smooth;
};
