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
    void Render() const; // Render as a closed line shape (using ImGui).

private:
    vector<ImVec2> control_points;
};
