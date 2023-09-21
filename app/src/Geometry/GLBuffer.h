#pragma once

#include <vector>

#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

using uint = unsigned int;
using GLuint = uint;
using GLenum = uint;

template<typename DataType>
struct GLBuffer {
    GLBuffer(GLenum type, uint size = 0) : Type(type) {
        glGenBuffers(1, &BufferId);
        Data.reserve(size);
    }

    ~GLBuffer() {
        glDeleteBuffers(1, &BufferId);
    }

    void Bind() const {
        glBindBuffer(Type, BufferId);
    }
    void BindData() const {
        if (Dirty) {
            Bind();
            glBufferData(Type, sizeof(DataType) * Data.size(), Data.data(), GL_STATIC_DRAW);
            Dirty = false;
        }
    }
    void Unbind() const {
        glBindBuffer(Type, 0);
    }

    void Set(size_t i, const DataType &v) {
        Data[i] = v;
        Dirty = true;
    }

    void push_back(const DataType &v) {
        Data.push_back(v);
        Dirty = true;
    }

    void clear() {
        Data.clear();
        Dirty = true;
    }

    template<typename Iterator>
    void assign(Iterator begin, Iterator end) {
        Data.assign(begin, end);
        Dirty = true;
    }

    void append(std::vector<DataType> &&v) {
        Data.insert(Data.end(), v.begin(), v.end());
        Dirty = true;
    }

    const DataType *data() const { return Data.data(); }

    void resize(size_t size) {
        Data.resize(size);
        Dirty = true;
    }
    void resize(size_t size, const DataType &v) {
        Data.resize(size, v);
        Dirty = true;
    }
    void reserve(size_t size) { Data.reserve(size); }

    const DataType &operator[](size_t index) const { return Data[index]; }

    bool empty() const { return Data.empty(); }
    size_t size() const { return Data.size(); }

    auto begin() const { return Data.cbegin(); }
    auto end() const { return Data.cend(); }
    auto begin() { return Data.begin(); }
    auto end() { return Data.end(); }

    GLuint BufferId;
    GLenum Type;
    mutable bool Dirty{false};

    std::vector<DataType> Data;
};

struct VertexBuffer : GLBuffer<glm::vec3> {
    VertexBuffer(uint size = 0) : GLBuffer(GL_ARRAY_BUFFER, size) {}
};

struct NormalBuffer : GLBuffer<glm::vec3> {
    NormalBuffer(uint size = 0) : GLBuffer(GL_ARRAY_BUFFER, size) {}
};

struct IndexBuffer : GLBuffer<uint> {
    IndexBuffer(uint size = 0) : GLBuffer(GL_ELEMENT_ARRAY_BUFFER, size) {}
};

struct InstanceModelsBuffer : GLBuffer<glm::mat4> {
    InstanceModelsBuffer(uint size = 0) : GLBuffer(GL_ARRAY_BUFFER, size) {
        push_back(glm::mat4{1});
    }
};

struct InstanceColorsBuffer : GLBuffer<glm::vec4> {
    InstanceColorsBuffer(uint size = 0) : GLBuffer(GL_ARRAY_BUFFER, size) {
        push_back({1, 1, 1, 1});
    }
};
