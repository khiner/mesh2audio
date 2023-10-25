#pragma once

#include "Geometry/Geometry.h"

struct GLVertexArray {
    void Generate() { glGenVertexArrays(1, &Id); }
    void Delete() const { glDeleteVertexArrays(1, &Id); }
    void Bind() const { glBindVertexArray(Id); }
    void Unbind() const { glBindVertexArray(0); }

    uint Id = 0;
};

struct Mesh : Geometry {
    Mesh() : Geometry() {}
    // If `parent` is provided, the mesh will be rendered relative to the parent's transform.
    Mesh(MeshBuffers &&triangles, Mesh *parent = nullptr) : Geometry(std::move(triangles)), Parent(parent) {}
    virtual ~Mesh() {}

    uint NumInstances() const { return Transforms.size(); }

    virtual const MeshBuffers &GetTriangles() const { return *this; }

    const glm::mat4 &GetTransform() const { return Transforms[0]; }
    const glm::vec3 GetLocalVertex(uint vi) const { return Geometry::GetVertex(vi); }
    const glm::vec3 GetVertex(uint vi, uint instance = 0) const { return Transforms[instance] * glm::vec4(Geometry::GetVertex(vi), 1); }
    const glm::vec3 GetFaceCenter(uint fi, uint instance = 0) const { return Transforms[instance] * glm::vec4(Geometry::GetFaceCenter(fi), 1); }

    void Generate() override;
    void Delete() const override;
    void EnableVertexAttributes() const override;

    void Render(RenderMode mode) const;
    virtual void PostRender(RenderMode) {}

    void ClearInstances() {
        Transforms.clear();
        Colors.clear();
        Dirty = true;
    }
    void AddInstance(const glm::mat4 &transform, const glm::vec4 &color) {
        Transforms.push_back(transform);
        Colors.push_back(color);
        Dirty = true;
    }

    void AddInstance(const glm::mat4 &transform) {
        AddInstance(transform, Colors.empty() ? glm::vec4{1} : Colors[0]);
        Dirty = true;
    }

    void SetPosition(const glm::vec3 &position) {
        for (auto &transform : Transforms) {
            transform[3][0] = position.x;
            transform[3][1] = position.y;
            transform[3][2] = position.z;
        }
        Dirty = true;
    }
    void SetTransform(uint instance, const glm::mat4 &transform) {
        Transforms[instance] = transform;
        Dirty = true;
    }
    void SetTransform(const glm::mat4 &transform) {
        for (uint instance = 0; instance < Transforms.size(); instance++) SetTransform(instance, transform);
    }
    void SetTransforms(std::vector<glm::mat4> &&transforms) {
        Transforms = std::move(transforms);
        Dirty = true;
    }

    void SetColor(uint instance, const glm::vec4 &color) {
        Colors[instance] = color;
        Dirty = true;
    }
    void SetColor(const glm::vec4 &color) {
        for (uint instance = 0; instance < Colors.size(); instance++) SetColor(instance, color);
    }
    void SetColors(std::vector<glm::vec4> &&colors) {
        Colors = std::move(colors);
        Colors.resize(Transforms.size());
        Dirty = true;
    }
    void ClearColors() {
        Colors.clear();
        Dirty = true;
    }

protected:
    Mesh *Parent{nullptr};

    std::vector<glm::vec4> Colors{{1, 1, 1, 1}};
    std::vector<glm::mat4> Transforms{glm::mat4{1}}; // If `Parent != nullptr`, this is relative to the parent's transform.
    mutable std::vector<glm::mat4> AbsoluteTransforms{}; // If `Parent != nullptr`, this is a combined transform of the parent and child. Otherwise, it's empty.

private:
    void BindData(RenderMode) const;

    GLVertexArray VertexArray;
    GLBuffer<glm::vec4, GL_ARRAY_BUFFER> ColorBuffer;
    GLBuffer<glm::mat4, GL_ARRAY_BUFFER> TransformBuffer;
    mutable bool Dirty{true};
};
