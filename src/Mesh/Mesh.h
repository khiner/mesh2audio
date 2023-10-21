#pragma once

#include "Geometry/Geometry.h"

struct GLVertexArray {
    void Generate() { glGenVertexArrays(1, &Id); }
    void Delete() const { glDeleteVertexArrays(1, &Id); }
    void Bind() const { glBindVertexArray(Id); }
    void Unbind() const { glBindVertexArray(0); }

    uint Id = 0;
};

struct Mesh {
    Mesh() {}
    // If `parent` is provided, the mesh will be rendered relative to the parent's transform.
    Mesh(Geometry &&triangles, Mesh *parent = nullptr) : Triangles(std::move(triangles)), Parent(parent) {}
    virtual ~Mesh() {}

    virtual const Geometry &ActiveGeometry() const { return Triangles; }
    inline Geometry &ActiveGeometry() { return const_cast<Geometry &>(static_cast<const Mesh *>(this)->ActiveGeometry()); }

    virtual std::vector<const Geometry *> AllGeometries() const { return {&Triangles}; }

    const Geometry &GetTriangles() const { return Triangles; }
    const glm::mat4 &GetTransform() const { return Transforms[0]; }

    void Generate();
    void Delete() const;
    void EnableVertexAttributes() const;

    virtual void PrepareRender(RenderMode mode) { ActiveGeometry().PrepareRender(mode); }
    void Render(RenderMode mode) const;
    virtual void PostRender(RenderMode) {}

    void SetPosition(const glm::vec3 &position) {
        for (auto &transform : Transforms) {
            transform[3][0] = position.x;
            transform[3][1] = position.y;
            transform[3][2] = position.z;
        }
        Dirty = true;
    }
    void SetTransform(const glm::mat4 &new_transform) {
        for (auto &transform : Transforms) transform = new_transform;
        Dirty = true;
    }
    void SetTransforms(std::vector<glm::mat4> &&transforms) {
        Transforms = std::move(transforms);
        Dirty = true;
    }
    void ClearTransforms() {
        Transforms.clear();
        Dirty = true;
    }
    void SetColor(uint instance, const glm::vec4 &color) {
        Colors[instance] = color;
        Dirty = true;
    }
    void SetColor(const glm::vec4 &color) {
        Colors.clear();
        Colors.resize(Transforms.size(), color);
        Dirty = true;
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
    Geometry Triangles;
    Mesh *Parent{nullptr};

    std::vector<glm::vec4> Colors{{1, 1, 1, 1}};
    std::vector<glm::mat4> Transforms{glm::mat4{1}}; // If `Parent != nullptr`, this is relative to the parent's transform.
    mutable std::vector<glm::mat4> AbsoluteTransforms{}; // If `Parent != nullptr`, this is a combined transform of the parent and child. Otherwise, it's empty.

private:
    GLVertexArray VertexArray;
    GLBuffer<glm::vec4, GL_ARRAY_BUFFER> ColorBuffer;
    GLBuffer<glm::mat4, GL_ARRAY_BUFFER> TransformBuffer;
    mutable bool Dirty{true};

    void BindData(RenderMode render_mode = RenderMode::Smooth) const;
};
