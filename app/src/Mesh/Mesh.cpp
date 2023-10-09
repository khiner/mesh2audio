#include "Mesh.h"

using glm::vec3, glm::vec4, glm::mat4;

void Mesh::Generate() {
    VertexArray.Generate();
    glGenBuffers(1, &ColorBufferId);
    glGenBuffers(1, &TransformBufferId);
    for (const auto *geom : static_cast<const Mesh *>(this)->AllGeometries()) {
        const_cast<Geometry *>(geom)->Generate();
    }
    EnableVertexAttributes();
}

void Mesh::Delete() const {
    VertexArray.Delete();
    glDeleteBuffers(1, &TransformBufferId);
    glDeleteBuffers(1, &ColorBufferId);
    for (const auto *geom : AllGeometries()) geom->Delete();
}

void Mesh::EnableVertexAttributes() const {
    VertexArray.Bind();
    ActiveGeometry().EnableVertexAttributes();

    static const GLuint ColorSlot = 2;
    static const GLuint TransformSlot = 3;
    glBindBuffer(GL_ARRAY_BUFFER, ColorBufferId);
    glEnableVertexAttribArray(ColorSlot);
    glVertexAttribPointer(ColorSlot, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
    glVertexAttribDivisor(ColorSlot, 1); // Attribute is updated once per instance.

    glBindBuffer(GL_ARRAY_BUFFER, TransformBufferId);
    // Since a `mat4` is actually 4 `vec4`s, we need to enable four attributes for it.
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(TransformSlot + i);
        glVertexAttribPointer(TransformSlot + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (GLvoid *)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(TransformSlot + i, 1); // Attribute is updated once per instance.
    }
    VertexArray.Unbind();
}

void Mesh::BindData(RenderMode render_mode) const {
    VertexArray.Bind();
    ActiveGeometry().BindData(render_mode);

    glBindBuffer(GL_ARRAY_BUFFER, TransformBufferId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * Transforms.size(), Transforms.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, ColorBufferId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * Colors.size(), Colors.data(), GL_STATIC_DRAW);

    VertexArray.Unbind();
}

void Mesh::SetupRender(RenderMode mode) {
    auto &geom = ActiveGeometry();
    if (mode == RenderMode_Lines && geom.LineIndices.empty()) geom.ComputeLineIndices();
    else if (mode != RenderMode_Lines && !geom.LineIndices.empty()) geom.LineIndices.clear(); // Save memory.
}

void Mesh::Render(RenderMode mode) const {
    if (Transforms.empty()) return;

    BindData(mode); // Only rebinds the data if it has changed.
    VertexArray.Bind();

    GLenum polygon_mode = mode == RenderMode_Points ? GL_POINT : GL_FILL;
    GLenum primitive_type = mode == RenderMode_Lines ? GL_LINES : GL_TRIANGLES;
    glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);

    auto &geom = ActiveGeometry();
    uint num_indices = mode == RenderMode_Lines ? geom.LineIndices.size() : geom.TriangleIndices.size();
    if (Transforms.size() == 1) {
        glDrawElements(primitive_type, num_indices, GL_UNSIGNED_INT, 0);
    } else {
        glDrawElementsInstanced(primitive_type, num_indices, GL_UNSIGNED_INT, 0, Transforms.size());
    }
}

void Mesh::SetPosition(const vec3 &position) {
    for (auto &transform : Transforms) {
        transform[3][0] = position.x;
        transform[3][1] = position.y;
        transform[3][2] = position.z;
    }
}
void Mesh::SetTransform(const mat4 &new_transform) {
    for (auto &transform : Transforms) transform = new_transform;
}
void Mesh::SetColor(const vec4 &color) {
    Colors.clear();
    Colors.resize(Transforms.size(), color);
}
