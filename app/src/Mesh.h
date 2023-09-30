#pragma once

#include "tetMesh.h" // Vega

#include "Material.h"
#include "MeshProfile.h"
#include "RealImpact.h"
#include "Scene.h"
#include "Worker.h"

#include "Geometry/Arrow.h"
#include "Geometry/Primitive/Sphere.h"

struct ImVec2;

struct Mesh {
    Scene &Scene;

    using Type = int;

    enum Type_ {
        MeshType_Triangular,
        MeshType_Tetrahedral,
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

    inline const Geometry &GetActiveGeometry() const { return ViewMeshType == MeshType_Triangular ? TriangularMesh : TetMesh; }
    inline Geometry &GetActiveGeometry() { return ViewMeshType == MeshType_Triangular ? TriangularMesh : TetMesh; }

    // Every time a tet mesh is generated, it is automatically saved to disk.
    void GenerateTetMesh();
    bool HasTetMesh() const { return !TetMesh.Empty(); }
    static std::string GetTetMeshName(fs::path file_path);

    std::string GenerateDsp() const;
    std::string GenerateDspAxisymmetric() const { return Profile != nullptr ? Profile->GenerateDspAxisymmetric() : ""; }
    int Num3DExcitationVertices() const { return NumExcitableVertices; }
    int Num2DExcitationVertices() const { return Profile != nullptr ? Profile->NumExcitationVertices() : 0; }

    void ApplyTransform();
    glm::mat4 GetTransform() const;

    inline static int NumExcitableVertices = 10;
    inline static bool ShowExcitableVertices = true; // Only shown when viewing tet mesh.
    inline static bool QualityTetMesh = true;

    fs::path TetMeshPath; // Path to the current loaded tet mesh.
    fs::path FilePath; // Most recently loaded file path.

private:
    Type ViewMeshType = MeshType_Triangular;

    void SetViewMeshType(Type type);

    void UpdateHoveredVertex();
    void UpdateExcitableVertices();
    void UpdateExcitableVertexColors();

    // Generate an axisymmetric 3D mesh by rotating the current 2D profile about the y-axis.
    // _This will have no effect if `Load(path)` was not called first to load a 2D profile._
    void ExtrudeProfile();

    void LoadRealImpact() {
        RealImpact = std::make_unique<::RealImpact>(FilePath.parent_path());
        Scene.AddGeometry(&RealImpactListenerPoints);
    }

    // Non-empty if the mesh was generated from a 2D profile:
    std::unique_ptr<MeshProfile> Profile;
    std::unique_ptr<::RealImpact> RealImpact;

    Geometry TriangularMesh, TetMesh;

    ImRect BoundsRect; // Bounds of original loaded mesh, before any transformations.

    Worker TetGenerator{"Generate tet mesh", "Generating tetrahedral mesh...", [&] { GenerateTetMesh(); }};
    Worker RealImpactLoader{"Load RealImpact", "Loading RealImpact data...", [&] { LoadRealImpact(); }};

    int HoveredVertexIndex = -1, CameraTargetVertexIndex = -1;
    Arrow HoveredVertexArrow{0.005, 0.001, 0.002, 0.003};

    vector<int> ExcitableVertexIndices; // Indexes into `TetMesh` vertices.
    Sphere ExcitableVertexPoints{0.0025}; // Instanced spheres for each excitable vertex.
    Sphere RealImpactListenerPoints{0.01}; // Instanced spheres for each listener point.

    glm::vec3 Translation{0.f}, Scale{1.f}, RotationAngles{0.0f};
};
