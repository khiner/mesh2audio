#pragma once

#include <filesystem>
#include <vector>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

using std::vector;

namespace fs = std::filesystem;

struct MeshProfile {
    explicit MeshProfile(fs::path svg_file_path); // Load a 2D profile from a .svg file.

    inline int NumControlPoints() const { return ControlPoints.size(); }
    int NumVertices() const { return Vertices.size(); }
    bool IsClosed() const; // Takes into account `ClosePath`, `OffsetX`, and vertex positions.

    // The vertices should be ordered clockwise, with the first vertex corresponding to the top/outside of the surface,
    // and last vertex corresponding the the bottom/inside of the surface.
    // These top/bottom vertices will be connected in the middle of the extruded 3D mesh,
    // creating a continuous connected solid "bridge" between all rotated slices.
    // E.g. for a bell profile, the top-center of the bell would be the first vertex, the bottom-center
    // would be the last vertex, and the outside lip of the bell would be somewhere in the middle.
    const vector<ImVec2> &GetVertices() const { return Vertices; }

    bool Render(); // Render as a closed line shape (using ImGui). Returns `true` if the profile was modified.
    bool RenderConfig(); // Render config section (using ImGui).

    // `CreateVertices` is called when these parameters change.
    inline static int NumRadialSlices{100};
    inline static float CurveTolerance{0.0001f};

    inline static bool ShowPath{true}, ShowAnchorPoints{true}, ShowControlPoints{false};
    inline static float PathLineThickness{2}, ControlLineThickness{1.5}, AnchorStrokeThickness{2};
    inline static ImVec4 PathLineColor = {1, 1, 1, 1}, AnchorFillColor = {0, 0, 0, 1}, AnchorStrokeColor = {1, 1, 1, 1}, ControlColor = {0, 1, 0, 1};
    inline static ImU32 PathLineColorU32 =  ImGui::ColorConvertFloat4ToU32(PathLineColor),
                        AnchorFillColorU32 = ImGui::ColorConvertFloat4ToU32(AnchorFillColor),
                        AnchorStrokeColorU32 = ImGui::ColorConvertFloat4ToU32(AnchorStrokeColor),
                        ControlColorU32 = ImGui::ColorConvertFloat4ToU32(ControlColor);
    inline static float AnchorPointRadius{6}, ControlPointRadius{3};

    // Offset applied to `Vertices`, used to extend the extruded mesh radially without stretching by creating a gap in the middle.
    // Does not affect `ControlPoints`.
    inline static float OffsetX;

    // If true, the last vertex will be connected to the first vertex.
    // If true, and if the x pos of the first or last vertex is not zero, the extruded mesh will have a hole in the middle.
    // Note: Leaving this param, but not making it configurable, since leaving a hole in the middle doesn't seem to work with tetgen.
    inline static bool ClosePath{false};

private:
    void CreateVertices();
    ImRect CalcBounds(); // Calculate current bounds based on control points. Note: original bounds cached in `OriginalBounds`.

    // Used internally for calculating window-relative draw positions.
    ImVec2 GetControlPoint(size_t i, const ImVec2 &offset, float scale) const;
    ImVec2 GetVertex(size_t i, const ImVec2 &offset, float scale) const;

    void DrawControlPoint(size_t i, const ImVec2 &offset, float scale) const;

    fs::path SvgFilePath; // Most recently loaded .svg file path.
    ImRect OriginalBounds; // Bounds as read directly from SVG, before normalizing.

    vector<ImVec2> ControlPoints;
    vector<ImVec2> Vertices; // Cached vertices, including Bezier curve segments.
};
