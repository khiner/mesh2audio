#include "MeshProfile.h"

#define NANOSVG_IMPLEMENTATION // Expands implementation
#include "nanosvg.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include <algorithm>

MeshProfile::MeshProfile(fs::path svg_file_path) {
    if (svg_file_path.extension() != ".svg") throw std::runtime_error("Unsupported file type: " + svg_file_path.string());

    struct NSVGimage *image;
    image = nsvgParseFromFile(svg_file_path.c_str(), "px", 96);

    for (auto *shape = image->shapes; shape != nullptr; shape = shape->next) {
        for (auto *path = shape->paths; path != nullptr; path = path->next) {
            for (int i = 0; i < path->npts; i++) {
                float *p = &path->pts[i * 2];
                control_points.push_back({p[0], p[1]});
            }
        }
    }
    nsvgDelete(image);

    Normalize();
}

void MeshProfile::Normalize() {
    float max_dim = 0.0f;
    for (auto &v : control_points) {
        if (v.x > max_dim) max_dim = v.x;
        if (v.y > max_dim) max_dim = v.y;
    }
    for (auto &v : control_points) v /= max_dim;
}

int MeshProfile::NumControlPoints() const { return control_points.size(); }

ImVec2 MeshProfile::GetControlPoint(int i, const ImVec2 &offset, const float scale) const {
    return control_points[i] * scale + offset;
}

static float DistPtSeg(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3) {
    const auto d23 = p3 - p2;
    const auto d12 = p1 - p2;
    const float d_mag = d23.x * d23.x + d23.y * d23.y;
    const float t_mag = std::clamp(d23.x * d12.x + d23.y * d12.y / (d_mag > 0 ? d_mag : 1), 0.f, 1.f);

    const ImVec2 d = p2 + d23 * t_mag - p1;
    return d.x * d.x + d.y * d.y;
}

static void AddCubicBez(vector<ImVec2> &vertices, const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec2 &p4, float tol, int level = 0) {
    if (level > 12) return;

    const auto p12 = (p1 + p2) * 0.5f;
    const auto p23 = (p2 + p3) * 0.5f;
    const auto p34 = (p3 + p4) * 0.5f;
    const auto p123 = (p12 + p23) * 0.5f;
    const auto p234 = (p23 + p34) * 0.5f;
    const auto p1234 = (p123 + p234) * 0.5f;
    const float d = DistPtSeg(p1234, p1, p4);

    if (d > tol * tol) {
        AddCubicBez(vertices, p1, p12, p123, p1234, tol, level + 1);
        AddCubicBez(vertices, p1234, p234, p34, p4, tol, level + 1);
    } else {
        vertices.push_back(p4);
    }
}

vector<ImVec2> MeshProfile::CreateVertices(const float tolerance) {
    vector<ImVec2> vertices;
    for (int i = 0; i < int(control_points.size()) - 1; i += 3) {
        AddCubicBez(vertices, control_points[i], control_points[i + 1], control_points[i + 2], control_points[i + 3], tolerance);
    }

    return vertices;
}

// Render the current 2D profile as a closed line shape (using ImGui).
void MeshProfile::Render() const {
    const int num_ctrl = NumControlPoints();
    if (num_ctrl == 0) {
        ImGui::Text("The current mesh was not loaded from a 2D profile.");
        return;
    }

    const static float line_thickness = 2.f;
    const auto offset = ImGui::GetCursorScreenPos();
    // The profile is normalized to 1 based on its largest dimension.
    const float scale = ImGui::GetContentRegionAvail().y - line_thickness * 2;

    auto *dl = ImGui::GetWindowDrawList();
    dl->PathLineTo(GetControlPoint(0, offset, scale));
    for (int i = 0; i < num_ctrl - 1; i += 3) {
        dl->PathBezierCubicCurveTo(
            GetControlPoint(i + 1, offset, scale),
            GetControlPoint(i + 2, offset, scale),
            GetControlPoint(i + 3, offset, scale),
            0
        );
    }
    dl->PathStroke(IM_COL32_WHITE, 0, line_thickness);

    // Draw control lines/points.

    // Control lines
    for (int i = 0; i < num_ctrl - 1; i += 3) {
        dl->AddLine(
            GetControlPoint(i, offset, scale),
            GetControlPoint(i + 1, offset, scale),
            IM_COL32_WHITE, line_thickness
        );
        dl->AddLine(
            GetControlPoint(i + 2, offset, scale),
            GetControlPoint(i + 3, offset, scale),
            IM_COL32_WHITE, line_thickness
        );
    }

    // Control points
    for (int i = 0; i < num_ctrl - 1; i += 3) {
        dl->AddCircleFilled(GetControlPoint(i + 3, offset, scale), 6.0f, IM_COL32_WHITE);
    }

    dl->AddCircleFilled(GetControlPoint(0, offset, scale), 3.0f, IM_COL32_BLACK);
    for (int i = 0; i < num_ctrl - 1; i += 3) {
        dl->AddCircleFilled(GetControlPoint(i + 1, offset, scale), 3.0f, IM_COL32_WHITE);
        dl->AddCircleFilled(GetControlPoint(i + 2, offset, scale), 3.0f, IM_COL32_WHITE);
        dl->AddCircleFilled(GetControlPoint(i + 3, offset, scale), 3.0f, IM_COL32_BLACK);
    }
}
