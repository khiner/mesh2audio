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
        Bind();
        if (Dirty) {
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

struct IndexBuffer : GLBuffer<uint> {
    IndexBuffer(size_t size = 0) : GLBuffer(GL_ELEMENT_ARRAY_BUFFER, size) {}
};

#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

template<typename DataType>
struct PointBuffer : GLBuffer<DataType> {
    using GLBuffer<DataType>::Dirty;
    using GLBuffer<DataType>::Data;
    using GLBuffer<DataType>::Bind;

    PointBuffer(size_t size = 0) : GLBuffer<DataType>(GL_ARRAY_BUFFER, size) {}

    // `layout (location = slot)` in the vertex shader.
    void EnableVertexAttribute(GLuint slot) const {
        Bind();
        glEnableVertexAttribArray(slot);
        DoEnableVertexAttribute(slot);
    }

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

protected:
    virtual void DoEnableVertexAttribute(GLuint slot) const = 0;
};

struct VertexBuffer : PointBuffer<glm::vec3> {
    VertexBuffer(size_t size = 0) : PointBuffer(size) {}

    void DoEnableVertexAttribute(GLuint slot) const override {
        glVertexAttribPointer(slot, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    }
};

struct NormalBuffer : PointBuffer<glm::vec3> {
    NormalBuffer(size_t size = 0) : PointBuffer(size) {}

    void DoEnableVertexAttribute(GLuint slot) const override {
        glVertexAttribPointer(slot, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    }
};

// {x, y, z, scale}
struct TranslateScaleBuffer : PointBuffer<glm::vec4> {
    TranslateScaleBuffer(size_t size = 0) : PointBuffer(size) {
        push_back(glm::vec4{1});
    }

    void DoEnableVertexAttribute(GLuint slot) const override {
        glVertexAttribPointer(slot, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
        glVertexAttribDivisor(slot, 1); // Attribute is updated once per instance.
    }
};

struct TransformBuffer : PointBuffer<glm::mat4> {
    TransformBuffer(size_t size = 0) : PointBuffer(size) {
        push_back(glm::mat4{1});
    }

    void DoEnableVertexAttribute(GLuint slot) const override {
        // Since a `mat4` is actually 4 `vec4`s, we need to enable four attributes for it.
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(slot + i);
            glVertexAttribPointer(slot + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (GLvoid *)(i * sizeof(glm::vec4)));
            glVertexAttribDivisor(slot + i, 1); // Attribute is updated once per instance.
        }
    }
};

struct ColorBuffer : PointBuffer<glm::vec4> {
    ColorBuffer(size_t size = 0) : PointBuffer(size) {
        push_back({1, 1, 1, 1});
    }

    void DoEnableVertexAttribute(GLuint slot) const override {
        glVertexAttribPointer(slot, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
        glVertexAttribDivisor(slot, 1); // Attribute is updated once per instance.
    }
};
