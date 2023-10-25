#include "Mesh.h"

void Mesh::Generate() {
    VertexArray.Generate();
    ColorBuffer.Generate();
    TransformBuffer.Generate();
    for (const auto *geom : static_cast<const Mesh *>(this)->AllGeometries()) {
        const_cast<GLGeometry *>(geom)->Generate();
    }
    EnableVertexAttributes();
}

void Mesh::Delete() const {
    VertexArray.Delete();
    TransformBuffer.Delete();
    ColorBuffer.Delete();
    for (const auto *geom : AllGeometries()) geom->Delete();
}

void Mesh::EnableVertexAttributes() const {
    VertexArray.Bind();
    ActiveGeometry().EnableVertexAttributes();

    ColorBuffer.Bind();
    static const GLuint ColorSlot = 2;
    glEnableVertexAttribArray(ColorSlot);
    glVertexAttribPointer(ColorSlot, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
    glVertexAttribDivisor(ColorSlot, 1); // Attribute is updated once per instance.

    TransformBuffer.Bind();
    // Since a `mat4` is actually 4 `vec4`s, we need to enable four attributes for it.
    for (int i = 0; i < 4; i++) {
        static const GLuint TransformSlot = 3;
        glEnableVertexAttribArray(TransformSlot + i);
        glVertexAttribPointer(TransformSlot + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (GLvoid *)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(TransformSlot + i, 1); // Attribute is updated once per instance.
    }
    VertexArray.Unbind();
}

void Mesh::BindData(RenderMode render_mode) const {
    VertexArray.Bind();
    ActiveGeometry().BindData(render_mode);

    if (Dirty) {
        if (Parent) {
            AbsoluteTransforms = Transforms;
            for (auto &transform : AbsoluteTransforms) transform = Parent->GetTransform() * transform;
            TransformBuffer.SetData(AbsoluteTransforms);
        } else {
            TransformBuffer.SetData(Transforms);
        }
        ColorBuffer.SetData(Colors);
    }
    Dirty = false;

    VertexArray.Unbind();
}

void Mesh::Render(RenderMode mode) const {
    if (Transforms.empty()) return;

    BindData(mode); // Only rebinds the data if it has changed.
    VertexArray.Bind();

    GLenum polygon_mode = mode == RenderMode::Points ? GL_POINT : GL_FILL;
    GLenum primitive_type = mode == RenderMode::Lines ? GL_LINES : GL_TRIANGLES;
    glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);

    uint num_indices = ActiveGeometry().NumIndices();
    if (Transforms.size() == 1) {
        glDrawElements(primitive_type, num_indices, GL_UNSIGNED_INT, 0);
    } else {
        glDrawElementsInstanced(primitive_type, num_indices, GL_UNSIGNED_INT, 0, Transforms.size());
    }
}
