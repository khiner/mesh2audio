#include "Geometry.h"

#include <unordered_set>

#include <glm/gtx/string_cast.hpp>

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;
using std::string;

void Geometry::Generate() {
    VertexBuffer.Generate();
    NormalBuffer.Generate();
    TriangleIndexBuffer.Generate();
    LineIndexBuffer.Generate();
}

void Geometry::EnableVertexAttributes() const {
    static const GLuint VertexSlot = 0;
    static const GLuint NormalSlot = 1;
    VertexBuffer.Bind();
    glEnableVertexAttribArray(VertexSlot);
    glVertexAttribPointer(VertexSlot, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    NormalBuffer.Bind();
    glEnableVertexAttribArray(NormalSlot);
    glVertexAttribPointer(NormalSlot, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    Dirty = true;
}

void Geometry::Delete() const {
    VertexBuffer.Delete();
    NormalBuffer.Delete();
    TriangleIndexBuffer.Delete();
    LineIndexBuffer.Delete();
}

// Any mutating changes before each render.
void Geometry::PrepareRender(RenderMode) {}

void Geometry::BindData(RenderMode render_mode) const {
    if (Dirty) {
        VertexBuffer.SetData(GetVertices());
        if (HasNormals()) NormalBuffer.SetData(GetNormals());
        if (render_mode == RenderMode::Lines) LineIndexBuffer.SetData(GetLineIndices());
        else TriangleIndexBuffer.SetData(GetIndices());
    }
    Dirty = false;
}
