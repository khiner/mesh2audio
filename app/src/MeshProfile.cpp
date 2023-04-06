#include "MeshProfile.h"

#define NANOSVG_IMPLEMENTATION // Expands implementation
#include "nanosvg.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

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

int MeshProfile::NumControlPoints() const { return control_points.size(); }

ImVec2 MeshProfile::GetControlPoint(int i, const ImVec2 &offset, const float scale) const {
    return control_points[i] * scale + offset;
}

static float DistPtSeg(float x, float y, float px, float py, float qx, float qy) {
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

static void AddCubicBez(vector<ImVec2> &vertices, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tol, int level = 0) {
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

    d = DistPtSeg(x1234, y1234, x1, y1, x4, y4);
    if (d > tol * tol) {
        AddCubicBez(vertices, x1, y1, x12, y12, x123, y123, x1234, y1234, tol, level + 1);
        AddCubicBez(vertices, x1234, y1234, x234, y234, x34, y34, x4, y4, tol, level + 1);
    } else {
        vertices.push_back({x4, y4});
    }
}

vector<ImVec2> MeshProfile::CreateVertices(const float tolerance) {
    vector<ImVec2> vertices;
    for (int i = 0; i < int(control_points.size()) - 1; i += 3) {
        const auto p1 = control_points[i];
        const auto p2 = control_points[i + 1];
        const auto p3 = control_points[i + 2];
        const auto p4 = control_points[i + 3];
        AddCubicBez(vertices, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y, tolerance);
    }

    return vertices;
}

void MeshProfile::Normalize() {
    float max_dim = 0.0f;
    for (auto &v : control_points) {
        if (v.x > max_dim) max_dim = v.x;
        if (v.y > max_dim) max_dim = v.y;
    }
    for (auto &v : control_points) v /= max_dim;
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
