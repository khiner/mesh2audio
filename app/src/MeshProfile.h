#pragma once

#include <filesystem>
#include <vector>

using std::vector;

namespace fs = std::filesystem;

struct ImVec2;

struct MeshProfile {
    explicit MeshProfile(fs::path svg_file_path); // Load a 2D profile from a .svg file.

    int NumControlPoints() const;
    ImVec2 GetControlPoint(size_t i, const ImVec2 &offset, float scale) const;
    vector<ImVec2> CreateVertices(float tolerance) const;

    void Normalize(); // Normalize control points so that the largest dimension is 1.0:
    void Render(); // Render as a closed line shape (using ImGui).
    void RenderConfig(); // Render config section (using ImGui).

    bool ShowPath{true}, ShowAnchorPoints{true}, ShowControlPoints{false};
    float PathLineThickness{2}, ControlLineThickness{1.5}, AnchorStrokeThickness{2};
    float PathLineColor[4] = {1, 1, 1, 1}, AnchorFillColor[4] = {0, 0, 0, 1}, AnchorStrokeColor[4] = {1, 1, 1, 1}, ControlColor[4] = {0, 1, 0, 1};
    float AnchorPointRadius{6}, ControlPointRadius{3};

private:
    fs::path SvgFilePath; // Most recently loaded .svg file path.
    vector<ImVec2> ControlPoints;
};
