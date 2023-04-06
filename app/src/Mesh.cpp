#include "Mesh.h"

#include <GL/glew.h>
#include <glm/geometric.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

Mesh::Mesh(fs::path file_path) {
    glGenVertexArrays(1, &vertex_array);
    glGenBuffers(1, &vertex_buffer);
    glGenBuffers(1, &normal_buffer);
    glGenBuffers(1, &index_buffer);

    const bool is_svg = file_path.extension() == ".svg";
    const bool is_obj = file_path.extension() == ".obj";
    if (!is_svg && !is_obj) throw std::runtime_error("Unsupported file type: " + file_path.string());

    if (is_svg) {
        profile = std::make_unique<MeshProfile>(file_path);
        ExtrudeProfile();
        // SVG coordinates are upside-down relative to our 3D rendering coordinates.
        // However, they're correctly oriented top-to-bottom for 2D ImGui rendering, so we only invert the 3D mesh - not the profile.
        InvertY();

        return;
    }

    FILE *fp;
    fp = fopen(file_path.c_str(), "rb");
    if (fp == nullptr) throw std::runtime_error("Error loading file: " + file_path.string());

    float x, y, z;
    int fx, fy, fz, ignore;
    int c1, c2;
    float y_min = INFINITY, z_min = INFINITY;
    float y_max = -INFINITY, z_max = -INFINITY;

    while (!feof(fp)) {
        c1 = fgetc(fp);
        while (!(c1 == 'v' || c1 == 'f')) {
            c1 = fgetc(fp);
            if (feof(fp)) break;
        }
        c2 = fgetc(fp);
        if ((c1 == 'v') && (c2 == ' ')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            vertices.push_back({x, y, z});
            if (y < y_min) y_min = y;
            if (z < z_min) z_min = z;
            if (y > y_max) y_max = y;
            if (z > z_max) z_max = z;
        } else if ((c1 == 'v') && (c2 == 'n')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            normals.push_back(glm::normalize(vec3(x, y, z)));
        } else if (c1 == 'f') {
            fscanf(fp, "%d//%d %d//%d %d//%d", &fx, &ignore, &fy, &ignore, &fz, &ignore);
            indices.push_back(fx - 1);
            indices.push_back(fy - 1);
            indices.push_back(fz - 1);
        }
    }
    fclose(fp);

    const float y_avg = (y_min + y_max) / 2.0f - 0.02f;
    const float z_avg = (z_min + z_max) / 2.0f;
    for (unsigned int i = 0; i < vertices.size(); ++i) {
        vertices[i] -= vec3(0.0f, y_avg, z_avg);
        vertices[i] *= vec3(1.58f, 1.58f, 1.58f);
    }
}

Mesh::~Mesh() {
    glDeleteBuffers(1, &index_buffer);
    glDeleteBuffers(1, &normal_buffer);
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteVertexArrays(1, &vertex_array);
}

void Mesh::Bind() const {
    glBindVertexArray(vertex_array);

    // Bind vertices to layout location 0
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // This allows usage of layout location 0 in the vertex shader
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    // Bind normals to layout location 1
    glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * normals.size(), &normals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(1); // This allows usage of layout location 1 in the vertex shader
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), &indices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Mesh::InvertY() {
    float min_y = INFINITY, max_y = -INFINITY;
    for (auto &v : vertices) {
        if (v.y < min_y) min_y = v.y;
        if (v.y > max_y) max_y = v.y;
    }
    for (auto &v : vertices) v.y = max_y - (v.y - min_y);
}

void Mesh::ExtrudeProfile(int num_radial_slices) {
    if (profile == nullptr) return;

    const vector<ImVec2> profile_vertices = profile->CreateVertices(0.0001);
    const double angle_increment = 2.0 * M_PI / num_radial_slices;
    for (int i = 0; i < num_radial_slices; i++) {
        const double angle = i * angle_increment;
        for (const auto &p : profile_vertices) {
            // Compute the x and y coordinates for this point on the extruded surface.
            const double x = p.x * cos(angle);
            const double y = p.y; // Use the original z-coordinate from the 2D path
            const double z = p.x * sin(angle);
            vertices.push_back({x, y, z});

            // Compute the normal for this vertex.
            const double nx = cos(angle);
            const double nz = sin(angle);
            normals.push_back({nx, 0.0, nz});
        }
    }

    // Compute indices for the triangles.
    for (int i = 0; i < num_radial_slices; i++) {
        for (int j = 0; j < int(profile_vertices.size() - 1); j++) {
            const int base_index = i * profile_vertices.size() + j;
            const int next_base_index = ((i + 1) % num_radial_slices) * profile_vertices.size() + j;

            // First triangle
            indices.push_back(base_index);
            indices.push_back(next_base_index + 1);
            indices.push_back(base_index + 1);

            // Second triangle
            indices.push_back(base_index);
            indices.push_back(next_base_index);
            indices.push_back(next_base_index + 1);
        }
    }
}

void Mesh::RenderProfile() const {
    if (profile != nullptr && profile->NumControlPoints() > 0) profile->Render();
    else ImGui::Text("The current mesh was not loaded from a 2D profile.");
}
