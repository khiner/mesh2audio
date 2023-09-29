#include "Scene.h"

#include "GLCanvas.h"
#include "Shader/ShaderProgram.h"

#include <algorithm>
#include <format>
#include <iostream>
#include <string>

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;

using std::string;

using namespace ImGui;

static GLCanvas Canvas;

namespace UniformName {
inline static const string
    LightPosition = "light_position",
    LightColor = "light_color",
    AmbientColor = "ambient_color",
    DiffuseColor = "diffuse_color",
    SpecularColor = "specular_color",
    ShininessFactor = "shininess_factor",
    Projection = "projection",
    CameraView = "camera_view",
    FlatShading = "flat_shading",
    LineWidth = "line_width";
} // namespace UniformName

Scene::Scene() {
    // Initialize all colors to white, and initialize the light positions to be in a circle on the xz plane.
    std::fill_n(LightColors, NumLights * 4, 1.0f);
    for (uint i = 0; i < NumLights; i++) {
        const float ratio = 2 * float(i) / NumLights;
        const float dist = 15.0f;
        LightPositions[i * 4 + 0] = dist * __cospif(ratio);
        LightPositions[i * 4 + 1] = 0;
        LightPositions[i * 4 + 2] = dist * __sinpif(ratio);
        LightPositions[4 * i + 3] = 1.0f;
        LightColors[4 * i + 3] = 1.0f;
    }

    /**
      Initialize a right-handed coordinate system, with:
        * Positive x pointing right
        * Positive y pointing up, and
        * Positive z pointing forward (toward the camera).
      This would put the camera `eye` at position (0, 0, camDistance) in world space, pointing at the origin.
      We offset the camera angle slightly from this point along spherical coordinates to make the initial view more interesting.
    */
    static const float x_angle = M_PI * 0.1; // Elevation angle (0° is in the X-Z plane, positive angles rotate upwards)
    static const float y_angle = M_PI * 0.4; // Azimuth angle (0° is along +X axis, positive angles rotate counterclockwise)
    static const vec3 eye(cosf(y_angle) * cosf(x_angle), sinf(x_angle), sinf(y_angle) * cosf(x_angle));
    CameraView = glm::lookAt(eye * CameraDistance, Origin, Up);

    namespace un = UniformName;
    static const fs::path ShaderDir = fs::path("res") / "shaders";
    static const Shader
        TransformVertexShader{GL_VERTEX_SHADER, ShaderDir / "transform_vertex.glsl", {un::Projection, un::CameraView}},
        TransformVertexLinesShader{GL_VERTEX_SHADER, ShaderDir / "transform_vertex_lines.glsl", {un::Projection, un::CameraView}},
        LinesGeometryShader{GL_GEOMETRY_SHADER, ShaderDir / "lines_geom.glsl", {un::LineWidth}},
        FragmentShader{GL_FRAGMENT_SHADER, ShaderDir / "fragment.glsl", {un::CameraView, un::LightPosition, un::LightColor, un::AmbientColor, un::DiffuseColor, un::SpecularColor, un::ShininessFactor, un::FlatShading}};

    MainShaderProgram = std::make_unique<ShaderProgram>(std::vector<const Shader *>{&TransformVertexShader, &FragmentShader});
    LinesShaderProgram = std::make_unique<ShaderProgram>(std::vector<const Shader *>{&TransformVertexLinesShader, &LinesGeometryShader, &FragmentShader});

    CurrShaderProgram = MainShaderProgram.get();
    CurrShaderProgram->Use();
}

Scene::~Scene() {}

void Scene::AddGeometry(Geometry *geometry) {
    if (!geometry) return;
    if (std::find(Geometries.begin(), Geometries.end(), geometry) != Geometries.end()) return;

    Geometries.push_back(geometry);
}

void Scene::RemoveGeometry(const Geometry *geometry) {
    if (!geometry) return;

    Geometries.erase(std::remove(Geometries.begin(), Geometries.end(), geometry), Geometries.end());
}

void Scene::SetupRender() {
    const auto &io = ImGui::GetIO();
    const bool window_hovered = IsWindowHovered();
    if (window_hovered && io.MouseWheel != 0) {
        SetCameraDistance(CameraDistance * (1.f - io.MouseWheel / 16.f));
    }
    const auto content_region = GetContentRegionAvail();
    UpdateCameraProjection(content_region);
    if (content_region.x <= 0 && content_region.y <= 0) return;

    const auto bg = GetStyleColorVec4(ImGuiCol_WindowBg);
    Canvas.SetupRender(content_region.x, content_region.y, bg.x, bg.y, bg.z, bg.w);

    namespace un = UniformName;
    glUniformMatrix4fv(CurrShaderProgram->GetUniform(un::Projection), 1, GL_FALSE, &CameraProjection[0][0]);
    glUniformMatrix4fv(CurrShaderProgram->GetUniform(un::CameraView), 1, GL_FALSE, &CameraView[0][0]);
    glUniform4fv(CurrShaderProgram->GetUniform(un::LightPosition), NumLights, LightPositions);
    glUniform4fv(CurrShaderProgram->GetUniform(un::LightColor), NumLights, LightColors);
    glUniform4fv(CurrShaderProgram->GetUniform(un::AmbientColor), 1, AmbientColor);
    glUniform4fv(CurrShaderProgram->GetUniform(un::DiffuseColor), 1, DiffusionColor);
    glUniform4fv(CurrShaderProgram->GetUniform(un::SpecularColor), 1, SpecularColor);
    glUniform1f(CurrShaderProgram->GetUniform(un::ShininessFactor), Shininess);
    glUniform1i(CurrShaderProgram->GetUniform(un::FlatShading), UseFlatShading && RenderMode == RenderType_Smooth ? 1 : 0);

    if (RenderMode == RenderType_Lines) {
        glUniform1f(CurrShaderProgram->GetUniform(un::LineWidth), LineWidth);
    }

    for (auto *geometry : Geometries) geometry->SetupRender(RenderMode);
}

void Scene::Draw() {
    SetupRender();
    // auto start_time = std::chrono::high_resolution_clock::now();
    if (RenderMode == RenderType_Points) glPointSize(PointRadius);
    for (const auto *geometry : Geometries) geometry->Render(RenderMode);
    // std::cout << "Draw time: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count() << "us" << std::endl;
    Render();
}

void Scene::Render() {
    // Render the scene to an OpenGl texture and display it (without changing the cursor position).
    const auto &content_region = GetContentRegionAvail();
    const auto &cursor = GetCursorPos();
    unsigned int texture_id = Canvas.Render();
    Image((void *)(intptr_t)texture_id, content_region, {0, 1}, {1, 0});
    SetCursorPos(cursor);

    // Render ImGuizmo.
    const auto &window_pos = GetWindowPos();
    if (ShowGizmo || ShowCameraGizmo || ShowGrid) {
        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(window_pos.x, window_pos.y + GetTextLineHeightWithSpacing(), content_region.x, content_region.y);
    }
    if (ShowGrid) {
        ImGuizmo::DrawGrid(&CameraView[0][0], &CameraProjection[0][0], &Identity[0][0], 100.f);
    }
    if (GizmoCallback) {
        if (ImGuizmo::Manipulate(
                &CameraView[0][0], &CameraProjection[0][0], GizmoOp, ImGuizmo::LOCAL, &GizmoTransform[0][0], nullptr,
                nullptr, ShowBounds ? Bounds : nullptr, nullptr
            )) {
            GizmoCallback(GizmoTransform);
        }
    }
    if (ShowCameraGizmo) {
        static const float ViewManipulateSize = 128;
        const auto view_manipulate_pos = window_pos + ImVec2{GetWindowContentRegionMax().x - ViewManipulateSize, GetWindowContentRegionMin().y};
        ImGuizmo::ViewManipulate(&CameraView[0][0], CameraDistance, view_manipulate_pos, {ViewManipulateSize, ViewManipulateSize}, 0);
    }
}

void Scene::RenderGizmoDebug() {
    if (!ShowGizmo) return;

    SeparatorText("Gizmo");
    using namespace ImGuizmo;
    const string interaction_text = std::format(
        "Interaction: {}",
        IsUsing()             ? "Using Gizmo" :
            IsOver(TRANSLATE) ? "Translate hovered" :
            IsOver(ROTATE)    ? "Rotate hovered" :
            IsOver(SCALE)     ? "Scale hovered" :
            IsOver()          ? "Hovered" :
                                "Not interacting"
    );
    TextUnformatted(interaction_text.c_str());

    if (IsKeyPressed(ImGuiKey_T)) GizmoOp = TRANSLATE;
    if (IsKeyPressed(ImGuiKey_R)) GizmoOp = ROTATE;
    if (IsKeyPressed(ImGuiKey_S)) GizmoOp = SCALE;
    if (RadioButton("Translate (T)", GizmoOp == TRANSLATE)) GizmoOp = TRANSLATE;
    if (RadioButton("Rotate (R)", GizmoOp == ROTATE)) GizmoOp = ROTATE;
    if (RadioButton("Scale (S)", GizmoOp == SCALE)) GizmoOp = SCALE;
    if (RadioButton("Universal", GizmoOp == UNIVERSAL)) GizmoOp = UNIVERSAL;
    Checkbox("Bound sizing", &ShowBounds);
}

void Scene::RenderConfig() {
    if (BeginTabBar("SceneConfig")) {
        if (BeginTabItem("Geometries")) {
            SeparatorText("Render mode");
            bool render_mode_changed = RadioButton("Smooth", &RenderMode, RenderType_Smooth);
            SameLine();
            render_mode_changed |= RadioButton("Lines", &RenderMode, RenderType_Lines);
            SameLine();
            render_mode_changed |= RadioButton("Point cloud", &RenderMode, RenderType_Points);
            if (render_mode_changed) {
                CurrShaderProgram = RenderMode == RenderType_Lines ? LinesShaderProgram.get() : MainShaderProgram.get();
                CurrShaderProgram->Use();
            }
            if (RenderMode == RenderType_Smooth) {
                Checkbox("Flat shading", &UseFlatShading);
            }
            if (RenderMode == RenderType_Lines) {
                SliderFloat("Line width", &LineWidth, 0.0001f, 0.04f, "%.4f", ImGuiSliderFlags_Logarithmic);
            }
            if (RenderMode == RenderType_Points) {
                SliderFloat("Point radius", &PointRadius, 0.1, 10, "%.2f", ImGuiSliderFlags_Logarithmic);
            }
            EndTabItem();
        }
        if (BeginTabItem("Camera")) {
            Checkbox("Show gizmo", &ShowCameraGizmo);
            SameLine();
            Checkbox("Grid", &ShowGrid);
            SliderFloat("FOV", &fov, 20.f, 110.f);

            float camera_distance = CameraDistance;
            if (SliderFloat("Distance", &camera_distance, .1f, 10.f)) {
                SetCameraDistance(camera_distance);
            }
            EndTabItem();
        }
        if (BeginTabItem("Lighing")) {
            SeparatorText("Colors");
            Checkbox("Custom colors", &CustomColors);
            if (CustomColors) {
                ColorEdit3("Ambient", &AmbientColor[0]);
                ColorEdit3("Diffusion", &DiffusionColor[0]);
                ColorEdit3("Specular", &SpecularColor[0]);
                SliderFloat("Shininess", &Shininess, 0.0f, 150.0f);
            } else {
                for (int i = 1; i < 3; i++) {
                    AmbientColor[i] = AmbientColor[0];
                    DiffusionColor[i] = DiffusionColor[0];
                    SpecularColor[i] = SpecularColor[0];
                }
                SliderFloat("Ambient", &AmbientColor[0], 0.0f, 1.0f);
                SliderFloat("Diffusion", &DiffusionColor[0], 0.0f, 1.0f);
                SliderFloat("Specular", &SpecularColor[0], 0.0f, 1.0f);
                SliderFloat("Shininess", &Shininess, 0.0f, 150.0f);
            }

            SeparatorText("Lights");
            for (int i = 0; i < NumLights; i++) {
                Separator();
                PushID(i);
                Text("Light %d", i + 1);
                SliderFloat3("Positions", &LightPositions[4 * i], -25.0f, 25.0f);
                ColorEdit3("Color", &LightColors[4 * i]);
                PopID();
            }
            EndTabItem();
        }
        EndTabBar();
    }
}

void Scene::SetCameraDistance(float distance) {
    // Extract the eye position from inverse camera view matrix and update the camera view based on the new distance.
    const vec3 eye = glm::inverse(CameraView)[3];
    CameraView = glm::lookAt(eye * (distance / CameraDistance), Origin, Up);
    CameraDistance = distance;
}

void Scene::UpdateCameraProjection(const ImVec2 &size) {
    CameraProjection = glm::perspective(glm::radians(fov * 2), size.x / size.y, 0.1f, 100.f);
}
