#include "Geometry.h"

#include <fstream>
#include <iostream>

#include <GL/glew.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

using glm::vec2, glm::vec3;
using std::string;

Geometry::Geometry() {
    glGenVertexArrays(1, &VertexArray);
    glGenBuffers(1, &VertexBuffer);
    glGenBuffers(1, &NormalBuffer);
    glGenBuffers(1, &IndexBuffer);
}
Geometry::Geometry(uint num_vertices, uint num_normals, uint num_indices) : Geometry() {
    Vertices.reserve(num_vertices);
    Normals.reserve(num_normals);
    Indices.reserve(num_indices);
}

Geometry::~Geometry() {
    glDeleteBuffers(1, &IndexBuffer);
    glDeleteBuffers(1, &NormalBuffer);
    glDeleteBuffers(1, &VertexBuffer);
    glDeleteVertexArrays(1, &VertexArray);
}

void Geometry::Bind() const {
    glBindVertexArray(VertexArray);

    // Bind vertices to layout location 0
    glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * Vertices.size(), &Vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // This allows usage of layout location 0 in the vertex shader
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

    // Bind normals to layout location 1
    glBindBuffer(GL_ARRAY_BUFFER, NormalBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * Normals.size(), &Normals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(1); // This allows usage of layout location 1 in the vertex shader
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * Indices.size(), &Indices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Geometry::Clear() {
    Vertices.clear();
    Normals.clear();
    Indices.clear();
    Min = {};
    Max = {};
}

void Geometry::Save(fs::path file_path) const {
    std::ofstream out(file_path.c_str());
    if (!out.is_open()) throw std::runtime_error(string("Error opening file: ") + file_path.string());

    out << std::setprecision(10);
    for (const vec3 &v : Vertices) {
        out << "v " << v.x << " " << v.y << " " << v.z << "\n";
    }
    for (const vec3 &n : Normals) {
        out << "vn " << n.x << " " << n.y << " " << n.z << "\n";
    }
    for (size_t i = 0; i < Indices.size(); i += 3) {
        out << "f " << Indices[i] + 1 << "//" << Indices[i] + 1 << " "
            << Indices[i + 1] + 1 << "//" << Indices[i + 1] + 1 << " "
            << Indices[i + 2] + 1 << "//" << Indices[i + 2] + 1 << "\n";
    }

    out.close();
}

void Geometry::Flip(bool x, bool y, bool z) {
    const vec3 flip(x ? -1 : 1, y ? -1 : 1, z ? -1 : 1);
    const vec3 center = (Min + Max) / 2.0f;
    for (auto &vertex : Vertices) vertex = center + (vertex - center) * flip;
    for (auto &normal : Normals) normal *= flip;
    UpdateBounds();
}
void Geometry::Rotate(const vec3 &axis, float angle) {
    const glm::qua rotation = glm::angleAxis(glm::radians(angle), glm::normalize(axis));
    for (auto &vertex : Vertices) vertex = rotation * vertex;
    for (auto &normal : Normals) normal = rotation * normal;
    UpdateBounds();
}
void Geometry::Scale(const vec3 &scale) {
    for (auto &vertex : Vertices) vertex *= scale;
    UpdateBounds();
}
void Geometry::Center() {
    const vec3 center = (Min + Max) / 2.0f;
    for (auto &vertex : Vertices) vertex -= center;
    UpdateBounds();
}

void Geometry::ComputeNormals() {
    if (!Normals.empty()) return;

    Normals.resize(Vertices.size());

    // Compute normals for each triangle.
    for (size_t i = 0; i < Indices.size(); i += 3) {
        const vec3 &v0 = Vertices[Indices[i]];
        const vec3 &v1 = Vertices[Indices[i + 1]];
        const vec3 &v2 = Vertices[Indices[i + 2]];
        const vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        Normals[Indices[i]] = Normals[Indices[i + 1]] = Normals[Indices[i + 2]] = normal;
    }
}

void Geometry::UpdateBounds() {
    // Update `Min`/`Max` based on the current vertices.
    Min = vec3(INFINITY, INFINITY, INFINITY);
    Max = vec3(-INFINITY, -INFINITY, -INFINITY);
    for (const vec3 &v : Vertices) {
        if (v.x < Min.x) Min.x = v.x;
        if (v.y < Min.y) Min.y = v.y;
        if (v.z < Min.z) Min.z = v.z;
        if (v.x > Max.x) Max.x = v.x;
        if (v.y > Max.y) Max.y = v.y;
        if (v.z > Max.z) Max.z = v.z;
    }
}

void Geometry::ExtrudeProfile(const vector<vec2> &profile_vertices, uint slices, bool closed) {
    Clear();
    if (profile_vertices.size() < 3) return;

    // The profile vertices are ordered clockwise, with the first vertex corresponding to the top/outside of the surface,
    // and last vertex corresponding the the bottom/inside of the surface.
    // If the profile is not closed (default), these top/bottom vertices will be connected in the middle of the extruded geometry,
    // creating a continuous connected solid "bridge" between all rotated slices.
    const int n = profile_vertices.size();
    const int start_index = closed ? 0 : 1;
    const int end_index = n - (closed ? 0 : 1);
    const int profile_size_no_connect = end_index - start_index;
    const int num_vertices = slices * profile_size_no_connect + (closed ? 0 : 2);
    const int num_indices = slices * (profile_size_no_connect + (closed ? -1 : 0)) * 6;
    Vertices.reserve(num_vertices);
    Normals.reserve(num_vertices);
    Indices.reserve(num_indices);

    const double angle_increment = 2.0 * M_PI / slices;
    for (uint slice = 0; slice < slices; slice++) {
        const double angle = slice * angle_increment;
        const double c = cos(angle);
        const double s = sin(angle);
        // Exclude the top/bottom vertices, which will be connected later.
        for (int i = start_index; i < end_index; i++) {
            const auto &p = profile_vertices[i];
            Vertices.push_back({p.x * c, p.y, p.x * s});
            Normals.push_back({c, 0.0, s});
        }
    }
    if (!closed) {
        Vertices.push_back({0.0, profile_vertices[0].y, 0.0});
        Normals.push_back({0.0, 0.0, 0.0});
        Vertices.push_back({0.0, profile_vertices[n - 1].y, 0.0});
        Normals.push_back({0.0, 0.0, 0.0});
    }

    // Compute indices for the triangles.
    for (uint slice = 0; slice < slices; slice++) {
        for (int i = 0; i < profile_size_no_connect - 1; i++) {
            const int base_index = slice * profile_size_no_connect + i;
            const int next_base_index = ((slice + 1) % slices) * profile_size_no_connect + i;
            // First triangle
            Indices.push_back(base_index);
            Indices.push_back(next_base_index + 1);
            Indices.push_back(base_index + 1);

            // Second triangle
            Indices.push_back(base_index);
            Indices.push_back(next_base_index);
            Indices.push_back(next_base_index + 1);
        }
    }

    // Connect the top and bottom.
    if (!closed) {
        for (uint slice = 0; slice < slices; slice++) {
            // Top
            Indices.push_back(slice * profile_size_no_connect);
            Indices.push_back(Vertices.size() - 2);
            Indices.push_back(((slice + 1) % slices) * profile_size_no_connect);
            // Bottom
            Indices.push_back(slice * profile_size_no_connect + n - 3);
            Indices.push_back(Vertices.size() - 1);
            Indices.push_back(((slice + 1) % slices) * profile_size_no_connect + n - 3);
        }
    }

    // SVG coordinates are upside-down relative to our 3D rendering coordinates.
    // However, they're correctly oriented top-to-bottom for 2D ImGui rendering, so we only invert the y-axis (the up/down axis).
    UpdateBounds();
    Flip(false, true, false);
    Center();
}
