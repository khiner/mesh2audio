#include "Scene.h"

#include "GLCanvas.h"
#include "Geometry/Primitive/Sphere.h"
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
    ShadowMap = "shadow_map",
    NumLights = "num_lights",
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
    SetupShadowMap();

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
    Lights[0].SetPosition({dist_factor * __cospif(key_light__angle), 0, dist_factor * __sinpif(key_light__angle)});
    // Fill light, twice as far away to make it less intense.
    Lights[1].SetPosition({-dist_factor * __cospif(key_light__angle) * 2, 0, -dist_factor * __sinpif(key_light__angle) * 2});
    // Back light.
    Lights[2].SetPosition({0, dist_factor * 1.5, -dist_factor});

    // Point all lights at the origin.
    for (auto &light : Lights) light.SetDirection(-glm::vec3(light.Position));

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
        FragmentShader{GL_FRAGMENT_SHADER, ShaderDir / "fragment.glsl", {un::ShadowMap, un::NumLights, un::AmbientColor, un::DiffuseColor, un::SpecularColor, un::ShininessFactor, un::FlatShading}},
        ShadowVertexShader{GL_VERTEX_SHADER, ShaderDir / "shadow_vertex.glsl", {}},
        ShadowFragmentShader{GL_FRAGMENT_SHADER, ShaderDir / "shadow_fragment.glsl", {}};

    MainShaderProgram = std::make_unique<ShaderProgram>(std::vector<const Shader *>{&TransformVertexShader, &FragmentShader});
    LinesShaderProgram = std::make_unique<ShaderProgram>(std::vector<const Shader *>{&TransformVertexLinesShader, &LinesGeometryShader, &FragmentShader});
    ShadowShaderProgram = std::make_unique<ShaderProgram>(std::vector<const Shader *>{&ShadowVertexShader, &ShadowFragmentShader});

    ShadowShaderProgram->Use();
    Lights.BindData();
    glBindBufferBase(GL_UNIFORM_BUFFER, glGetUniformBlockIndex(ShadowShaderProgram->Id, "LightBlock"), Lights.BufferId);

    MainShaderProgram->Use();
    Lights.BindData();
    glBindBufferBase(GL_UNIFORM_BUFFER, glGetUniformBlockIndex(MainShaderProgram->Id, "LightBlock"), Lights.BufferId);

    CurrShaderProgram = MainShaderProgram.get();
    CurrShaderProgram->Use();
}

Scene::~Scene() {}

void Scene::SetupShadowMap() {
    glGenFramebuffers(1, &ShadowFramebufferId);
    glGenTextures(1, &DepthTextureId);
    glBindTexture(GL_TEXTURE_2D, DepthTextureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    // For shadow mapping, set the border color to white since we are using depth values in the range [0, 1].
    // This way, any lookup outside the texture range will return a depth that will not produce a shadow.
    GLfloat border_color[] = {1.0, 1.0, 1.0, 1.0};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

    // Allocate and attach the depth texture as the framebuffer's depth buffer.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, ShadowMapSize.x, ShadowMapSize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, ShadowFramebufferId);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTextureId, 0);

    // We are not rendering color.
    // The shadow mapping framebuffer is used only to capture depth information from the light's point of view.
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Framebuffer not complete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene::AddGeometry(Geometry *geometry) {
    if (!geometry) return;
    if (std::find(Geometries.begin(), Geometries.end(), geometry) != Geometries.end()) return;

    Geometries.push_back(geometry);
}

void Scene::RemoveGeometry(const Geometry *geometry) {
    if (!geometry) return;

    Geometries.erase(std::remove(Geometries.begin(), Geometries.end(), geometry), Geometries.end());
}

void Scene::Draw() {
    const auto &io = ImGui::GetIO();
    const bool window_hovered = IsWindowHovered();
    if (window_hovered && io.MouseWheel != 0) {
        SetCameraDistance(CameraDistance * (1.f - io.MouseWheel / 16.f));
    }
    const auto content_region = GetContentRegionAvail();
    CameraProjection = glm::perspective(glm::radians(fov * 2), content_region.x / content_region.y, 0.1f, 100.f);
    if (content_region.x <= 0 && content_region.y <= 0) return;

    Lights.BindData();
    for (auto *geometry : Geometries) {
        if (RenderMode == RenderType_Lines && geometry->LineIndices.empty()) geometry->ComputeLineIndices();
        else if (RenderMode != RenderType_Lines && !geometry->LineIndices.empty()) geometry->LineIndices.clear(); // Save memory.
    }

    if (RenderMode == RenderType_Smooth) {
        // Shadow mapping (which is only meaningful when rendering closed geometries - not points or lines).
        glBindFramebuffer(GL_FRAMEBUFFER, ShadowFramebufferId);
        glViewport(0, 0, ShadowMapSize.x, ShadowMapSize.y);
        glClear(GL_DEPTH_BUFFER_BIT);
        ShadowShaderProgram->Use();
        for (const auto *geometry : Geometries) geometry->Render(RenderMode);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    CurrShaderProgram->Use();
    const auto bg = GetStyleColorVec4(ImGuiCol_WindowBg);
    Canvas.SetupRender(content_region.x, content_region.y, bg.x, bg.y, bg.z, bg.w);

    glActiveTexture(GL_TEXTURE0); // Bind the shadow map
    glBindTexture(GL_TEXTURE_2D, DepthTextureId);

    namespace un = UniformName;
    glUniformMatrix4fv(CurrShaderProgram->GetUniform(un::Projection), 1, GL_FALSE, &CameraProjection[0][0]);
    glUniformMatrix4fv(CurrShaderProgram->GetUniform(un::CameraView), 1, GL_FALSE, &CameraView[0][0]);
    glUniform1i(CurrShaderProgram->GetUniform(un::ShadowMap), 0);
    glUniform1i(CurrShaderProgram->GetUniform(un::NumLights), Lights.size());
    glUniform4fv(CurrShaderProgram->GetUniform(un::AmbientColor), 1, &AmbientColor[0]);
    glUniform4fv(CurrShaderProgram->GetUniform(un::DiffuseColor), 1, &DiffusionColor[0]);
    glUniform4fv(CurrShaderProgram->GetUniform(un::SpecularColor), 1, &SpecularColor[0]);
    glUniform1f(CurrShaderProgram->GetUniform(un::ShininessFactor), Shininess);
    glUniform1i(CurrShaderProgram->GetUniform(un::FlatShading), UseFlatShading && RenderMode == RenderType_Smooth ? 1 : 0);

    if (RenderMode == RenderType_Lines) {
        glUniform1f(CurrShaderProgram->GetUniform(un::LineWidth), LineWidth);
    }

    // auto start_time = std::chrono::high_resolution_clock::now();
    if (RenderMode == RenderType_Points) glPointSize(PointRadius);
    for (const auto *geometry : Geometries) geometry->Render(RenderMode);
    // std::cout << "Draw time: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count() << "us" << std::endl;

    // Render the scene to an OpenGl texture and display it (without changing the cursor position).
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
                        LightPoints[i] = std::make_unique<Sphere>(0.1);
                        LightPoints[i]->SetColor(Lights[i].Color);
                        LightPoints[i]->SetPosition(Lights[i].Position);
                        AddGeometry(LightPoints[i].get());
                    } else {
                        RemoveGeometry(LightPoints[i].get());
                        LightPoints.erase(i);
                    }
                }
                if (SliderFloat3("Position", &Lights[i].Position[0], -8, 8)) {
                    Lights[i].UpdateViewProjection();
                    if (LightPoints.contains(i)) LightPoints[i]->SetPosition(Lights[i].Position);
                }
                if (SliderFloat3("Direction", &Lights[i].Direction[0], -1, 1)) {
                    Lights[i].UpdateViewProjection();
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

void Scene::SetCameraDistance(float distance) {
    // Extract the eye position from inverse camera view matrix and update the camera view based on the new distance.
    const vec3 eye = glm::inverse(CameraView)[3];
    CameraView = glm::lookAt(eye * (distance / CameraDistance), Origin, Up);
    CameraDistance = distance;
}
