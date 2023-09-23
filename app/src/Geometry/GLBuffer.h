#pragma once

#include <vector>

#include <GL/glew.h>

using uint = unsigned int;
using GLuint = uint;
using GLenum = uint;

template<typename DataType>
struct GLBuffer {
    GLBuffer(GLenum type, size_t size = 0) : Type(type) {
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

    const DataType *data() const { return Data.data(); }

    DataType &operator[](size_t index) {
        Dirty = true;
        return Data[index];
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

    auto begin() {
        Dirty = true;
        return Data.begin();
    }
    auto end() {
        Dirty = true;
        return Data.end();
    }

    GLuint BufferId;
    GLenum Type;
    mutable bool Dirty{false};

    std::vector<DataType> Data;
};

#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct PointBuffer : GLBuffer<glm::vec3> {
    PointBuffer(size_t size = 0) : GLBuffer(GL_ARRAY_BUFFER, size) {}

    const PointBuffer &operator*=(const glm::vec3 &value) {
        for (auto &elem : Data) elem *= value;
        Dirty = true;
        return *this;
    }

    const PointBuffer &operator*=(float value) {
        for (auto &elem : Data) elem *= value;
        Dirty = true;
        return *this;
    }

    const PointBuffer &operator*=(const glm::qua<float> &value) {
        for (auto &elem : Data) elem = value * elem;
        Dirty = true;
        return *this;
    }

    const PointBuffer &operator+=(const glm::vec3 &value) {
        for (auto &elem : Data) elem += value;
        Dirty = true;
        return *this;
    }

    const PointBuffer &operator-=(const glm::vec3 &value) {
        for (auto &elem : Data) elem -= value;
        Dirty = true;
        return *this;
    }
};

struct VertexBuffer : PointBuffer {
    VertexBuffer(size_t size = 0) : PointBuffer(size) {}
};

struct NormalBuffer : PointBuffer {
    NormalBuffer(size_t size = 0) : PointBuffer(size) {}
};

struct IndexBuffer : GLBuffer<uint> {
    IndexBuffer(size_t size = 0) : GLBuffer(GL_ELEMENT_ARRAY_BUFFER, size) {}
};

struct InstanceModelsBuffer : GLBuffer<glm::mat4> {
    InstanceModelsBuffer(size_t size = 0) : GLBuffer(GL_ARRAY_BUFFER, size) {
        push_back(glm::mat4{1});
    }
};

struct InstanceColorsBuffer : GLBuffer<glm::vec4> {
    InstanceColorsBuffer(size_t size = 0) : GLBuffer(GL_ARRAY_BUFFER, size) {
        push_back({1, 1, 1, 1});
    }
};
