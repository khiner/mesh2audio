#include "Scene.h"

#include "GlCanvas.h"
#include "Shader.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <string>

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;

using std::string;

using namespace ImGui;

static GlCanvas gl_canvas;

static const mat4 Identity(1.f);
static const vec3 Origin{0.f}, Up{0.f, 1.f, 0.f};

// Variables to set uniform params for lighting fragment shader
static GLuint lightcol, lightpos, ambientcol, diffusecol, specularcol, emissioncol, shininesscol;
static GLuint projectionPos, modelviewPos;

Scene::Scene() {
    // Initialize all colors to white, and initialize the light positions to be in a circle on the xz plane.
    std::fill_n(LightColors, NumLights * 4, 1.0f);
    for (int i = 0; i < NumLights; i++) {
        const float angle = 2 * M_PI * i / NumLights;
        const float dist = 15.0f;
        LightPositions[i * 4 + 0] = dist * cosf(angle);
        LightPositions[i * 4 + 1] = 0;
        LightPositions[i * 4 + 2] = dist * sinf(angle);
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
    static float x_angle = M_PI / 10; // Elevation angle (0° is in the X-Z plane, positive angles rotate upwards)
    static float y_angle = M_PI / 2 - M_PI / 10; // Azimuth angle (0° is along +X axis, positive angles rotate counterclockwise)
    static vec3 eye(cosf(y_angle) * cosf(x_angle), sinf(x_angle), sinf(y_angle) * cosf(x_angle));
    CameraView = glm::lookAt(eye * CameraDistance, Origin, Up);

    static GLuint vertexshader = Shader::InitShader(GL_VERTEX_SHADER, fs::path("res") / "shaders" / "vertex.glsl");
    static GLuint fragmentshader = Shader::InitShader(GL_FRAGMENT_SHADER, fs::path("res") / "shaders" / "fragment.glsl");
    static GLuint shaderprogram = Shader::InitProgram(vertexshader, fragmentshader);

    lightpos = glGetUniformLocation(shaderprogram, "light_posn");
    lightcol = glGetUniformLocation(shaderprogram, "light_col");
    ambientcol = glGetUniformLocation(shaderprogram, "ambient");
    diffusecol = glGetUniformLocation(shaderprogram, "diffuse");
    specularcol = glGetUniformLocation(shaderprogram, "specular");
    emissioncol = glGetUniformLocation(shaderprogram, "emission");
    shininesscol = glGetUniformLocation(shaderprogram, "shininess");
    projectionPos = glGetUniformLocation(shaderprogram, "projection");
    modelviewPos = glGetUniformLocation(shaderprogram, "modelview");
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
    gl_canvas.SetupRender(content_region.x, content_region.y, bg.x, bg.y, bg.z, bg.w);

    glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &CameraProjection[0][0]);
    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(CameraView * ObjectMatrix)[0][0]);

    glUniform4fv(lightpos, NumLights, LightPositions);
    glUniform4fv(lightcol, NumLights, LightColors);

    glUniform4fv(ambientcol, 1, Ambient);
    glUniform4fv(diffusecol, 1, Diffusion);
    glUniform4fv(specularcol, 1, Specular);
    glUniform1f(shininesscol, Shininess);
}

void Scene::Draw(const Geometry &geometry) {
    const int num_indices = geometry.Indices.size();
    glBindVertexArray(geometry.VertexArray);
    if (RenderMode == RenderType_Smooth) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (RenderMode == RenderType_Lines) {
        glLineWidth(1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    if (RenderMode == RenderType_Points) {
        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    }
    if (RenderMode == RenderType_Mesh) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
        // const static float black[4] = {0, 0, 0, 0}, white[4] = {1, 1, 1, 1};
        // glUniform4fv(diffusecol, 1, black);
        // glUniform4fv(specularcol, 1, white);

        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);

        glLineWidth(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // All render modes:
    glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
}

void Scene::DrawPoints(int first, int count, const float color[]) {
    glUniform4fv(diffusecol, 1, color);
    glUniform4fv(specularcol, 1, color);

    // Draw the excite vertex as a single point
    glPointSize(8.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    glDrawArrays(GL_POINTS, first, count);
}

void Scene::DrawPoint(int vertex_index, const float color[]) {
    DrawPoints(vertex_index, 1, color);
}

void Scene::RestoreDefaultMaterial() {
    glUniform4fv(diffusecol, 1, Diffusion);
    glUniform4fv(specularcol, 1, Specular);
}

void Scene::Render() {
    // Render the mesh to an OpenGl texture and display it, without changing the cursor position.
    const auto &content_region = GetContentRegionAvail();
    const auto &cursor = GetCursorPos();
    unsigned int texture_id = gl_canvas.Render();
    Image((void *)(intptr_t)texture_id, content_region, {0, 1}, {1, 0});
    SetCursorPos(cursor);

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
    if (ShowGizmo) {
        ImGuizmo::Manipulate(
            &CameraView[0][0], &CameraProjection[0][0], GizmoOp, ImGuizmo::LOCAL, &ObjectMatrix[0][0], nullptr,
            nullptr, ShowBounds ? Bounds : nullptr, nullptr
        );
    }
    if (ShowCameraGizmo) {
        static const float view_manipulate_size = 128;
        const auto viewManipulatePos = window_pos +
            ImVec2{
                GetWindowContentRegionMax().x - view_manipulate_size,
                GetWindowContentRegionMin().y,
            };
        ImGuizmo::ViewManipulate(&CameraView[0][0], CameraDistance, viewManipulatePos, {view_manipulate_size, view_manipulate_size}, 0);
    }
}

void Scene::RenderConfig() {
    if (BeginTabBar("SceneConfig")) {
        if (BeginTabItem("Geometries")) {
            SeparatorText("Render mode");
            RadioButton("Smooth", &RenderMode, RenderType_Smooth);
            SameLine();
            RadioButton("Lines", &RenderMode, RenderType_Lines);
            RadioButton("Point cloud", &RenderMode, RenderType_Points);
            SameLine();
            RadioButton("Mesh", &RenderMode, RenderType_Mesh);
            NewLine();
            SeparatorText("Gizmo");
            Checkbox("Show gizmo", &ShowGizmo);
            if (ShowGizmo) {
                const string interaction_text = "Interaction: " +
                    string(ImGuizmo::IsUsing() ? "Using Gizmo" : ImGuizmo::IsOver(ImGuizmo::TRANSLATE) ? "Translate hovered" :
                               ImGuizmo::IsOver(ImGuizmo::ROTATE)                                      ? "Rotate hovered" :
                               ImGuizmo::IsOver(ImGuizmo::SCALE)                                       ? "Scale hovered" :
                               ImGuizmo::IsOver()                                                      ? "Hovered" :
                                                                                                         "Not interacting");
                TextUnformatted(interaction_text.c_str());

                if (IsKeyPressed(ImGuiKey_T)) GizmoOp = ImGuizmo::TRANSLATE;
                if (IsKeyPressed(ImGuiKey_R)) GizmoOp = ImGuizmo::ROTATE;
                if (IsKeyPressed(ImGuiKey_S)) GizmoOp = ImGuizmo::SCALE;
                if (RadioButton("Translate (T)", GizmoOp == ImGuizmo::TRANSLATE)) GizmoOp = ImGuizmo::TRANSLATE;
                if (RadioButton("Rotate (R)", GizmoOp == ImGuizmo::ROTATE)) GizmoOp = ImGuizmo::ROTATE;
                if (RadioButton("Scale (S)", GizmoOp == ImGuizmo::SCALE)) GizmoOp = ImGuizmo::SCALE;
                if (RadioButton("Universal", GizmoOp == ImGuizmo::UNIVERSAL)) GizmoOp = ImGuizmo::UNIVERSAL;
                Checkbox("Bound sizing", &ShowBounds);
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
                ColorEdit3("Ambient", &Ambient[0]);
                ColorEdit3("Diffusion", &Diffusion[0]);
                ColorEdit3("Specular", &Specular[0]);
                SliderFloat("Shininess", &Shininess, 0.0f, 150.0f);
            } else {
                for (int i = 1; i < 3; i++) {
                    Ambient[i] = Ambient[0];
                    Diffusion[i] = Diffusion[0];
                    Specular[i] = Specular[0];
                }
                SliderFloat("Ambient", &Ambient[0], 0.0f, 1.0f);
                SliderFloat("Diffusion", &Diffusion[0], 0.0f, 1.0f);
                SliderFloat("Specular", &Specular[0], 0.0f, 1.0f);
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
