#include "MeshProfile.h"

#define NANOSVG_IMPLEMENTATION // Expands implementation
#include "nanosvg.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

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

ImVec2 MeshProfile::GetControlPoint(size_t i, const ImVec2 &offset, float scale) const {
    return control_points[i] * scale + offset;
}

vector<ImVec2> MeshProfile::CreateVertices(const float tolerance) const {
    const size_t num_ctrl = NumControlPoints();
    if (num_ctrl < 4) return {};

    static ImDrawListSharedData sharedData;
    sharedData.CurveTessellationTol = tolerance;

    // Use ImGui to create the Bezier curve vertices, to ensure that the vertices are identical to those rendered by ImGui.
    // Note: tolerance is scaled to normalized control points in [-1, 1], whereas tolerance in ImGui rendering is in pixels.
    // TODO should unify these two tolerances.
    static ImDrawList dl(&sharedData);
    dl.PathLineTo(control_points[0]);
    for (size_t i = 0; i < control_points.size() - 1; i += 3) {
        dl.PathBezierCubicCurveTo(control_points[i + 1], control_points[i + 2], control_points[i + 3]);
    }

    vector<ImVec2> vertices(dl._Path.Size);
    for (int i = 0; i < dl._Path.Size; i++) vertices.push_back(dl._Path[i]);

    dl.PathClear();

    return vertices;
}

// Render the current 2D profile as a closed line shape (using ImGui).
void MeshProfile::Render() const {
    const size_t num_ctrl = NumControlPoints();
    if (num_ctrl < 4) {
        ImGui::Text("The current mesh was not loaded from a 2D profile.");
        return;
    }

    const static float line_thickness = 2.f;
    const auto offset = ImGui::GetCursorScreenPos();
    // The profile is normalized to 1 based on its largest dimension.
    const float scale = ImGui::GetContentRegionAvail().y - line_thickness * 2;

    auto *dl = ImGui::GetWindowDrawList();
    dl->PathLineTo(GetControlPoint(0, offset, scale));
    for (size_t i = 0; i < num_ctrl - 1; i += 3) {
        dl->PathBezierCubicCurveTo(
            GetControlPoint(i + 1, offset, scale),
            GetControlPoint(i + 2, offset, scale),
            GetControlPoint(i + 3, offset, scale)
        );
    }
    dl->PathStroke(IM_COL32_WHITE, 0, line_thickness);

    // Draw control lines/points.

    // Control lines
    for (size_t i = 0; i < num_ctrl - 1; i += 3) {
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
    for (size_t i = 0; i < num_ctrl - 1; i += 3) {
        dl->AddCircleFilled(GetControlPoint(i + 3, offset, scale), 6.0f, IM_COL32_WHITE);
    }

    dl->AddCircleFilled(GetControlPoint(0, offset, scale), 3.0f, IM_COL32_BLACK);
    for (size_t i = 0; i < num_ctrl - 1; i += 3) {
        dl->AddCircleFilled(GetControlPoint(i + 1, offset, scale), 3.0f, IM_COL32_WHITE);
        dl->AddCircleFilled(GetControlPoint(i + 2, offset, scale), 3.0f, IM_COL32_WHITE);
        dl->AddCircleFilled(GetControlPoint(i + 3, offset, scale), 3.0f, IM_COL32_BLACK);
    }
}
