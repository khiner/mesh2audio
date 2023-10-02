#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "ImGuizmo.h"

#include "Geometry/Geometry.h"

struct ImVec2;

struct ShaderProgram;

struct Scene {
    Scene();
    ~Scene();

    void SetCameraDistance(float);
    void UpdateCameraProjection(const ImVec2 &size);

    void AddGeometry(Geometry *);
    void RemoveGeometry(const Geometry *);

    void Draw();
    void RenderConfig();
    void RenderGizmoDebug();

    LightBuffer Lights{5};
    glm::vec4 AmbientColor = {0.05, 0.05, 0.05, 1};
    glm::vec4 DiffusionColor = {0.2, 0.2, 0.2, 1};
    glm::vec4 SpecularColor = {0.5, 0.5, 0.5, 1};
    float Shininess = 10;
    float LineWidth = 0.005, PointRadius = 1;
    bool CustomColors = false, UseFlatShading = true;

    bool ShowCameraGizmo = true, ShowGrid = false, ShowGizmo = false, ShowBounds = false;

    glm::mat4 GizmoTransform{1};
    std::function<void(const glm::mat4 &)> GizmoCallback;

    ImGuizmo::OPERATION GizmoOp{ImGuizmo::TRANSLATE};

    glm::mat4 CameraView, CameraProjection;
    float CameraDistance = 4, fov = 27;

    inline static float Bounds[6] = {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    inline static RenderType RenderMode = RenderType_Smooth;

    std::unique_ptr<ShaderProgram> MainShaderProgram, LinesShaderProgram;

    ShaderProgram *CurrShaderProgram = nullptr;

    std::vector<Geometry *> Geometries;
    std::unique_ptr<Geometry> LightPoints[5]; // For visualizing light positions.

private:
    void SetupRender();
    void Render();
};
