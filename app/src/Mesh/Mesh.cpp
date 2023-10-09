#include "Mesh.h"

using glm::vec3, glm::vec4, glm::mat4;

void Mesh::Generate() {
    VertexArray.Generate();
    Colors.Generate();
    Transforms.Generate();

    std::vector<const Geometry *> const_geometries = static_cast<const Mesh *>(this)->AllGeometries();
    for (const auto *const_geom : const_geometries) {
        auto *geom = const_cast<Geometry *>(const_geom);
        geom->Vertices.Generate();
        geom->Normals.Generate();
        geom->TriangleIndices.Generate();
        geom->LineIndices.Generate();
    }

    EnableVertexAttributes();
}

void Mesh::Delete() const {
    VertexArray.Delete();

    Transforms.Delete();
    Colors.Delete();
    for (const auto *geom : AllGeometries()) {
        geom->Vertices.Delete();
        geom->Normals.Delete();
        geom->TriangleIndices.Delete();
        geom->LineIndices.Delete();
    }
}

void Mesh::EnableVertexAttributes() const {
    VertexArray.Bind();
    const auto &geom = ActiveGeometry();
    geom.Vertices.EnableVertexAttribute(0);
    geom.Normals.EnableVertexAttribute(1);
    Colors.EnableVertexAttribute(2);
    Transforms.EnableVertexAttribute(3);
    VertexArray.Unbind();
}

void Mesh::BindData(RenderMode render_mode) const {
    VertexArray.Bind();
    auto &geom = ActiveGeometry();
    geom.Vertices.BindData();
    if (!geom.Normals.empty()) geom.Normals.BindData();
    const auto &indices = render_mode == RenderMode_Lines ? geom.LineIndices : geom.TriangleIndices;
    indices.BindData();
    Transforms.BindData();
    Colors.BindData();
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
