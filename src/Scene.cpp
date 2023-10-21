#include "Scene.h"

#include <format>
#include <string>

#include <glm/gtx/quaternion.hpp>

#include "GLCanvas.h"
#include "Geometry/Primitive/Rect.h"
#include "Geometry/Primitive/Sphere.h"
#include "Shader/ShaderProgram.h"

namespace UniformName {
inline static const std::string
    NumLights = "num_lights",
    AmbientColor = "ambient_color",
    DiffuseColor = "diffuse_color",
    SpecularColor = "specular_color",
    ShininessFactor = "shininess_factor",
    Projection = "projection",
    CameraView = "camera_view",
    FlatShading = "flat_shading",
    LineWidth = "line_width",
    GridLinesColor = "grid_lines_color";
} // namespace UniformName

Scene::Scene() {
    Canvas = std::make_unique<GLCanvas>();

    /**
      Initialize light positions using a three-point lighting system:
        1) Key light: The main light, positioned at a 45-degree angle from the subject.
        2) Fill light: Positioned opposite the key light to fill the shadows. It's less intense than the key light.
        3) Back light: Positioned behind and above the subject to create a rim of light around the subject, separating it from the background.
      We consider the "subject" to be the origin.
    */
    for (int _ = 0; _ < 3; _++) Lights.push_back({});
    static const float dist_factor = 2.0f;

    // Key light.
    float key_light__angle = 1.f / 4.f; // Multiplied by pi.
    Lights[0].Position = {dist_factor * __cospif(key_light__angle), 0, dist_factor * __sinpif(key_light__angle), 1};
    // Fill light, twice as far away to make it less intense.
    Lights[1].Position = {-dist_factor * __cospif(key_light__angle) * 2, 0, -dist_factor * __sinpif(key_light__angle) * 2, 1};
    // Back light.
    Lights[2].Position = {0, dist_factor * 1.5, -dist_factor, 1};

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
    static const glm::vec3 eye(cosf(y_angle) * cosf(x_angle), sinf(x_angle), sinf(y_angle) * cosf(x_angle));
    CameraView = glm::lookAt(eye * CameraDistance, Origin, Up);

    namespace un = UniformName;
    static const fs::path ShaderDir = fs::path("res") / "shaders";
    static const Shader
        TransformVertexShader{GL_VERTEX_SHADER, ShaderDir / "transform_vertex.glsl", {un::Projection, un::CameraView}},
        TransformVertexLinesShader{GL_VERTEX_SHADER, ShaderDir / "transform_vertex_lines.glsl", {un::Projection, un::CameraView}},
        LinesGeometryShader{GL_GEOMETRY_SHADER, ShaderDir / "lines_geom.glsl", {un::LineWidth}},
        FragmentShader{GL_FRAGMENT_SHADER, ShaderDir / "fragment.glsl", {un::NumLights, un::AmbientColor, un::DiffuseColor, un::SpecularColor, un::ShininessFactor, un::FlatShading}},
        GridLinesVertexShader{GL_VERTEX_SHADER, ShaderDir / "grid_lines_vertex.glsl", {un::Projection, un::CameraView}},
        GridLinesFragmentShader{GL_FRAGMENT_SHADER, ShaderDir / "grid_lines_fragment.glsl", {}};

    MainShaderProgram = std::make_unique<ShaderProgram>(std::vector<const Shader *>{&TransformVertexShader, &FragmentShader});
    LinesShaderProgram = std::make_unique<ShaderProgram>(std::vector<const Shader *>{&TransformVertexLinesShader, &LinesGeometryShader, &FragmentShader});
    GridLinesShaderProgram = std::make_unique<ShaderProgram>(std::vector<const Shader *>{&GridLinesVertexShader, &GridLinesFragmentShader});

    CurrShaderProgram = MainShaderProgram.get();
    CurrShaderProgram->Use();

    glGenBuffers(1, &LightBufferId);
    glBindBuffer(GL_UNIFORM_BUFFER, LightBufferId);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Light) * Lights.size(), Lights.data(), GL_STATIC_DRAW);

    GLuint light_block_index = glGetUniformBlockIndex(CurrShaderProgram->Id, "LightBlock");
    glBindBufferBase(GL_UNIFORM_BUFFER, light_block_index, LightBufferId);
}

Scene::~Scene() {
    glDeleteBuffers(1, &LightBufferId);
}

void Scene::AddMesh(Mesh *mesh) {
    if (!mesh) return;
    if (std::find(Meshes.begin(), Meshes.end(), mesh) != Meshes.end()) return;

    Meshes.push_back(mesh);
}

void Scene::RemoveMesh(const Mesh *mesh) {
    if (!mesh) return;

    Meshes.erase(std::remove(Meshes.begin(), Meshes.end(), mesh), Meshes.end());
}

void Scene::SetCameraDistance(float distance) {
    // Extract the eye position from inverse camera view matrix and update the camera view based on the new distance.
    const glm::vec3 eye = glm::inverse(CameraView)[3];
    CameraView = glm::lookAt(eye * (distance / CameraDistance), Origin, Up);
    CameraDistance = distance;
}

using namespace ImGui;

void Scene::Render() {
    const auto &io = ImGui::GetIO();
    const bool window_hovered = IsWindowHovered();
    if (window_hovered && io.MouseWheel != 0) {
        SetCameraDistance(CameraDistance * (1.f - io.MouseWheel / 16.f));
    }
    const auto content_region = GetContentRegionAvail();
    CameraProjection = glm::perspective(glm::radians(fov), content_region.x / content_region.y, 0.1f, 100.f);

    if (content_region.x <= 0 && content_region.y <= 0) return;

    const auto bg = GetStyleColorVec4(ImGuiCol_WindowBg);
    Canvas->PrepareRender(content_region.x, content_region.y, bg.x, bg.y, bg.z, bg.w);

    glBindBuffer(GL_UNIFORM_BUFFER, LightBufferId);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Light) * Lights.size(), Lights.data(), GL_STATIC_DRAW);

    CurrShaderProgram->Use();

    namespace un = UniformName;
    glUniformMatrix4fv(CurrShaderProgram->GetUniform(un::Projection), 1, GL_FALSE, &CameraProjection[0][0]);
    glUniformMatrix4fv(CurrShaderProgram->GetUniform(un::CameraView), 1, GL_FALSE, &CameraView[0][0]);
    glUniform1i(CurrShaderProgram->GetUniform(un::NumLights), Lights.size());
    glUniform4fv(CurrShaderProgram->GetUniform(un::AmbientColor), 1, &AmbientColor[0]);
    glUniform4fv(CurrShaderProgram->GetUniform(un::DiffuseColor), 1, &DiffusionColor[0]);
    glUniform4fv(CurrShaderProgram->GetUniform(un::SpecularColor), 1, &SpecularColor[0]);
    glUniform1f(CurrShaderProgram->GetUniform(un::ShininessFactor), Shininess);
    glUniform1i(CurrShaderProgram->GetUniform(un::FlatShading), UseFlatShading && ActiveRenderMode == RenderMode::Smooth ? 1 : 0);

    if (ActiveRenderMode == RenderMode::Lines) {
        glUniform1f(CurrShaderProgram->GetUniform(un::LineWidth), LineWidth);
    }

    for (auto *mesh : Meshes) mesh->PrepareRender(ActiveRenderMode);

    // auto start_time = std::chrono::high_resolution_clock::now();
    if (ActiveRenderMode == RenderMode::Points) glPointSize(PointRadius);

    for (const auto *mesh : Meshes) mesh->Render(ActiveRenderMode);
    for (auto *mesh : Meshes) mesh->PostRender(ActiveRenderMode);
    // std::cout << "Draw time: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count() << "us" << std::endl;

    if (Grid) {
        GridLinesShaderProgram->Use();
        namespace un = UniformName;
        glUniformMatrix4fv(GridLinesShaderProgram->GetUniform(un::Projection), 1, GL_FALSE, &CameraProjection[0][0]);
        glUniformMatrix4fv(GridLinesShaderProgram->GetUniform(un::CameraView), 1, GL_FALSE, &CameraView[0][0]);
        // glUniform4fv(GridLinesShaderProgram->GetUniform(un::GridLinesColor), 1, &GridLinesColor[0]);

        glEnable(GL_BLEND);
        Grid->PrepareRender(RenderMode::Smooth);
        Grid->Render(RenderMode::Smooth);
        glDisable(GL_BLEND);
    }

    // Render the scene to an OpenGl texture and display it (without changing the cursor position).
    const auto &cursor = GetCursorPos();
    unsigned int texture_id = Canvas->Render();
    Image((void *)(intptr_t)texture_id, content_region, {0, 1}, {1, 0});
    SetCursorPos(cursor);

    // Render ImGuizmo.
    const auto &window_pos = GetWindowPos();
    if (ShowGizmo || ShowCameraGizmo) {
        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(window_pos.x, window_pos.y + GetTextLineHeightWithSpacing(), content_region.x, content_region.y);
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
    const std::string interaction_text = std::format(
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
            bool show_grid = bool(Grid);
            if (Checkbox("Show grid", &show_grid)) {
                if (show_grid) {
                    // Rendering of grid lines derives from a plane at z = 0.
                    // See `grid_lines_vertex.glsl` and `grid_lines_fragment.glsl` for details.
                    // Based on https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid.
                    Grid = std::make_unique<Mesh>(Rect{{1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {-1, 1, 0}});
                    Grid->ClearColors();
                    Grid->Generate();
                } else {
                    Grid->Delete();
                    Grid.reset();
                }
            }
            SeparatorText("Render mode");
            int render_mode = int(ActiveRenderMode);
            bool render_mode_changed = RadioButton("Smooth", &render_mode, int(RenderMode::Smooth));
            SameLine();
            render_mode_changed |= RadioButton("Lines", &render_mode, int(RenderMode::Lines));
            SameLine();
            render_mode_changed |= RadioButton("Point cloud", &render_mode, int(RenderMode::Points));
            if (render_mode_changed) {
                ActiveRenderMode = RenderMode(render_mode);
                CurrShaderProgram = ActiveRenderMode == RenderMode::Lines ? LinesShaderProgram.get() : MainShaderProgram.get();
            }
            if (ActiveRenderMode == RenderMode::Smooth) {
                Checkbox("Flat shading", &UseFlatShading);
            }
            if (ActiveRenderMode == RenderMode::Lines) {
                SliderFloat("Line width", &LineWidth, 0.0001f, 0.04f, "%.4f", ImGuiSliderFlags_Logarithmic);
            }
            if (ActiveRenderMode == RenderMode::Points) {
                SliderFloat("Point radius", &PointRadius, 0.1, 10, "%.2f", ImGuiSliderFlags_Logarithmic);
            }
            EndTabItem();
        }
        if (BeginTabItem("Camera")) {
            Checkbox("Show gizmo", &ShowCameraGizmo);
            SameLine();
            SliderFloat("FOV", &fov, 20.f, 110.f);

            float camera_distance = CameraDistance;
            if (SliderFloat("Distance", &camera_distance, .1f, 10.f)) {
                SetCameraDistance(camera_distance);
            }
            EndTabItem();
        }
        if (BeginTabItem("Lighting")) {
            SeparatorText("Colors");
            Checkbox("Custom colors", &CustomColors);
            if (CustomColors) {
                ColorEdit3("Ambient", &AmbientColor[0]);
                ColorEdit3("Diffusion", &DiffusionColor[0]);
                ColorEdit3("Specular", &SpecularColor[0]);
            } else {
                for (uint i = 1; i < 3; i++) {
                    AmbientColor[i] = AmbientColor[0];
                    DiffusionColor[i] = DiffusionColor[0];
                    SpecularColor[i] = SpecularColor[0];
                }
                SliderFloat("Ambient", &AmbientColor[0], 0.0f, 1.0f);
                SliderFloat("Diffusion", &DiffusionColor[0], 0.0f, 1.0f);
                SliderFloat("Specular", &SpecularColor[0], 0.0f, 1.0f);
            }
            SliderFloat("Shininess", &Shininess, 0.0f, 150.0f);

            SeparatorText("Lights");
            for (size_t i = 0; i < Lights.size(); i++) {
                Separator();
                PushID(i);
                Text("Light %d", int(i + 1));
                bool show_lights = LightPoints.contains(i);
                if (Checkbox("Show", &show_lights)) {
                    if (show_lights) {
                        LightPoints[i] = std::make_unique<Mesh>(Sphere{0.1});
                        LightPoints[i]->Generate();
                        LightPoints[i]->SetColor(Lights[i].Color);
                        LightPoints[i]->SetPosition(Lights[i].Position);
                        AddMesh(LightPoints[i].get());
                    } else {
                        RemoveMesh(LightPoints[i].get());
                        LightPoints.erase(i);
                    }
                }
                if (SliderFloat3("Position", &Lights[i].Position[0], -8, 8)) {
                    if (LightPoints.contains(i)) LightPoints[i]->SetPosition(Lights[i].Position);
                }
                if (ColorEdit3("Color", &Lights[i].Color[0]) && LightPoints.contains(i)) {
                    LightPoints[i]->SetColor(Lights[i].Color);
                }
                PopID();
            }
            EndTabItem();
        }
        EndTabBar();
    }
}
