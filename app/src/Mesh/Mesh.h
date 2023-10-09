#pragma once

#include "Geometry/Geometry.h"

struct VertexArray {
    void Generate() { glGenVertexArrays(1, &Id); }
    void Delete() const { glDeleteVertexArrays(1, &Id); }
    void Bind() const { glBindVertexArray(Id); }
    void Unbind() const { glBindVertexArray(0); }

    uint Id = 0;
};

struct Mesh {
    Mesh() {}
    Mesh(Geometry &&triangles) : Triangles(std::move(triangles)) {}
    virtual ~Mesh() {}

    virtual const Geometry &ActiveGeometry() const { return Triangles; }
    inline Geometry &ActiveGeometry() { return const_cast<Geometry &>(static_cast<const Mesh *>(this)->ActiveGeometry()); }

    virtual std::vector<const Geometry *> AllGeometries() const { return {&Triangles}; }

    void Generate();
    void Delete() const;

    void EnableVertexAttributes() const;

    void SetupRender(RenderMode mode = RenderMode_Smooth);
    void Render(RenderMode mode = RenderMode_Smooth) const;

    void SetPosition(const glm::vec3 &);
    void SetTransform(const glm::mat4 &);
    void SetColor(const glm::vec4 &);

    Geometry Triangles;
    std::vector<glm::vec4> Colors{{1, 1, 1, 1}};
    std::vector<glm::mat4> Transforms{glm::mat4{1}};

private:
    VertexArray VertexArray;
    GLuint ColorBufferId, TransformBufferId;

    void BindData(RenderMode render_mode = RenderMode_Smooth) const;
};
