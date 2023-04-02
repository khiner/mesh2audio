#include "Mesh.h"
#include "Transform.h"

#include <GL/glew.h>

#define NANOSVG_IMPLEMENTATION // Expands implementation
#include "nanosvg.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

namespace gl {
void Mesh::Init() {
    glGenVertexArrays(1, &vertex_array);
    glGenBuffers(1, &vertex_buffer);
    glGenBuffers(1, &normal_buffer);
    glGenBuffers(1, &index_buffer);
}

void Mesh::Destroy() {
    glDeleteBuffers(1, &index_buffer);
    glDeleteBuffers(1, &normal_buffer);
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteVertexArrays(1, &vertex_array);

    vertices.clear();
    normals.clear();
    indices.clear();
    profile_vertices.clear();
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

void Mesh::Load(fs::path file_path) {
    const bool is_svg = file_path.extension() == ".svg";
    const bool is_obj = file_path.extension() == ".obj";
    if (!is_svg && !is_obj) throw std::runtime_error("Unsupported file type: " + file_path.string());

    if (is_svg) {
        struct NSVGimage *image;
        image = nsvgParseFromFile(file_path.c_str(), "px", 96);

        static const float tol = 6;
        for (auto *shape = image->shapes; shape != nullptr; shape = shape->next) {
            for (auto *path = shape->paths; path != nullptr; path = path->next) {
                for (int i = 0; i < path->npts; i++) {
                    float *p = &path->pts[i * 2];
                    control_points.push_back({p[0], p[1]});
                }
                for (int i = 0; i < path->npts - 1; i += 3) {
                    float *p = &path->pts[i * 2];
                    CubicBez(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], tol);
                }
            }
        }
        nsvgDelete(image);

        NormalizeProfile();
        ExtrudeProfile();
        // SVG coordinates are upside-down relative to our 3D rendering coordinates.
        // However, they're correctly oriented top-to-bottom for ImGui rendering, so we only invert the 3D mesh.
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

void Mesh::NormalizeProfile() {
    float max_dim = 0.0f;
    for (auto &v : profile_vertices) {
        if (v.x > max_dim) max_dim = v.x;
        if (v.y > max_dim) max_dim = v.y;
    }
    for (auto &v : profile_vertices) v /= max_dim;
    for (auto &v : control_points) v /= max_dim;
}

void Mesh::InvertY() {
    float min_y = INFINITY, max_y = -INFINITY;
    for (auto &v : vertices) {
        if (v.y < min_y) min_y = v.y;
        if (v.y > max_y) max_y = v.y;
    }
    for (auto &v : vertices) v.y = max_y - (v.y - min_y);
}

void Mesh::CenterProfileY() {
    float min_y = INFINITY, max_y = -INFINITY;
    for (const auto &v : profile_vertices) {
        if (v.y < min_y) min_y = v.y;
        if (v.y > max_y) max_y = v.y;
    }
    for (auto &v : profile_vertices) v.y -= (min_y + max_y) / 2;
    for (auto &v : control_points) v.y -= (min_y + max_y) / 2;
}

void Mesh::ExtrudeProfile(int num_radial_slices) {
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

// Render the current 2D profile as a closed line shape (using ImGui).
void Mesh::RenderProfile() const {
    const int num_vertices = profile_vertices.size();
    const int num_ctrl = control_points.size();
    if (num_vertices == 0) {
        ImGui::Text("The current mesh is not based on a 2D profile.");
        return;
    }

    const static float line_thickness = 2.f;

    // TODO memoize
    ImVec2 vecs[num_vertices], control_vecs[num_ctrl];

    const auto screen_pos = ImGui::GetCursorScreenPos();
    // The profile is normalized to 1 based on its largest dimension.
    const float scale = ImGui::GetContentRegionAvail().y - line_thickness * 2;
    for (int i = 0; i < num_vertices; i++) {
        vecs[i] = {profile_vertices[i].x, profile_vertices[i].y};
        vecs[i] *= scale;
        vecs[i] += screen_pos;
    }
    for (int i = 0; i < num_ctrl; i++) {
        control_vecs[i] = {control_points[i].x, control_points[i].y};
        control_vecs[i] *= scale;
        control_vecs[i] += screen_pos;
    }
    auto *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddPolyline(vecs, profile_vertices.size(), IM_COL32_WHITE, ImDrawFlags_Closed, line_thickness);

    // Draw control lines/points.

    // Control lines
    for (int i = 0; i < num_ctrl - 1; i += 3) {
        const auto &p1 = control_vecs[i];
        const auto &p2 = control_vecs[i + 1];
        const auto &p3 = control_vecs[i + 2];
        const auto &p4 = control_vecs[i + 3];
        draw_list->AddLine({p1.x, p1.y}, {p2.x, p2.y}, IM_COL32_WHITE, line_thickness);
        draw_list->AddLine({p3.x, p3.y}, {p4.x, p4.y}, IM_COL32_WHITE, line_thickness);
    }

    // Control points
    for (int i = 0; i < num_ctrl - 1; i += 3) {
        const auto &p4 = control_vecs[i + 3];
        draw_list->AddCircleFilled({p4.x, p4.y}, 6.0f, IM_COL32_WHITE);
    }

    const auto &p1 = control_points[0];
    draw_list->AddCircleFilled({p1.x, p1.y}, 3.0f, IM_COL32_BLACK);
    for (int i = 0; i < num_ctrl - 1; i += 3) {
        const auto &p2 = control_vecs[i + 1];
        const auto &p3 = control_vecs[i + 2];
        const auto &p4 = control_vecs[i + 3];
        draw_list->AddCircleFilled({p2.x, p2.y}, 3.0f, IM_COL32_WHITE);
        draw_list->AddCircleFilled({p3.x, p3.y}, 3.0f, IM_COL32_WHITE);
        draw_list->AddCircleFilled({p4.x, p4.y}, 3.0f, IM_COL32_BLACK);
    }
}

static float dist_pt_seg(float x, float y, float px, float py, float qx, float qy) {
    const float pqx = qx - px;
    const float pqy = qy - py;
    float dx = x - px;
    float dy = y - py;
    const float d = pqx * pqx + pqy * pqy;
    float t = pqx * dx + pqy * dy;
    if (d > 0) t /= d;
    if (t < 0) t = 0;
    else if (t > 1) t = 1;

    dx = px + t * pqx - x;
    dy = py + t * pqy - y;

    return dx * dx + dy * dy;
}

// Private:

void Mesh::CubicBez(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tol, int level) {
    float x12, y12, x23, y23, x34, y34, x123, y123, x234, y234, x1234, y1234;
    float d;

    if (level > 12) return;

    x12 = (x1 + x2) * 0.5f;
    y12 = (y1 + y2) * 0.5f;
    x23 = (x2 + x3) * 0.5f;
    y23 = (y2 + y3) * 0.5f;
    x34 = (x3 + x4) * 0.5f;
    y34 = (y3 + y4) * 0.5f;
    x123 = (x12 + x23) * 0.5f;
    y123 = (y12 + y23) * 0.5f;
    x234 = (x23 + x34) * 0.5f;
    y234 = (y23 + y34) * 0.5f;
    x1234 = (x123 + x234) * 0.5f;
    y1234 = (y123 + y234) * 0.5f;

    d = dist_pt_seg(x1234, y1234, x1, y1, x4, y4);
    if (d > tol * tol) {
        CubicBez(x1, y1, x12, y12, x123, y123, x1234, y1234, tol, level + 1);
        CubicBez(x1234, y1234, x234, y234, x34, y34, x4, y4, tol, level + 1);
    } else {
        profile_vertices.push_back({x4, y4});
    }
}

} // namespace gl
