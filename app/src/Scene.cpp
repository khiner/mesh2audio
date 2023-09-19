#include "Scene.h"

#include "Shader.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;

using namespace ImGui;

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

void Scene::SetupDraw() {
    glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &CameraProjection[0][0]);
    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(CameraView * ObjectMatrix)[0][0]);

    glUniform4fv(lightpos, NumLights, LightPositions);
    glUniform4fv(lightcol, NumLights, LightColors);

    glUniform4fv(ambientcol, 1, Ambient);
    glUniform4fv(diffusecol, 1, Diffusion);
    glUniform4fv(specularcol, 1, Specular);
    glUniform1f(shininesscol, Shininess);
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

void Scene::RenderConfig() {
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
