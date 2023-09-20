#pragma once

#include <glm/mat4x4.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "ImGuizmo.h"

#include "Geometry.h"

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
    float Ambient[4] = {0.05, 0.05, 0.05, 1};
    float Diffusion[4] = {0.2, 0.2, 0.2, 1};
    float Specular[4] = {0.5, 0.5, 0.5, 1};
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
    void DrawPoint(int vertex_index, const float color[]);
    void DrawPoints(int first, int count, const float color[]);

    void SetupRender();
    void Render();
    void RenderConfig();
};
