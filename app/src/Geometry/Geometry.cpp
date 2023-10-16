#include "Geometry.h"

#include <unordered_set>

#include <glm/gtx/string_cast.hpp>

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;
using std::string;

void Geometry::Generate() {
    VertexBuffer.Generate();
    NormalBuffer.Generate();
    IndexBuffer.Generate();
}

void Geometry::EnableVertexAttributes() const {
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

void Geometry::Delete() const {
    VertexBuffer.Delete();
    NormalBuffer.Delete();
    IndexBuffer.Delete();
}

// Any mutating changes before each render.
void Geometry::PrepareRender(RenderMode) {}

void Geometry::BindData(RenderMode render_mode) const {
    if (Dirty || render_mode != LastBoundRenderMode) {
        VertexBuffer.SetData(GetVertices());
        if (HasNormals()) NormalBuffer.SetData(GetNormals());
        IndexBuffer.SetData(render_mode == RenderMode::Lines ? GetLineIndices() : GetIndices());
    }
    LastBoundRenderMode = render_mode;
    Dirty = false;
}
