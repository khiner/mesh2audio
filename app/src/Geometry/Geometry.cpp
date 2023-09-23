#include "Geometry.h"

#include <fstream>
#include <iostream>

#include <glm/gtx/string_cast.hpp>

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;
using std::string;

Geometry::Geometry(uint num_vertices, uint num_normals, uint num_indices)
    : Vertices(num_vertices), Normals(num_normals), Indices(num_indices) {
    glGenVertexArrays(1, &VertexArray);
    glBindVertexArray(VertexArray);

    Vertices.Bind();
    glEnableVertexAttribArray(0); // `layout (location = 0)` in the vertex shader
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

    Normals.Bind();
    glEnableVertexAttribArray(1); // `layout (location = 1)` in the vertex shader
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

    InstanceModels.Bind();
    // Since a `mat4` is actually 4 `vec4`s, we need to enable four attributes for it.
    for (int i = 0; i < 4; i++) {
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(vec4), (GLvoid *)(i * sizeof(vec4)));
        glEnableVertexAttribArray(2 + i);
        glVertexAttribDivisor(2 + i, 1); // Attribute is updated once per instance.
    }

    InstanceColors.Bind();
    glEnableVertexAttribArray(6); // `layout (location = 6)` in the vertex shader
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glVertexAttribDivisor(6, 1); // Attribute is updated once per instance.

    glBindVertexArray(0);
}

Geometry::Geometry(fs::path file_path) : Geometry() { Load(file_path); }

Geometry::~Geometry() {
    glDeleteVertexArrays(1, &VertexArray);
}

void Geometry::Load(fs::path file_path) {
    if (file_path.extension() != ".obj") throw std::runtime_error("Unsupported file type: " + file_path.string());

    FILE *fp;
    fp = fopen(file_path.c_str(), "rb");
    if (fp == nullptr) throw std::runtime_error("Error loading file: " + file_path.string());

    float x, y, z;
    int fx, fy, fz, ignore;
    int c1, c2;
    while (!feof(fp)) {
        c1 = fgetc(fp);
        while (!(c1 == 'v' || c1 == 'f')) {
            c1 = fgetc(fp);
            if (feof(fp)) break;
        }
        c2 = fgetc(fp);
        if ((c1 == 'v') && (c2 == ' ')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            Vertices.push_back({x, y, z});
        } else if ((c1 == 'v') && (c2 == 'n')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            Normals.push_back(glm::normalize(vec3(x, y, z)));
        } else if (c1 == 'f') {
            fscanf(fp, "%d", &fx);
            int first_char = fgetc(fp);
            int second_char = fgetc(fp);
            if (first_char == '/' && second_char == '/') {
                fscanf(fp, "%d %d//%d %d//%d", &ignore, &fy, &ignore, &fz, &ignore);
            } else {
                ungetc(second_char, fp);
                fscanf(fp, "%d %d/%d %d/%d", &ignore, &fy, &ignore, &fz, &ignore);
            }
            Indices.push_back(fx - 1);
            Indices.push_back(fy - 1);
            Indices.push_back(fz - 1);
        }
    }
    fclose(fp);

    ComputeNormals();
    UpdateBounds();
    Center();
}

void Geometry::BindData() const {
    glBindVertexArray(VertexArray);
    Vertices.BindData();
    Normals.BindData();
    Indices.BindData();
    InstanceModels.BindData();
    InstanceColors.BindData();
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
    Vertices -= center;
    Vertices *= flip;
    Vertices += center;
    Normals *= flip;
    UpdateBounds();
}
void Geometry::Rotate(const vec3 &axis, float angle) {
    const glm::qua rotation = glm::angleAxis(glm::radians(angle), glm::normalize(axis));
    Vertices *= rotation;
    Normals *= rotation;
    UpdateBounds();
}
void Geometry::Scale(const vec3 &scale) {
    Vertices *= scale;
    UpdateBounds();
}
void Geometry::Center() {
    Vertices -= (Min + Max) / 2.0f;
    UpdateBounds();
}

void Geometry::Translate(const glm::vec3 &translation) {
    Vertices += translation;
    UpdateBounds();
}

void Geometry::SetColor(const vec4 &color) {
    InstanceColors.clear();
    InstanceColors.resize(InstanceModels.size(), color);
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
        for (int j = 0; j < 3; j++) Normals[Indices[i + j]] = normal;
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

    for (uint slice = 0; slice < slices; slice++) {
        const float ratio = 2 * float(slice) / slices;
        const float c = __cospif(ratio);
        const float s = __sinpif(ratio);
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
            const uint base_index = slice * profile_size_no_connect + i;
            const uint next_base_index = ((slice + 1) % slices) * profile_size_no_connect + i;
            Indices.append({
                // First triangle
                base_index,
                next_base_index + 1,
                base_index + 1,
                // Second triangle
                base_index,
                next_base_index,
                next_base_index + 1,
            });
        }
    }

    // Connect the top and bottom.
    if (!closed) {
        for (uint slice = 0; slice < slices; slice++) {
            Indices.append({
                // Top
                uint(Vertices.size() - 2),
                slice * profile_size_no_connect,
                ((slice + 1) % slices) * profile_size_no_connect,
                // Bottom
                uint(Vertices.size() - 1),
                slice * profile_size_no_connect + n - 3,
                ((slice + 1) % slices) * profile_size_no_connect + n - 3,
            });
        }
    }

    // SVG coordinates are upside-down relative to our 3D rendering coordinates.
    // However, they're correctly oriented top-to-bottom for 2D ImGui rendering, so we only invert the y-axis (the up/down axis).
    UpdateBounds();
    Flip(false, true, false);
    Center();
}
