#include "Geometry.h"

#include <fstream>
#include <iostream>

#include <glm/gtx/string_cast.hpp>

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;
using std::string;

Geometry::Geometry(uint num_vertices, uint num_normals, uint num_indices)
    : Vertices(num_vertices), Normals(num_normals), Indices(num_indices) {
    glGenVertexArrays(1, &VertexArray);
    EnableVertexAttributes();
}

Geometry::Geometry(fs::path file_path) : Geometry() { Load(file_path); }

Geometry::~Geometry() {
    glDeleteVertexArrays(1, &VertexArray);
}

void Geometry::EnableVertexAttributes(bool full_transforms) const {
    glBindVertexArray(VertexArray);

    Vertices.Bind();
    Vertices.EnableVertexAttribute(0);

    Normals.Bind();
    Normals.EnableVertexAttribute(1);

    Colors.Bind();
    Colors.EnableVertexAttribute(2);

    if (full_transforms) {
        Transforms.Bind();
        Transforms.EnableVertexAttribute(3);
    } else {
        TranslateScales.Bind();
        TranslateScales.EnableVertexAttribute(3);
    }
    glBindVertexArray(0);
}

void Geometry::BindData() const {
    glBindVertexArray(VertexArray);
    Vertices.BindData();
    Normals.BindData();
    Indices.BindData();
    Transforms.BindData();
    TranslateScales.BindData();
    Colors.BindData();
    glBindVertexArray(0);
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

    CenterVertices();
    ComputeNormals();
}

void Geometry::CenterVertices() {
    vec3 min = vec3(INFINITY, INFINITY, INFINITY);
    vec3 max = vec3(-INFINITY, -INFINITY, -INFINITY);
    for (const vec3 &v : Vertices) {
        if (v.x < min.x) min.x = v.x;
        if (v.y < min.y) min.y = v.y;
        if (v.z < min.z) min.z = v.z;
        if (v.x > max.x) max.x = v.x;
        if (v.y > max.y) max.y = v.y;
        if (v.z > max.z) max.z = v.z;
    }
    Vertices -= (min + max) / 2.0f;
}

void Geometry::Clear() {
    Vertices.clear();
    Normals.clear();
    Indices.clear();
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

void Geometry::SetTransform(const mat4 &transform) {
    for (auto &instance_model : Transforms) instance_model = transform;
}
void Geometry::SetColor(const vec4 &color) {
    Colors.clear();
    Colors.resize(Transforms.size(), color);
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

    CenterVertices();
    // SVG coordinates are upside-down relative to our 3D rendering coordinates.
    // However, they're correctly oriented top-to-bottom for 2D ImGui rendering, so we only invert the y-axis (the up/down axis).
    Vertices *= {1.0, -1.0, 1.0};
}
