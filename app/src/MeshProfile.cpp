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
                ControlPoints.push_back({p[0], p[1]});
            }
        }
    }
    nsvgDelete(image);

    Normalize();
    SvgFilePath = svg_file_path;
}

void MeshProfile::Normalize() {
    float max_dim = 0.0f;
    for (auto &v : ControlPoints) {
        if (v.x > max_dim) max_dim = v.x;
        if (v.y > max_dim) max_dim = v.y;
    }
    for (auto &v : ControlPoints) v /= max_dim;
}

int MeshProfile::NumControlPoints() const { return ControlPoints.size(); }

ImVec2 MeshProfile::GetControlPoint(size_t i, const ImVec2 &offset, float scale) const {
    return ControlPoints[i] * scale + offset;
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
    dl.PathLineTo(ControlPoints[0]);
    for (size_t i = 0; i < ControlPoints.size() - 1; i += 3) {
        dl.PathBezierCubicCurveTo(ControlPoints[i + 1], ControlPoints[i + 2], ControlPoints[i + 3]);
    }

    vector<ImVec2> vertices(dl._Path.Size);
    for (int i = 0; i < dl._Path.Size; i++) vertices.push_back(dl._Path[i]);

    dl.PathClear();

    return vertices;
}

// Render the current 2D profile as a closed line shape (using ImGui).
void MeshProfile::Render() const {
    const size_t num_ctrl = NumControlPoints();
    if (num_ctrl < 4) return;

    const float spacing = 2 + std::max(PathLineThickness, std::max(AnchorPointRadius, ControlPointRadius));
    const auto offset = ImGui::GetCursorScreenPos() + ImVec2{spacing, spacing};
    // The profile is normalized to 1 based on its largest dimension.
    const float scale = ImGui::GetContentRegionAvail().y - 2 * spacing;

    const auto path_line_color = ImGui::ColorConvertFloat4ToU32({PathLineColor[0], PathLineColor[1], PathLineColor[2], PathLineColor[3]});

    auto *dl = ImGui::GetWindowDrawList();
    if (ShowPath) {
        dl->PathLineTo(GetControlPoint(0, offset, scale));
        for (size_t i = 0; i < num_ctrl - 1; i += 3) {
            dl->PathBezierCubicCurveTo(
                GetControlPoint(i + 1, offset, scale),
                GetControlPoint(i + 2, offset, scale),
                GetControlPoint(i + 3, offset, scale)
            );
        }
        dl->PathStroke(path_line_color, 0, PathLineThickness);
    }

    if (ShowControlPoints) {
        const auto control_color = ImGui::ColorConvertFloat4ToU32({ControlColor[0], ControlColor[1], ControlColor[2], ControlColor[3]});
        for (size_t i = 0; i < num_ctrl - 1; i += 3) {
            dl->PathLineTo(GetControlPoint(i, offset, scale));
            dl->PathLineTo(GetControlPoint(i + 1, offset, scale));
            dl->PathStroke(control_color, 0, ControlLineThickness);
            dl->PathLineTo(GetControlPoint(i + 2, offset, scale));
            dl->PathLineTo(GetControlPoint(i + 3, offset, scale));
            dl->PathStroke(control_color, 0, ControlLineThickness);
        }
        dl->AddCircleFilled(GetControlPoint(0, offset, scale), ControlPointRadius, control_color);
        for (size_t i = 0; i < num_ctrl - 1; i += 3) {
            dl->AddCircleFilled(GetControlPoint(i + 1, offset, scale), ControlPointRadius, control_color);
            dl->AddCircleFilled(GetControlPoint(i + 2, offset, scale), ControlPointRadius, control_color);
        }
    }
    if (ShowAnchorPoints) {
        const auto anchor_fill_color = ImGui::ColorConvertFloat4ToU32({AnchorFillColor[0], AnchorFillColor[1], AnchorFillColor[2], AnchorFillColor[3]});
        const auto anchor_stroke_color = ImGui::ColorConvertFloat4ToU32({AnchorStrokeColor[0], AnchorStrokeColor[1], AnchorStrokeColor[2], AnchorStrokeColor[3]});
        for (size_t i = 0; i < num_ctrl - 1; i += 3) {
            dl->AddCircleFilled(GetControlPoint(i + 3, offset, scale), AnchorPointRadius, anchor_fill_color);
            dl->AddCircle(GetControlPoint(i + 3, offset, scale), AnchorPointRadius, anchor_stroke_color, 0, AnchorStrokeThickness);
        }
    }
}

void MeshProfile::RenderConfig() {
    ImGui::Text("SVG file: %s", SvgFilePath.filename().string().c_str());
    ImGui::Indent();
    ImGui::Text("(File -> Load mesh)");
    ImGui::Unindent();

    ImGui::NewLine();
    ImGui::Checkbox("Path", &ShowPath);
    if (ShowPath) {
        ImGui::SliderFloat("Path line thickness", &PathLineThickness, 0.5f, 5.f);
        ImGui::ColorEdit3("Path line color", &PathLineColor[0]);
    }

    if (AnchorPointRadius < ControlPointRadius) AnchorPointRadius = ControlPointRadius;
    ImGui::Checkbox("Anchor points", &ShowAnchorPoints);
    if (ShowAnchorPoints) {
        ImGui::SliderFloat("Anchor point radius", &AnchorPointRadius, std::max(0.5f, ControlPointRadius), 10.f);
        ImGui::SliderFloat("Anchor stroke thickness", &AnchorStrokeThickness, 5.f, 5.f);
        ImGui::ColorEdit3("Anchor fill color", &AnchorFillColor[0]);
        ImGui::ColorEdit3("Anchor stroke color", &AnchorStrokeColor[0]);
    }

    ImGui::Checkbox("Control points", &ShowControlPoints);
    if (ShowControlPoints) {
        ImGui::SliderFloat("Control point radius", &ControlPointRadius, 0.5f, 5.f);
        ImGui::SliderFloat("Control line thickness", &ControlLineThickness, 0.5f, 5.f);
        ImGui::ColorEdit3("Control color", &ControlColor[0]);
    }
}
