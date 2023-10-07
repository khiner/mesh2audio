#pragma once

#include "Material.h"
#include "MeshProfile.h"
#include "Scene.h"
#include "Worker.h"

#include "Geometry/Arrow.h"
#include "Geometry/Primitive/Sphere.h"

struct RealImpact;
struct tetgenio;

struct Mesh {
    Scene &Scene;

    using Type = int;

    enum Type_ {
        MeshType_Triangular,
        MeshType_Tetrahedral,
        MeshType_ConvexHull,
    };

    // Load a 3D mesh from a .obj file, or a 2D profile from a .svg file.
    Mesh(::Scene &scene, fs::path file_path);
    ~Mesh();

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
        switch (ViewMeshType) {
            case MeshType_Triangular: return TriangularMesh;
            case MeshType_Tetrahedral: return TetMesh;
            case MeshType_ConvexHull: return ConvexHullMesh;
        }
    }
    inline Geometry &GetActiveGeometry() {
        switch (ViewMeshType) {
            case MeshType_Triangular: return TriangularMesh;
            case MeshType_Tetrahedral: return TetMesh;
            case MeshType_ConvexHull: return ConvexHullMesh;
        }
    }

    bool HasTetMesh() const { return !TetMesh.Empty(); }

    std::string GenerateDsp() const;
    std::string GenerateDspAxisymmetric() const { return Profile != nullptr ? Profile->GenerateDspAxisymmetric() : ""; }
    int Num3DExcitationVertices() const { return NumExcitableVertices; }
    int Num2DExcitationVertices() const { return Profile != nullptr ? Profile->NumExcitationVertices() : 0; }

    void ApplyTransform();
    glm::mat4 GetTransform() const;

    int NumExcitableVertices = 10;
    bool ShowExcitableVertices = true; // Only shown when viewing tet mesh.
    bool QualityTetMesh = true;
    bool AutomaticTetGeneration = true;

    fs::path FilePath; // Most recently loaded file path.

private:
    Type ViewMeshType = MeshType_Triangular;

    void SetViewMeshType(Type);

    void UpdateHoveredVertex();
    void UpdateExcitableVertices();
    void UpdateExcitableVertexColors();

    void GenerateTetMesh(); // Populates `TetGenResult`.
    void UpdateTetMesh(); // Update the `TetMesh` geometry from `TetGenResult`.

    // Generate an axisymmetric 3D mesh by rotating the current 2D profile about the y-axis.
    // _This will have no effect if `Load(path)` was not called first to load a 2D profile._
    void ExtrudeProfile();
    void LoadRealImpact(); // Load [RealImpact](https://github.com/khiner/RealImpact) in the same directory as the loaded .obj file.

    std::unique_ptr<tetgenio> TetGenResult;
    std::unique_ptr<MeshProfile> Profile;
    std::unique_ptr<::RealImpact> RealImpact;

    Geometry TriangularMesh, TetMesh, ConvexHullMesh; // `ConvexHullMesh` is the convex hull of `TriangularMesh`.

    // Bounds of original loaded mesh, before any transformations.
    // Used to determine initial camera distance and scale of auto-generated geometries.
    std::pair<glm::vec3, glm::vec3> InitialBounds; // [{min_x, min_y, min_z}, {max_x, max_y, max_z}]

    Worker TetGenerator{"Generate tet mesh", "Generating tetrahedral mesh...", [&] { GenerateTetMesh(); }};
    Worker RealImpactLoader{"Load RealImpact", "Loading RealImpact data...", [&] { LoadRealImpact(); }};

    int HoveredVertexIndex = -1, CameraTargetVertexIndex = -1;
    Arrow HoveredVertexArrow{0.5, 0.1, 0.2, 0.3};

    std::vector<int> ExcitableVertexIndices; // Indexes into `TetMesh` vertices.
    Arrow ExcitableVertexArrows{0.25, 0.05, 0.1, 0.15}; // Instanced arrows for each excitable vertex, with less emphasis than `HoveredVertexArrow`.
    Sphere RealImpactListenerPoints{0.01}; // Instanced spheres for each listener point.

    glm::vec3 Translation{0.f}, Scale{1.f}, RotationAngles{0.0f};
};
