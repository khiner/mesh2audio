#pragma once

#include "Material.h"
#include "MeshProfile.h"
#include "Scene.h"
#include "Worker.h"

#include "Geometry/Arrow.h"
#include "Geometry/Primitive/Sphere.h"

struct RealImpact;
struct tetgenio;

struct InteractiveMesh {
    Scene &Scene;

    enum ViewMode {
        ViewMode_Triangular,
        ViewMode_Tetrahedral,
        ViewMode_ConvexHull,
    };

    // Load a 3D mesh from a .obj file, or a 2D profile from a .svg file.
    InteractiveMesh(::Scene &, fs::path file_path);
    ~InteractiveMesh();

    void Update();

    void Render();
    void RenderConfig();
    void RenderProfile();
    void RenderProfileConfig();
    void Save(fs::path file_path) const; // Export the active mesh to a .obj file.

    bool HasProfile() const { return Profile != nullptr; }
    void SaveProfile(fs::path file_path) const {
        if (Profile != nullptr) Profile->SaveTesselation(file_path);
    }

    inline const Geometry &GetActiveGeometry() const {
        switch (ActiveViewMode) {
            case ViewMode_Triangular: return Triangles;
            case ViewMode_Tetrahedral: return Tets;
            case ViewMode_ConvexHull: return ConvexHull;
        }
    }
    inline Geometry &GetActiveGeometry() {
        switch (ActiveViewMode) {
            case ViewMode_Triangular: return Triangles;
            case ViewMode_Tetrahedral: return Tets;
            case ViewMode_ConvexHull: return ConvexHull;
        }
    }

    bool HasTets() const { return !Tets.Empty(); }

    std::string GenerateDsp() const;
    std::string GenerateDspAxisymmetric() const { return Profile != nullptr ? Profile->GenerateDspAxisymmetric() : ""; }
    int Num3DExcitationVertices() const { return NumExcitableVertices; }
    int Num2DExcitationVertices() const { return Profile != nullptr ? Profile->NumExcitationVertices() : 0; }

    void ApplyTransform();
    glm::mat4 GetTransform() const;

    int NumExcitableVertices = 10;
    bool ShowExcitableVertices = true; // Only shown when viewing tet mesh.
    bool QualityTets = true;
    bool AutomaticTetGeneration = true;

    fs::path FilePath; // Most recently loaded file path.

private:
    ViewMode ActiveViewMode = ViewMode_Triangular;

    void SetViewMode(ViewMode);

    void UpdateHoveredVertex();
    void UpdateExcitableVertices();
    void UpdateExcitableVertexColors();

    void GenerateTets(); // Populates `TetGenResult`.
    void UpdateTets(); // Update the `Tets` geometry from `TetGenResult`.

    // Generate an axisymmetric 3D mesh by rotating the current 2D profile about the y-axis.
    // _This will have no effect if `Load(path)` was not called first to load a 2D profile._
    void ExtrudeProfile();
    void LoadRealImpact(); // Load [RealImpact](https://github.com/khiner/RealImpact) in the same directory as the loaded .obj file.

    Geometry Triangles, Tets, ConvexHull; // `ConvexHull` is the convex hull of `Triangles`.

    // Bounds of original loaded mesh, before any transformations.
    // Used to determine initial camera distance and scale of auto-generated geometries.
    std::pair<glm::vec3, glm::vec3> InitialBounds; // [{min_x, min_y, min_z}, {max_x, max_y, max_z}]

    glm::vec3 Translation{0.f}, Scale{1.f}, RotationAngles{0.0f};

    std::unique_ptr<tetgenio> TetGenResult;
    std::unique_ptr<MeshProfile> Profile;
    std::unique_ptr<::RealImpact> RealImpact;

    Worker TetGenerator{"Generate tet mesh", "Generating tetrahedral mesh...", [&] { GenerateTets(); }};
    Worker RealImpactLoader{"Load RealImpact", "Loading RealImpact data...", [&] { LoadRealImpact(); }};

    int HoveredVertexIndex = -1, CameraTargetVertexIndex = -1;
    Arrow HoveredVertexArrow{0.5, 0.1, 0.2, 0.3};

    std::vector<int> ExcitableVertexIndices; // Indexes into `Tets` vertices.
    Arrow ExcitableVertexArrows{0.25, 0.05, 0.1, 0.15}; // Instanced arrows for each excitable vertex, with less emphasis than `HoveredVertexArrow`.
    Sphere RealImpactListenerPoints{0.01}; // Instanced spheres for each listener point.
};
