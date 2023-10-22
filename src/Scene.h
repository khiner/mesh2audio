#pragma once

#include <functional>
#include <unordered_map>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "ImGuizmo.h"

#include "Mesh/Mesh.h"

struct GLCanvas;
struct ShaderProgram;
struct Rect;
struct Physics;

struct Light {
    glm::vec4 Position{0.0f};
    glm::vec4 Color{1.0f};
};

struct Scene {
    Scene();
    ~Scene();

    void AddMesh(Mesh *);
    void RemoveMesh(const Mesh *);

    void Render();
    void RenderConfig();
    void RenderGizmoDebug();

    void SetCameraDistance(float);

    std::vector<Mesh *> Meshes;

    GLuint LightBufferId;
    std::vector<Light> Lights;
    glm::vec4 AmbientColor = {0.2, 0.2, 0.2, 1};
    // todo Diffusion and specular colors are object properties, not scene properties.
    glm::vec4 DiffusionColor = {0.3, 0.3, 0.3, 1};
    glm::vec4 SpecularColor = {0.15, 0.15, 0.15, 1};
    float Shininess = 10;
    float LineWidth = 0.005, PointRadius = 1;
    bool CustomColors = false;

    bool ShowCameraGizmo = true, ShowGizmo = false, ShowBounds = false;

    glm::mat4 GizmoTransform{1};
    std::function<void(const glm::mat4 &)> GizmoCallback;
    ImGuizmo::OPERATION GizmoOp{ImGuizmo::TRANSLATE};

    glm::mat4 CameraView, CameraProjection;
    float CameraDistance = 4, fov = 50;

    inline static float Bounds[6] = {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    inline static RenderMode ActiveRenderMode = RenderMode::Flat;

private:
    void UpdateNormalIndicators();

    std::unique_ptr<ShaderProgram> MainShaderProgram, LinesShaderProgram, SilhouetteShaderProgram, GridLinesShaderProgram;
    ShaderProgram *CurrShaderProgram = nullptr;
    std::unordered_map<uint, std::unique_ptr<Mesh>> LightPoints; // For visualizing light positions. Key is `Lights` index.

    std::unique_ptr<GLCanvas> Canvas;
    std::unique_ptr<Mesh> Grid;
    std::unique_ptr<Mesh> NormalIndicator; // Instanced mesh for displaying normals.
    glm::vec4 NormalIndicatorColor = {0, 0, 1, 1};
    float NormalIndicatorLength = 1.f; // This is normalized by a factor based on the mesh's bounding box diagonal length.

    glm::vec4 SilhouetteColor = {1, 0.5, 0, 1};

    enum class NormalIndicatorMode {
        None,
        Vertex,
        Face,
    };

    NormalIndicatorMode NormalMode = NormalIndicatorMode::None;
};
