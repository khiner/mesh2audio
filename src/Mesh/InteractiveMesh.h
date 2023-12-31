#pragma once

#include "Geometry/Arrow.h"
#include "Geometry/Primitive/Sphere.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshProfile.h"
#include "Scene.h"
#include "Worker.h"

struct RealImpact;
struct tetgenio;

struct InteractiveMesh : Mesh {
    enum GeometryMode {
        GeometryMode_Poly, // The base polyhedral mesh.
        GeometryMode_Tets, // Tetrahedral mesh, generated from the polyhedral mesh.
        GeometryMode_ConvexHull, // Convex hull of the polyhedral mesh.
    };

    // Load a 3D mesh from a .obj file, or a 2D profile from a .svg file.
    InteractiveMesh(::Scene &, fs::path file_path);
    ~InteractiveMesh();

    inline const GLGeometry &GetGeometry() const override {
        switch (ActiveGeometryMode) {
            case GeometryMode_Poly: return Polyhedron;
            case GeometryMode_Tets: return Tets;
            case GeometryMode_ConvexHull: return ConvexHull;
        }
    }

    const GLGeometry &GetTets() const { return Tets; }

    void Generate() override;
    void Delete() const override;

    void PrepareRender(RenderMode) override;
    void PostRender(RenderMode) override;

    // Trigger the nearest excitation vertex nearest to the provided vertex.
    void TriggerVertex(uint vertex_index, float amount = 1);
    void ReleaseTrigger();

    void RenderConfig();
    void RenderProfile();
    void RenderProfileConfig();
    void Save(fs::path file_path) const; // Export the active mesh to a .obj file.

    bool HasProfile() const { return Profile != nullptr; }

    bool HasTets() const { return !Tets.Empty(); }
    bool HasConvexHull() const { return !ConvexHull.Empty(); }

    std::string GenerateDsp() const;

    void ApplyTransform();
    glm::mat4 GetTransform() const;

    int NumExcitableVertices = 10;
    bool ShowExcitableVertices = true; // Only shown when viewing tet mesh.
    bool QualityTets = true;
    bool AutomaticTetGeneration = true;

    fs::path FilePath; // Most recently loaded file path.

private:
    void SetGeometryMode(GeometryMode);

    void UpdateHoveredVertex();
    void UpdateExcitableVertices();
    void UpdateExcitableVertexColors();

    void GenerateTets(); // Populates `TetGenResult`.
    void UpdateTets(); // Update the `Tets` geometry from `TetGenResult`.

    // Generate an axisymmetric 3D mesh by rotating the current 2D profile about the y-axis.
    // _This will have no effect if `Load(path)` was not called first to load a 2D profile._
    void ExtrudeProfile();
    void LoadRealImpact(); // Load [RealImpact](https://github.com/khiner/RealImpact) in the same directory as the loaded .obj file.

    Scene &Scene;

    GLGeometry Tets, ConvexHull;

    // Bounds of original loaded mesh, before any transformations.
    // Used to determine initial camera distance and scale of auto-generated geometries.
    std::pair<glm::vec3, glm::vec3> InitialBounds; // [{min_x, min_y, min_z}, {max_x, max_y, max_z}]

    glm::vec3 Translation{0.f}, Scale{1.f}, RotationAngles{0.0f};

    GeometryMode ActiveGeometryMode = GeometryMode_Poly;

    std::unique_ptr<tetgenio> TetGenResult;
    std::unique_ptr<MeshProfile> Profile;
    std::unique_ptr<::RealImpact> RealImpact;

    Worker TetGenerator{"Generate tet mesh", "Generating tetrahedral mesh...", [&] { GenerateTets(); }};
    Worker RealImpactLoader{"Load RealImpact", "Loading RealImpact data...", [&] { LoadRealImpact(); }};

    int HoveredVertexIndex = -1, CameraTargetVertexIndex = -1;
    Mesh HoveredVertexArrow{Arrow{0.5, 0.1, 0.2, 0.3}, this};

    std::vector<int> ExcitableVertexIndices; // Indexes into `Tets` vertices.
    Mesh ExcitableVertexArrows{Arrow{0.25, 0.05, 0.1, 0.15}, this}; // Instanced arrows for each excitable vertex, with less emphasis than `HoveredVertexArrow`.
    Mesh RealImpactListenerPoints{Sphere{0.01}}; // Instanced spheres for each listener point.
};
