#include "Mesh.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "Shader.h"

static const vec3 center = {0.f, 0.f, 0.f}, up = {0.f, 1.f, 0.f};

static bool StaticInitialized = false;
static GLuint projectionPos, modelviewPos;
// Variables to set uniform params for lighting fragment shader
static GLuint lightcol, lightpos, ambientcol, diffusecol, specularcol, emissioncol, shininesscol;

void Mesh::InitializeStatic() {
    if (StaticInitialized) return;

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
    CameraView = glm::lookAt(eye * CameraDistance, center, up);

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

    StaticInitialized = true;
}

Mesh::Mesh(fs::path file_path) {
    InitializeStatic();

    glGenVertexArrays(1, &VertexArray);
    glGenBuffers(1, &VertexBuffer);
    glGenBuffers(1, &NormalBuffer);
    glGenBuffers(1, &IndexBuffer);

    const bool is_svg = file_path.extension() == ".svg";
    const bool is_obj = file_path.extension() == ".obj";
    if (!is_svg && !is_obj) throw std::runtime_error("Unsupported file type: " + file_path.string());

    if (is_svg) {
        Profile = std::make_unique<MeshProfile>(file_path);
        ExtrudeProfile();
        // SVG coordinates are upside-down relative to our 3D rendering coordinates.
        // However, they're correctly oriented top-to-bottom for 2D ImGui rendering, so we only invert the 3D mesh - not the profile.
        InvertY();

        return;
    }

    FILE *fp;
    fp = fopen(file_path.c_str(), "rb");
    if (fp == nullptr) throw std::runtime_error("Error loading file: " + file_path.string());

    float x, y, z;
    int fx, fy, fz, ignore;
    int c1, c2;
    float y_min = INFINITY, z_min = INFINITY;
    float y_max = -INFINITY, z_max = -INFINITY;

    while (!feof(fp)) {
        c1 = fgetc(fp);
        while (!(c1 == 'v' || c1 == 'f')) {
            c1 = fgetc(fp);
            if (feof(fp)) break;
        }
        c2 = fgetc(fp);
        if ((c1 == 'v') && (c2 == ' ')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            Vertices.push_back({x, y, z});
            if (y < y_min) y_min = y;
            if (z < z_min) z_min = z;
            if (y > y_max) y_max = y;
            if (z > z_max) z_max = z;
        } else if ((c1 == 'v') && (c2 == 'n')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            Normals.push_back(glm::normalize(vec3(x, y, z)));
        } else if (c1 == 'f') {
            fscanf(fp, "%d//%d %d//%d %d//%d", &fx, &ignore, &fy, &ignore, &fz, &ignore);
            Indices.push_back(fx - 1);
            Indices.push_back(fy - 1);
            Indices.push_back(fz - 1);
        }
    }
    fclose(fp);

    const float y_avg = (y_min + y_max) / 2.0f - 0.02f;
    const float z_avg = (z_min + z_max) / 2.0f;
    for (unsigned int i = 0; i < Vertices.size(); ++i) {
        Vertices[i] -= vec3(0.0f, y_avg, z_avg);
        Vertices[i] *= vec3(1.58f, 1.58f, 1.58f);
    }
}

Mesh::~Mesh() {
    glDeleteBuffers(1, &IndexBuffer);
    glDeleteBuffers(1, &NormalBuffer);
    glDeleteBuffers(1, &VertexBuffer);
    glDeleteVertexArrays(1, &VertexArray);
}

void Mesh::Bind() const {
    glBindVertexArray(VertexArray);

    // Bind vertices to layout location 0
    glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * Vertices.size(), &Vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // This allows usage of layout location 0 in the vertex shader
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    // Bind normals to layout location 1
    glBindBuffer(GL_ARRAY_BUFFER, NormalBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * Normals.size(), &Normals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(1); // This allows usage of layout location 1 in the vertex shader
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * Indices.size(), &Indices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Mesh::InvertY() {
    float min_y = INFINITY, max_y = -INFINITY;
    for (auto &v : Vertices) {
        if (v.y < min_y) min_y = v.y;
        if (v.y > max_y) max_y = v.y;
    }
    for (auto &v : Vertices) v.y = max_y - (v.y - min_y);
}

void Mesh::ExtrudeProfile(int num_radial_slices) {
    if (Profile == nullptr) return;

    const vector<ImVec2> profile_vertices = Profile->CreateVertices(0.0001);
    const double angle_increment = 2.0 * M_PI / num_radial_slices;
    for (int i = 0; i < num_radial_slices; i++) {
        const double angle = i * angle_increment;
        for (const auto &p : profile_vertices) {
            // Compute the x and y coordinates for this point on the extruded surface.
            const double x = p.x * cos(angle);
            const double y = p.y; // Use the original z-coordinate from the 2D path
            const double z = p.x * sin(angle);
            Vertices.push_back({x, y, z});

            // Compute the normal for this vertex.
            const double nx = cos(angle);
            const double nz = sin(angle);
            Normals.push_back({nx, 0.0, nz});
        }
    }

    // Compute indices for the triangles.
    for (int i = 0; i < num_radial_slices; i++) {
        for (int j = 0; j < int(profile_vertices.size() - 1); j++) {
            const int base_index = i * profile_vertices.size() + j;
            const int next_base_index = ((i + 1) % num_radial_slices) * profile_vertices.size() + j;

            // First triangle
            Indices.push_back(base_index);
            Indices.push_back(next_base_index + 1);
            Indices.push_back(base_index + 1);

            // Second triangle
            Indices.push_back(base_index);
            Indices.push_back(next_base_index);
            Indices.push_back(next_base_index + 1);
        }
    }
}

void Mesh::RenderProfile() const {
    if (Profile != nullptr && Profile->NumControlPoints() > 0) Profile->Render();
    else ImGui::Text("The current mesh was not loaded from a 2D profile.");
}

void Mesh::SetCameraDistance(float distance) {
    // Extract the eye position from inverse camera view matrix and update the camera view based on the new distance.
    const vec3 eye = glm::inverse(CameraView)[3];
    CameraView = glm::lookAt(eye * (distance / CameraDistance), center, up);
    CameraDistance = distance;
}
void Mesh::UpdateCameraProjection(const ImVec2 &size) {
    CameraProjection = glm::perspective(glm::radians(fov * 2), size.x / size.y, 0.1f, 100.f);
}

void Mesh::Render(int mode) const {
    glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &CameraProjection[0][0]);
    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(CameraView * ObjectMatrix)[0][0]);

    glUniform4fv(lightpos, NumLights, LightPositions);
    glUniform4fv(lightcol, NumLights, LightColors);

    glUniform4fv(ambientcol, 1, Ambient);
    glUniform4fv(diffusecol, 1, Diffusion);
    glUniform4fv(specularcol, 1, Specular);
    glUniform1f(shininesscol, Shininess);

    glBindVertexArray(VertexArray);
    if (mode == 0) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
    }
    if (mode == 1) {
        glLineWidth(1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
    }
    if (mode == 2) {
        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
    }
    if (mode == 3) {
        const static GLfloat black[4] = {0, 0, 0, 0}, white[4] = {1, 1, 1, 1};

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
        glUniform4fv(diffusecol, 1, black);
        glUniform4fv(specularcol, 1, white);

        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);

        glLineWidth(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, NumIndices(), GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}
