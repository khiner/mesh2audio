#include "MeshProfile.h"

#define NANOSVG_IMPLEMENTATION // Expands implementation
#include "nanosvg.h"

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

    SvgFilePath = svg_file_path;
    OriginalBounds = CalcBounds();
    const float max_dim = std::max(OriginalBounds.GetWidth(), OriginalBounds.GetHeight());
    for (auto &v : ControlPoints) {
        v.x -= OriginalBounds.Min.x; // Offset so that leftmost x is 0.
        v /= max_dim; // Normalize so that the largest dimension is 1.0.
    }

    CreateVertices();
}

// Index of the currently selected anchor point, along with the positions of the anchor and its two corresponding
// control points at the time of drag initiation.
static int SelectedAnchorPoint = -1;
static ImVec2 SelectedDragInitPositions[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}; // (Only the first point uses the fourth element.)

// Render the current 2D profile as a closed line shape (using ImGui).
bool MeshProfile::Render() {
    const size_t num_ctrl = NumControlPoints();
    if (num_ctrl < 4) return false;

    const float spacing = 2 + std::max(PathLineThickness, std::max(AnchorPointRadius, ControlPointRadius));
    const auto offset = ImGui::GetCursorScreenPos() + ImVec2{spacing, spacing};
    // The profile is normalized to 1 based on its largest dimension.
    const auto &draw_size = ImGui::GetContentRegionAvail() - ImVec2{2.f, 2.f} * spacing;
    const float scale = draw_size.y;
    if (scale <= 0) return false;

    auto *dl = ImGui::GetWindowDrawList();
    const float curr_tol = dl->_Data->CurveTessellationTol; // Save current tolerance to restore later.
    dl->_Data->CurveTessellationTol = CurveTolerance;
    if (ShowPath) {
        // Bezier path has already been calculated, so just push it to the draw list's path.
        const auto path_color = ImGui::ColorConvertFloat4ToU32({PathLineColor[0], PathLineColor[1], PathLineColor[2], PathLineColor[3]});
        dl->PathLineTo(GetVertex(0, offset, scale));
        for (size_t i = 1; i < Vertices.size(); i++) {
            dl->_Path.push_back(GetVertex(i, offset, scale));
        }
        dl->PathStroke(path_color, 0, PathLineThickness);
    }
    if (ShowControlPoints) {
        const auto control_color = ImGui::ColorConvertFloat4ToU32({ControlColor[0], ControlColor[1], ControlColor[2], ControlColor[3]});
        // Draw the two control point lines.
        for (size_t i = 0; i < num_ctrl - 1; i += 3) {
            dl->PathLineTo(GetControlPoint(i, offset, scale));
            dl->PathLineTo(GetControlPoint(i + 1, offset, scale));
            dl->PathStroke(control_color, 0, ControlLineThickness);

            dl->PathLineTo(GetControlPoint(i + 2, offset, scale));
            dl->PathLineTo(GetControlPoint(i + 3, offset, scale));
            dl->PathStroke(control_color, 0, ControlLineThickness);
        }
        // Draw the control points.
        dl->AddCircleFilled(GetControlPoint(0, offset, scale), ControlPointRadius, control_color);
        for (size_t i = 0; i < num_ctrl - 1; i += 3) {
            dl->AddCircleFilled(GetControlPoint(i + 1, offset, scale), ControlPointRadius, control_color);
            dl->AddCircleFilled(GetControlPoint(i + 2, offset, scale), ControlPointRadius, control_color);
        }
    }

    bool modified = false;
    if (ShowAnchorPoints) {
        const auto &io = ImGui::GetIO();
        if (SelectedAnchorPoint > -1 && SelectedAnchorPoint < int(num_ctrl)) {
            const auto &drag_delta = ImGui::GetMouseDragDelta() / scale;
            if (drag_delta.x != 0 || drag_delta.y != 0) {
                auto new_anchor_pos = SelectedDragInitPositions[0] + drag_delta;
                // Prevent from dragging offscreen.
                new_anchor_pos.x = std::clamp(new_anchor_pos.x, 0.f, draw_size.x / scale);
                new_anchor_pos.y = std::clamp(new_anchor_pos.y, 0.f, draw_size.y / scale);
                const auto &pos_delta = new_anchor_pos - SelectedDragInitPositions[0];
                ControlPoints[SelectedAnchorPoint] = new_anchor_pos;
                ControlPoints[SelectedAnchorPoint == 0 ? num_ctrl - 1 : SelectedAnchorPoint - 1] = SelectedDragInitPositions[1] + pos_delta;
                ControlPoints[SelectedAnchorPoint + 1] = SelectedDragInitPositions[2] + pos_delta;
                if (SelectedAnchorPoint == 0) ControlPoints[num_ctrl - 2] = SelectedDragInitPositions[3] + pos_delta;

                modified = true;
            }
        }

        if (ImGui::IsMouseReleased(0)) SelectedAnchorPoint = -1;

        const bool mouse_clicked = ImGui::IsMouseClicked(0);
        const auto anchor_fill_color = ImGui::ColorConvertFloat4ToU32({AnchorFillColor[0], AnchorFillColor[1], AnchorFillColor[2], AnchorFillColor[3]});
        const auto anchor_stroke_color = ImGui::ColorConvertFloat4ToU32({AnchorStrokeColor[0], AnchorStrokeColor[1], AnchorStrokeColor[2], AnchorStrokeColor[3]});
        const ImVec2 radius_area{AnchorPointRadius, AnchorPointRadius};
        for (size_t i = 0; i < num_ctrl - 1; i += 3) {
            const auto &cp = GetControlPoint(i, offset, scale);
            const ImRect cp_rect = {cp - radius_area, cp + radius_area};
            const bool is_active = SelectedAnchorPoint == int(i) || ImGui::IsMouseHoveringRect(cp_rect.Min, cp_rect.Max);
            if (SelectedAnchorPoint == -1 && mouse_clicked && cp_rect.Contains(io.MouseClickedPos[0])) {
                SelectedAnchorPoint = i;
                SelectedDragInitPositions[0] = ControlPoints[i];
                SelectedDragInitPositions[1] = ControlPoints[i == 0 ? num_ctrl - 1 : i - 1];
                SelectedDragInitPositions[2] = ControlPoints[i + 1];
                if (i == 0) SelectedDragInitPositions[3] = ControlPoints[num_ctrl - 2]; // Control points of first anchor wrap around.
            }
            dl->AddCircleFilled(cp, AnchorPointRadius, is_active ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : anchor_fill_color);
            dl->AddCircle(cp, AnchorPointRadius, anchor_stroke_color, 0, AnchorStrokeThickness);
        }
    }

    dl->_Data->CurveTessellationTol = curr_tol;

    if (modified) CreateVertices();
    return modified;
}

bool MeshProfile::RenderConfig() {
    ImGui::Text("SVG file: %s", SvgFilePath.c_str());

    // If either of these parameters change, we need to regenerate the mesh.
    ImGui::SeparatorText("Resolution");
    bool modified = ImGui::SliderInt("Radial seg.", &NumRadialSlices, 3, 200, nullptr, ImGuiSliderFlags_Logarithmic);
    modified |= ImGui::SliderFloat("Curve tol.", &CurveTolerance, 0.00001f, 0.5f, "%.5f", ImGuiSliderFlags_Logarithmic);
    modified |= ImGui::SliderFloat("X-Offset", &OffsetX, 0, 1.f);

    ImGui::NewLine();
    ImGui::Checkbox("Path", &ShowPath);
    if (ShowPath) {
        ImGui::SeparatorText("Path");
        ImGui::SliderFloat("Stroke weight", &PathLineThickness, 0.5f, 5.f);
        ImGui::ColorEdit3("Color", &PathLineColor[0]);
    }

    if (AnchorPointRadius < ControlPointRadius) AnchorPointRadius = ControlPointRadius;

    ImGui::Checkbox("Anchor points", &ShowAnchorPoints);
    if (ShowAnchorPoints) {
        ImGui::SeparatorText("Anchor points");
        ImGui::SliderFloat("Radius", &AnchorPointRadius, std::max(0.5f, ControlPointRadius), 10.f);
        ImGui::SliderFloat("Stroke weight", &AnchorStrokeThickness, 5.f, 5.f);
        ImGui::ColorEdit3("Fill", &AnchorFillColor[0]);
        ImGui::ColorEdit3("Stroke", &AnchorStrokeColor[0]);
    }

    ImGui::Checkbox("Control points", &ShowControlPoints);
    if (ShowControlPoints) {
        ImGui::SeparatorText("Control points");
        ImGui::SliderFloat("Radius", &ControlPointRadius, 0.5f, 5.f);
        ImGui::SliderFloat("Stroke weight", &ControlLineThickness, 0.5f, 5.f);
        ImGui::ColorEdit3("Color", &ControlColor[0]);
    }

    if (modified) CreateVertices();
    return modified;
}

// Private

static constexpr float Epsilon = 1e-6f;

void MeshProfile::CreateVertices() {
    const size_t num_ctrl = NumControlPoints();
    if (num_ctrl < 4) return;

    static ImDrawListSharedData sharedData;
    sharedData.CurveTessellationTol = CurveTolerance;

    static ImDrawList dl(&sharedData);
    // If an x-offset is applied, or if the user has moved the first or last anchor point away from `x = 0`,
    // we add horizontal line segment(s) to the path to ensure X coordinate of first and last vertex are zero.
    if (std::abs(ControlPoints[0].x) > Epsilon) dl.PathLineTo({0, ControlPoints[0].y});
    if (OffsetX > 0) dl.PathLineTo(ControlPoints[0]);

    const ImVec2 Offset = {OffsetX, 0};
    dl.PathLineTo(ControlPoints[0] + Offset);
    // Draw Bezier curves between each anchor point, based on its surrounding control points.
    // We don't connect the last control point, since it's a dupe of the first.
    // (TODO should check to be sure)
    const size_t last_anchor_i = num_ctrl - 4;
    for (size_t i = 0; i < last_anchor_i; i += 3) {
        dl.PathBezierCubicCurveTo(
            ControlPoints[i + 1] + Offset,
            ControlPoints[i + 2] + Offset,
            ControlPoints[i + 3] + Offset
        );
    }
    if (OffsetX > 0) dl.PathLineTo(ControlPoints[last_anchor_i]);
    if (std::abs(ControlPoints[last_anchor_i].x) > Epsilon) dl.PathLineTo({0, ControlPoints[last_anchor_i].y});

    Vertices.resize(dl._Path.Size);
    for (int i = 0; i < dl._Path.Size; i++) Vertices[i] = dl._Path[i];

    dl.PathClear();
}

ImRect MeshProfile::CalcBounds() {
    float x_min = INFINITY, x_max = -INFINITY, y_min = INFINITY, y_max = -INFINITY;
    for (auto &v : ControlPoints) {
        if (v.x < x_min) x_min = v.x;
        if (v.x > x_max) x_max = v.x;
        if (v.y < y_min) y_min = v.y;
        if (v.y > y_max) y_max = v.y;
    }
    return {x_min, y_min, x_max - x_min, y_max - y_min};
}

ImVec2 MeshProfile::GetControlPoint(size_t i, const ImVec2 &offset, float scale) const {
    return (ControlPoints[i] + ImVec2{OffsetX, 0}) * scale + offset;
}
ImVec2 MeshProfile::GetVertex(size_t i, const ImVec2 &offset, float scale) const {
    return Vertices[i] * scale + offset;
}
