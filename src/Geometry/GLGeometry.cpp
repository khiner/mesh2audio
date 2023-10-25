#include "GLGeometry.h"

void GLGeometry::Generate() {
    VertexBuffer.Generate();
    NormalBuffer.Generate();
    IndexBuffer.Generate();
}

void GLGeometry::EnableVertexAttributes() const {
    VertexBuffer.Bind();
    static const GLuint VertexSlot = 0;
    glEnableVertexAttribArray(VertexSlot);
    glVertexAttribPointer(VertexSlot, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);

    NormalBuffer.Bind();
    static const GLuint NormalSlot = 1;
    glEnableVertexAttribArray(NormalSlot);
    glVertexAttribPointer(NormalSlot, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);

    Dirty = true;
}

void GLGeometry::Delete() const {
    VertexBuffer.Delete();
    NormalBuffer.Delete();
    IndexBuffer.Delete();
}

void GLGeometry::BindData(RenderMode render_mode) const {
    if (Dirty || render_mode != LastBoundRenderMode) {
        VertexBuffer.SetData(Vertices);
        if (!Normals.empty()) NormalBuffer.SetData(Normals);
        IndexBuffer.SetData(Indices);
    }
    LastBoundRenderMode = render_mode;
    Dirty = false;
}
