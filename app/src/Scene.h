#pragma once

#include <glm/mat4x4.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "ImGuizmo.h"

#include "Geometry/Geometry.h"

struct ImVec2;

struct Scene {
    enum RenderType_ {
        RenderType_Smooth,
        RenderType_Lines,
        RenderType_Points,
        RenderType_Mesh
    };
    using RenderType = int;

    inline static const int NumLights = 5;
    float LightPositions[NumLights * 4] = {0.0f};
    float LightColors[NumLights * 4] = {0.0f};
    float AmbientColor[4] = {0.05, 0.05, 0.05, 1};
    float DiffusionColor[4] = {0.2, 0.2, 0.2, 1};
    float SpecularColor[4] = {0.5, 0.5, 0.5, 1};
    float Shininess = 10;
    bool CustomColors = false;

    bool ShowCameraGizmo = true, ShowGrid = false, ShowGizmo = false, ShowBounds = false;
    ImGuizmo::OPERATION GizmoOp{ImGuizmo::TRANSLATE};

    glm::mat4 ObjectMatrix{1.f}, CameraView, CameraProjection;
    float CameraDistance = 4, fov = 27;

    inline static float Bounds[6] = {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
    inline static RenderType RenderMode = RenderType_Smooth;

    Scene();
    ~Scene() = default;

    void SetCameraDistance(float distance);
    void UpdateCameraProjection(const ImVec2 &size);
    void RestoreDefaultMaterial();

    void Draw(const Geometry &);

    void SetupRender();
    void Render();
    void RenderConfig();
};
