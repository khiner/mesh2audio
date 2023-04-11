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

    int NumControlPoints() const;
    const vector<ImVec2> &GetVertices() const { return Vertices; }

    bool Render(); // Render as a closed line shape (using ImGui). Returns `true` if the profile was modified.
    bool RenderConfig(); // Render config section (using ImGui).

    // `CreateVertices` is called when these parameters change.
    inline static int NumRadialSlices{100};
    inline static float CurveTolerance{0.0001f};

    inline static bool ShowPath{true}, ShowAnchorPoints{true}, ShowControlPoints{false};
    inline static float PathLineThickness{2}, ControlLineThickness{1.5}, AnchorStrokeThickness{2};
    inline static float PathLineColor[4] = {1, 1, 1, 1}, AnchorFillColor[4] = {0, 0, 0, 1}, AnchorStrokeColor[4] = {1, 1, 1, 1}, ControlColor[4] = {0, 1, 0, 1};
    inline static float AnchorPointRadius{6}, ControlPointRadius{3};

    // Offset applied to `Vertices`, used to extend the extruded mesh radially without stretching by creating a gap in the middle.
    // Does not affect `ControlPoints`.
    inline static ImVec2 Offset;

private:
    void CreateVertices();
    ImRect CalcBounds(); // Calculate current bounds based on control points. Note: original bounds cached in `OriginalBounds`.

    // Used internally for calculating window-relative draw positions.
    ImVec2 GetControlPoint(size_t i, const ImVec2 &offset, float scale) const;
    ImVec2 GetVertex(size_t i, const ImVec2 &offset, float scale) const;

    fs::path SvgFilePath; // Most recently loaded .svg file path.
    ImRect OriginalBounds; // Bounds as read directly from SVG, before normalizing.

    vector<ImVec2> ControlPoints;
    vector<ImVec2> Vertices; // Cached vertices, including Bezier curve segments.
};
