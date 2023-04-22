#include "MeshProfile.h"

#define NANOSVG_IMPLEMENTATION // Expands implementation
#include "mesh2faust.h"
#include "nanosvg.h"

#include <fmt/format.h>
#include <fstream>

#include "Eigen/SparseCore"
#include "Material.h"

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

static Eigen::SparseMatrix<double> ReadSparseMatrix(const fs::path &file_path) {
    std::ifstream input_file(file_path);
    if (!input_file.is_open()) throw std::runtime_error(string("Error opening file: ") + file_path.string());

    vector<Eigen::Triplet<double>> K_triplets;
    string line;
    while (std::getline(input_file, line)) {
        std::istringstream line_stream(line);
        unsigned int i, j;
        double entry;
        string comma;
        line_stream >> i >> comma >> j >> comma >> entry;

        // Decrement indices by 1 since Eigen uses 0-based indexing.
        K_triplets.emplace_back(i - 1, j - 1, entry);
    }

    input_file.close();

    // Find matrix dimensions.
    int num_rows = 0, num_cols = 0;
    for (const auto &triplet : K_triplets) {
        num_rows = std::max(num_rows, triplet.row() + 1);
        num_cols = std::max(num_cols, triplet.col() + 1);
    }

    Eigen::SparseMatrix<double> matrix(num_rows, num_cols);
    matrix.setFromTriplets(K_triplets.begin(), K_triplets.end());

    return matrix;
}

static const fs::path TesselationDir = fs::path("./") / "profile_tesselation";

string MeshProfile::GenerateDspAxisymmetric() const {
    // Write the profile to an obj file.
    const auto fem_dir = fs::path("./") / ".." / ".." / "fem";
    fs::create_directory(TesselationDir); // Create the tesselation dir if it doesn't exist.
    const auto obj_path = TesselationDir / SvgFilePath.filename().replace_extension(".obj");
    SaveTesselation(obj_path);

    // Execute the `fem` program to generate the mass/stiffness matrices.
    fs::path obj_path_no_extension = obj_path;
    obj_path_no_extension = obj_path_no_extension.replace_extension("");
    const string fem_cmd = fmt::format("{} {} {} {}", (fem_dir / "fem").string(), obj_path_no_extension.string(), Material.YoungModulus, Material.PoissonRatio);
    int result = std::system(fem_cmd.c_str());
    if (result != 0) throw std::runtime_error("Error executing fem command.");

    const auto M = ReadSparseMatrix(obj_path_no_extension.string() + "_M.out");
    const auto K = ReadSparseMatrix(obj_path_no_extension.string() + "_K.out");

    static const int num_vertices = 4;
    static const int vertex_dim = 2;
    return m2f::mesh2faust(
        M,
        K,
        num_vertices,
        vertex_dim,
        "modalModel", // generated object name
        true, // freq control activated
        20, // lowest mode freq
        10000, // highest mode freq
        20, // number of synthesized modes (default is 20)
        20, // number of modes to be computed for the finite element analysis (default is 100)
        {}, // specific excitation positions
        4, // number of excitation positions (default is max: -1)
        false,
        true
    );
}

static constexpr float Epsilon = 1e-6f;

bool MeshProfile::IsClosed() const {
    return ClosePath && (OffsetX > 0 || (std::abs(Vertices.front().x) >= Epsilon && std::abs(Vertices.back().x) >= Epsilon));
}

using namespace ImGui;

// Index of the currently selected control point, along with its position and the positions of its two corresponding
// control points at the time of drag initiation (used if the selected control point is also an anchor point).
static int ActiveAnchorPoint = -1, SelectedControlPoint = -1, HoveredControlPoint = -1;
static ImVec2 SelectedDragInitPositions[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}; // (The fourth element only applies to the first anchor point.)

void MeshProfile::DrawControlPoint(size_t i, const ImVec2 &offset, float scale) const {
    if (i < 0 || i >= ControlPoints.size()) return;

    auto *dl = GetWindowDrawList();
    // Draw the two control point lines.
    dl->PathLineTo(GetControlPoint(i, offset, scale));
    dl->PathLineTo(GetControlPoint(i + 1, offset, scale));
    dl->PathStroke(ControlColorU32, 0, ControlLineThickness);

    dl->PathLineTo(GetControlPoint(i + 2, offset, scale));
    dl->PathLineTo(GetControlPoint(i + 3, offset, scale));
    dl->PathStroke(ControlColorU32, 0, ControlLineThickness);

    // Draw the control points.
    for (size_t j = i + 1; j <= i + 2; j++) {
        const bool is_active = ActiveAnchorPoint == int(j) || HoveredControlPoint == int(j);
        const bool is_selected = SelectedControlPoint == int(j);
        const float radius = is_active || is_selected ? ControlPointRadius + ControlLineThickness : ControlPointRadius;
        const auto &cp = GetControlPoint(j, offset, scale);
        dl->AddCircleFilled(cp, radius, ControlColorU32);
        if (is_selected) {
            dl->AddCircle(cp, radius + ControlLineThickness, GetColorU32(ImGuiCol_ButtonActive), 0, ControlLineThickness);
        }
    }
}

static int GetAnchorPoint(int control_point) {
    if (control_point % 3 == 0) return control_point;
    if ((control_point - 1) % 3 == 0) return control_point - 1;
    if ((control_point + 1) % 3 == 0) return control_point + 1;
}

// Render the current 2D profile as a closed line shape (using ImGui).
bool MeshProfile::Render() {
    const size_t num_ctrl = NumControlPoints();
    if (num_ctrl < 4) return false;

    const float spacing = 2 + std::max(PathLineThickness, std::max(AnchorPointRadius, ControlPointRadius));
    const auto offset = GetCursorScreenPos() + ImVec2{spacing, spacing};
    // The profile is normalized to 1 based on its largest dimension.
    const auto &draw_size = GetContentRegionAvail() - ImVec2{2.f, 2.f} * spacing;
    const float scale = draw_size.y;
    if (scale <= 0) return false;

    // Interaction pass.
    bool modified = false;
    HoveredControlPoint = -1;
    if (ShowAnchorPoints || ShowControlPoints) {
        if (ActiveAnchorPoint > -1 && ActiveAnchorPoint < int(num_ctrl)) {
            const auto &drag_delta = GetMouseDragDelta() / scale;
            if (drag_delta.x != 0 || drag_delta.y != 0) {
                auto new_anchor_pos = SelectedDragInitPositions[0] + drag_delta;
                // Prevent from dragging offscreen.
                new_anchor_pos.x = std::clamp(new_anchor_pos.x, 0.f, draw_size.x / scale);
                new_anchor_pos.y = std::clamp(new_anchor_pos.y, 0.f, draw_size.y / scale);
                const auto &pos_delta = new_anchor_pos - SelectedDragInitPositions[0];
                ControlPoints[ActiveAnchorPoint] = new_anchor_pos;
                if (ActiveAnchorPoint % 3 == 0) {
                    // Anchors drag their corresponding control points.
                    ControlPoints[ActiveAnchorPoint == 0 ? num_ctrl - 1 : ActiveAnchorPoint - 1] = SelectedDragInitPositions[1] + pos_delta;
                    ControlPoints[ActiveAnchorPoint + 1] = SelectedDragInitPositions[2] + pos_delta;
                }

                if (ActiveAnchorPoint == 0) ControlPoints[num_ctrl - 2] = SelectedDragInitPositions[3] + pos_delta;

                modified = true;
            }
        }

        if (IsMouseReleased(0)) ActiveAnchorPoint = -1;

        const auto &io = GetIO();
        const bool mouse_clicked = IsMouseClicked(0);
        const ImVec2 radius_area{AnchorPointRadius, AnchorPointRadius};
        for (size_t i = 0; i < num_ctrl - 1; i++) {
            // todo not correct for non-anchor cps
            const bool is_visible = ShowControlPoints || (ShowAnchorPoints && i % 3 == 0) || (SelectedControlPoint != -1 && std::abs(int(i) - GetAnchorPoint(SelectedControlPoint)) < 5);
            if (!is_visible) continue;

            const auto &cp = GetControlPoint(i, offset, scale);
            const ImRect cp_rect = {cp - radius_area, cp + radius_area};
            if (cp_rect.Contains(io.MousePos)) {
                if (mouse_clicked) {
                    ActiveAnchorPoint = SelectedControlPoint = i;

                    SelectedDragInitPositions[0] = ControlPoints[i];
                    SelectedDragInitPositions[1] = ControlPoints[i == 0 ? num_ctrl - 1 : i - 1];
                    SelectedDragInitPositions[2] = ControlPoints[i + 1];
                    if (i == 0) SelectedDragInitPositions[3] = ControlPoints[num_ctrl - 2]; // Control points of first anchor wrap around.
                } else {
                    HoveredControlPoint = i;
                }
                if (HoveredControlPoint % 3 == 0 || ActiveAnchorPoint % 3 == 0) break; // Handle clicking/hovering overlapping points in favor of anchor points.
            }
        }
        // Clicking outside of an anchor point deselects it.
        if (mouse_clicked && ActiveAnchorPoint == -1) SelectedControlPoint = -1;
    }

    // Draw pass.
    auto *dl = GetWindowDrawList();

    const float curr_tol = dl->_Data->CurveTessellationTol; // Save current tolerance to restore later.
    dl->_Data->CurveTessellationTol = CurveTolerance;
    if (ShowPath) {
        // Bezier path has already been calculated, so just push it to the draw list's path.
        for (size_t i = 0; i < Vertices.size(); i++) dl->_Path.push_back(GetVertex(i, offset, scale));
        if (ClosePath) dl->PathLineTo(GetVertex(0, offset, scale));
        dl->PathStroke(PathLineColorU32, 0, PathLineThickness);
    }
    dl->_Data->CurveTessellationTol = curr_tol; // Restore previous tolerance.

    if (ShowControlPoints) {
        for (size_t i = 0; i < num_ctrl - 1; i += 3) {
            DrawControlPoint(i, offset, scale);
        }
    } else if (SelectedControlPoint != -1) {
        const int ap = GetAnchorPoint(SelectedControlPoint);
        // If the control points are hidden, but an anchor point is selected, draw the control points for that anchor.
        DrawControlPoint(ap - 3, offset, scale);
        DrawControlPoint(ap, offset, scale);
    }
    if (ShowAnchorPoints) {
        for (size_t i = 0; i < num_ctrl - 1; i += 3) {
            const auto &cp = GetControlPoint(i, offset, scale);
            const bool is_active = ActiveAnchorPoint == int(i) || HoveredControlPoint == int(i);
            const bool is_selected = SelectedControlPoint == int(i);
            dl->AddCircleFilled(cp, AnchorPointRadius, is_active ? GetColorU32(ImGuiCol_ButtonHovered) : AnchorFillColorU32);
            dl->AddCircle(cp, AnchorPointRadius, AnchorStrokeColorU32, 0, AnchorStrokeThickness);
            if (is_selected) {
                dl->AddCircle(cp, AnchorPointRadius + AnchorStrokeThickness, GetColorU32(ImGuiCol_ButtonActive), 0, AnchorStrokeThickness);
            }
        }
    }

    if (ShowTesselation) {
        for (size_t i = 0; i < TesselationIndices.size() - 1; i += 3) {
            for (int j = 0; j < 3; j++) {
                dl->_Path.push_back(GetVertex(TesselationIndices[i + j], offset, scale));
            }
            dl->PathStroke(TesselationStrokeColorU32, 0, 1);
        }
    }

    if (modified) CreateVertices();
    return modified;
}

bool MeshProfile::RenderConfig() {
    Text("SVG file: %s", SvgFilePath.c_str());
    if (SelectedControlPoint != -1) Text("Selected anchor point: %d", SelectedControlPoint);
    // If either of these parameters change, we need to regenerate the mesh.
    SeparatorText("Resolution");
    bool modified = SliderInt("Radial seg.", &NumRadialSlices, 3, 200, nullptr, ImGuiSliderFlags_Logarithmic);
    modified |= SliderFloat("Curve tol.", &CurveTolerance, 0.00001f, 0.5f, "%.5f", ImGuiSliderFlags_Logarithmic);
    modified |= SliderFloat("X-Offset", &OffsetX, 0, 1.f);
    // modified |= Checkbox("Close path", &ClosePath); // Leaving holes doesn't work well with tetgen.

    NewLine();
    Checkbox("Path", &ShowPath);
    SameLine();
    Checkbox("Tesselation", &ShowTesselation);
    if (ShowPath) {
        SeparatorText("Path");
        SliderFloat("Stroke weight##Path", &PathLineThickness, 0.5f, 5.f);
        if (ColorEdit4("Color##Path", (float *)&PathLineColor)) PathLineColorU32 = ColorConvertFloat4ToU32(PathLineColor);
    }
    if (ShowTesselation) {
        SeparatorText("Tesselation");
        if (ColorEdit4("Color##Tesselation", (float *)&TesselationStrokeColor)) TesselationStrokeColorU32 = ColorConvertFloat4ToU32(TesselationStrokeColor);
    }

    if (AnchorPointRadius < ControlPointRadius) AnchorPointRadius = ControlPointRadius;

    Checkbox("Anchor points", &ShowAnchorPoints);
    if (ShowAnchorPoints) {
        SeparatorText("Anchor points");
        SliderFloat("Radius##AnchorPoint", &AnchorPointRadius, std::max(0.5f, ControlPointRadius), 10.f);
        SliderFloat("Stroke weight##AnchorPoint", &AnchorStrokeThickness, 5.f, 5.f);
        if (ColorEdit4("Fill##AnchorPoint", (float *)&AnchorFillColor)) AnchorFillColorU32 = ColorConvertFloat4ToU32(AnchorFillColor);
        if (ColorEdit4("Stroke##AnchorPoint", (float *)&AnchorStrokeColor)) AnchorStrokeColorU32 = ColorConvertFloat4ToU32(AnchorStrokeColor);
    }

    Checkbox("Control points", &ShowControlPoints);
    if (ShowControlPoints) {
        SeparatorText("Control points");
        SliderFloat("Radius##ControlPoint", &ControlPointRadius, 0.5f, 5.f);
        SliderFloat("Stroke weight##ControlPoint", &ControlLineThickness, 0.5f, 5.f);
        if (ColorEdit4("Color##ControlPoint", (float *)&ControlColor)) ControlColorU32 = ColorConvertFloat4ToU32(ControlColor);
    }

    if (modified) CreateVertices();
    return modified;
}

// Private

void MeshProfile::CreateVertices() {
    const size_t num_ctrl = NumControlPoints();
    if (num_ctrl < 4) return;

    static ImDrawListSharedData sharedData;
    sharedData.CurveTessellationTol = CurveTolerance;

    static ImDrawList dl(&sharedData);
    if (!ClosePath) {
        // If an x-offset is applied, or if the user has moved the first or last anchor point away from `x = 0`,
        // we add horizontal line segment(s) to the path to ensure X coordinate of first and last vertex are zero.
        if (std::abs(ControlPoints[0].x) > Epsilon) dl.PathLineTo({0, ControlPoints[0].y});
        if (OffsetX > 0) dl.PathLineTo(ControlPoints[0]);
    }

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
    if (!ClosePath) {
        if (OffsetX > 0) dl.PathLineTo(ControlPoints[last_anchor_i]);
        if (std::abs(ControlPoints[last_anchor_i].x) > Epsilon) dl.PathLineTo({0, ControlPoints[last_anchor_i].y});
    }

    Vertices.resize(dl._Path.Size);
    for (int i = 0; i < dl._Path.Size; i++) Vertices[i] = dl._Path[i];

    dl.PathClear();

    Tesselate();
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

#include <fstream>

void MeshProfile::SaveTesselation(fs::path file_path) const {
    std::ofstream out(file_path.c_str());
    if (!out.is_open()) throw std::runtime_error(std::string("Error opening file: ") + file_path.string());

    out << "# Vertices: " << Vertices.size() << "\n";
    out << "# Faces: " << TesselationIndices.size() / 3 << "\n";

    out << std::setprecision(10);
    for (const ImVec2 &v : Vertices) {
        out << "v " << v.x << " " << v.y << " " << 0 << "\n";
    }
    for (size_t i = 0; i < TesselationIndices.size() - 1; i += 3) {
        out << "f ";
        for (int j = 0; j < 3; j++) out << TesselationIndices[i + j] + 1 << " ";
        out << "\n";
    }
    out.close();
}

#include "earcut.hpp"

void MeshProfile::Tesselate() {
    vector<vector<std::array<float, 2>>> polygon{{}};
    for (const auto &v : Vertices) polygon[0].push_back({v.x, v.y});
    TesselationIndices = mapbox::earcut<uint>(polygon);
}
