#pragma once

#include <functional>
#include <unordered_map>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "ImGuizmo.h"

#include "Mesh/Mesh.h"

struct ShaderProgram;
struct Rect;
struct Physics;

struct Scene {
    Scene();
    ~Scene();

    void SetCameraDistance(float);

    void AddMesh(Mesh *);
    void RemoveMesh(const Mesh *);

    void Render();
    void RenderConfig();
    void RenderGizmoDebug();

    std::vector<Mesh *> Meshes;

    LightBuffer Lights;
    glm::vec4 AmbientColor = {0.2, 0.2, 0.2, 1};
    // todo Diffusion and specular colors are object properties, not scene properties.
    glm::vec4 DiffusionColor = {0.2, 0.2, 0.2, 1};
    glm::vec4 SpecularColor = {0.5, 0.5, 0.5, 1};
    float Shininess = 10;
    float LineWidth = 0.005, PointRadius = 1;
    bool CustomColors = false, UseFlatShading = true;

    bool ShowCameraGizmo = true, ShowGizmo = false, ShowBounds = false;

    glm::mat4 GizmoTransform{1};
    std::function<void(const glm::mat4 &)> GizmoCallback;

    ImGuizmo::OPERATION GizmoOp{ImGuizmo::TRANSLATE};

    glm::mat4 CameraView, CameraProjection;
    float CameraDistance = 4, fov = 50;

    inline static uint MaxNumLights = 5;
    inline static float Bounds[6] = {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    inline static RenderMode ActiveRenderMode = RenderMode_Smooth;

    std::unique_ptr<ShaderProgram> MainShaderProgram, LinesShaderProgram, GridLinesShaderProgram;

    ShaderProgram *CurrShaderProgram = nullptr;

    std::unordered_map<uint, std::unique_ptr<Mesh>> LightPoints; // For visualizing light positions. Key is `Lights` index.

    std::unique_ptr<Mesh> Grid, Floor;
    std::unique_ptr<Physics> Physics;
};
