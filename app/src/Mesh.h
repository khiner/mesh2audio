#pragma once

#include "tetMesh.h" // Vega
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cinolib/geometry/vec_mat.h>

#include "MeshProfile.h"

#include "ImGuizmo.h"
using glm::vec3, glm::mat4;

struct ImVec2;

// Currently, this class also handles things like camera and lighting.
// If there were more than one mesh, we would move that stuff out of here.
struct Mesh {
    using Type = int;

    enum Type_ {
        MeshType_Triangular,
        MeshType_Tetrahedral,
    };

    enum RenderType_ {
        RenderType_Smooth,
        RenderType_Lines,
        RenderType_Points,
        RenderType_Mesh
    };
    using RenderType = int;

    // Defaults to aluminum.
    struct MaterialProperties {
        double YoungModulus{70E9};
        double PoissonRatio{0.35};
        double Density{2700};
    };

    // Load a 3D mesh from a .obj file, or a 2D profile from a .svg file.
    Mesh(fs::path file_path);
    ~Mesh();

    struct Data {
        bool Empty() const { return Vertices.empty(); }

        void Clear();
        void Save(fs::path file_path) const; // Export the mesh to a .obj file.

        void Flip(bool x, bool y, bool z);
        void Rotate(const vec3 &axis, float angle);
        void Scale(const vec3 &scale);
        void Center();

        void UpdateBounds(); // Updates the bounding box (`Min` and `Max`).
        void ExtrudeProfile(const MeshProfile &profile);

        vector<vec3> Vertices, Normals;
        vector<unsigned int> Indices;
        vec3 Min, Max; // The bounding box of the mesh.
    };

    void Render();
    void RenderConfig();
    void RenderProfile();
    void RenderProfileConfig();
    void Save(fs::path file_path) const; // Export the active mesh to a .obj file.

    const Data &GetActiveData() const;

    // Every time a tet mesh is generated, it is automatically saved to disk.
    void CreateTetraheralMesh();
    void LoadTetMesh(fs::path file_path);
    void LoadTetMesh(const vector<cinolib::vec3d> &vecs, const vector<vector<uint>> &polys);
    bool HasTetrahedralMesh() const { return !TetrahedralMesh.Empty(); }

    std::string GenerateDsp() const;

    void Flip(bool x, bool y, bool z); // Flip vertices across the given axes, about the center of the mesh.
    void Rotate(const vec3 &axis, float angle);
    void Scale(const vec3 &scale); // Scale the mesh by the given amounts.
    void Center(); // Center the mesh at the origin.

    static void SetCameraDistance(float distance);
    static void UpdateCameraProjection(const ImVec2 &size);

    inline static const int NumLights = 5;

    inline static RenderType RenderMode = RenderType_Smooth;
    inline static float Ambient[4] = {0.05, 0.05, 0.05, 1};
    inline static float Diffusion[4] = {0.2, 0.2, 0.2, 1};
    inline static float Specular[4] = {0.5, 0.5, 0.5, 1};
    inline static float LightPositions[NumLights * 4] = {0.0f};
    inline static float LightColors[NumLights * 4] = {0.0f};
    inline static float Shininess = 10;
    inline static bool CustomColor = false;
    inline static bool ShowCameraGizmo = true, ShowGrid = false, ShowGizmo = false, ShowBounds = false;
    inline static ImGuizmo::OPERATION GizmoOp{ImGuizmo::TRANSLATE};

    inline static mat4 ObjectMatrix{1.f}, CameraView, CameraProjection;

    inline static float CameraDistance = 4, fov = 27;
    inline static float Bounds[6] = {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    inline static Type ViewMeshType = MeshType_Triangular;

    inline static std::map<std::string, MaterialProperties> MaterialPresets = {
        {"Copper", {110e9f, 0.33f, 8600.0f}}, // 8900
        {"Aluminum", {70e9f, 0.35f, 2700.0f}},
        {"Steel", {200e9f, 0.3f, 8000.0f}},
        {"Glass", {70e9f, 0.2f, 2500.0f}},
        {"Wood", {10e9f, 0.3f, 500.0f}},
    };
    inline static MaterialProperties Material{MaterialPresets["Copper"]};

    fs::path FilePath; // Most recently loaded file path.

private:
    static void InitializeStatic(); // Initialize variables shared across all meshes.

    // Generate an axisymmetric 3D mesh by rotating the current 2D profile about the y-axis.
    // _This will have no effect if `Load(path)` was not called first to load a 2D profile._
    void ExtrudeProfile();

    // Non-empty if the mesh was generated from a 2D profile:
    std::unique_ptr<MeshProfile> Profile;
    struct Data TriangularMesh, TetrahedralMesh;
    Type ActiveViewMeshType = MeshType_Triangular;

    unsigned int VertexArray, VertexBuffer, NormalBuffer, IndexBuffer;

    void Bind(); // Bind active mesh.
    void Bind(const Data &data); // Bind mesh and set up vertex attributes.
};
