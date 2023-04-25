#include "Mesh.h"

#include <fmt/chrono.h>
#include <iomanip>
#include <thread>

// mesh2faust/vega
#include "mesh2faust.h"
#include "tetMesher.h"

#include "imspinner.h"

#include <cinolib/meshes/meshes.h>
#include <cinolib/tetgen_wrap.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Audio.h"
#include "GlCanvas.h"
#include "Shader.h"

// auto start = std::chrono::high_resolution_clock::now();
// auto end = std::chrono::high_resolution_clock::now();
// auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
// std::cout << fmt::format("{}", duration) << std::endl;

using std::string, std::to_string;

// Alias for epoch seconds.
using seconds_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

static GlCanvas gl_canvas;

static std::thread GeneratorThread; // Worker thread for generating tetrahedral meshes.

static const mat4 Identity(1.f);
static const vec3 Origin{0.f}, Up{0.f, 1.f, 0.f};

static bool StaticInitialized = false;
static GLuint projectionPos, modelviewPos;
// Variables to set uniform params for lighting fragment shader
static GLuint lightcol, lightpos, ambientcol, diffusecol, specularcol, emissioncol, shininesscol;

static int HoveredVertexIndex = -1, CameraTargetVertexIndex = -1;
static vector<int> ExcitableVertexIndices;

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

    StaticInitialized = true;
}

Mesh::Mesh(fs::path file_path) {
    InitializeStatic();

    const bool is_svg = file_path.extension() == ".svg";
    const bool is_obj = file_path.extension() == ".obj";
    if (!is_svg && !is_obj) throw std::runtime_error("Unsupported file type: " + file_path.string());

    FilePath = file_path; // Store the most recent file path.
    if (is_svg) {
        Profile = std::make_unique<MeshProfile>(FilePath);
        ExtrudeProfile();
        return;
    }

    FILE *fp;
    fp = fopen(FilePath.c_str(), "rb");
    if (fp == nullptr) throw std::runtime_error("Error loading file: " + FilePath.string());

    float x, y, z;
    int fx, fy, fz, ignore;
    int c1, c2;
    while (!feof(fp)) {
        c1 = fgetc(fp);
        while (!(c1 == 'v' || c1 == 'f')) {
            c1 = fgetc(fp);
            if (feof(fp)) break;
        }
        c2 = fgetc(fp);
        if ((c1 == 'v') && (c2 == ' ')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            TriangularMesh.Vertices.push_back({x, y, z});
        } else if ((c1 == 'v') && (c2 == 'n')) {
            fscanf(fp, "%f %f %f", &x, &y, &z);
            TriangularMesh.Normals.push_back(glm::normalize(vec3(x, y, z)));
        } else if (c1 == 'f') {
            fscanf(fp, "%d//%d %d//%d %d//%d", &fx, &ignore, &fy, &ignore, &fz, &ignore);
            TriangularMesh.Indices.push_back(fx - 1);
            TriangularMesh.Indices.push_back(fy - 1);
            TriangularMesh.Indices.push_back(fz - 1);
        }
    }
    fclose(fp);

    TriangularMesh.UpdateBounds();
    TriangularMesh.Center();
    Bind();
}

Mesh::~Mesh() {
    if (GeneratorThread.joinable()) GeneratorThread.join();
}

const MeshInstance &Mesh::GetActiveInstance() const {
    if (ActiveViewMeshType == MeshType_Triangular) return TriangularMesh;
    return TetMesh;
}
MeshInstance &Mesh::GetActiveInstance() {
    if (ActiveViewMeshType == MeshType_Triangular) return TriangularMesh;
    return TetMesh;
}

void Mesh::Save(fs::path file_path) const {
    GetActiveInstance().Save(file_path);
}

void Mesh::Flip(bool x, bool y, bool z) {
    TriangularMesh.Flip(x, y, z);
    TetMesh.Flip(x, y, z);
    Bind();
}
void Mesh::Rotate(const vec3 &axis, float angle) {
    TriangularMesh.Rotate(axis, angle);
    TetMesh.Rotate(axis, angle);
    Bind();
}
void Mesh::Scale(const vec3 &scale) {
    TriangularMesh.Scale(scale);
    TetMesh.Scale(scale);
    Bind();
}
void Mesh::Center() {
    TriangularMesh.Center();
    TetMesh.Center();
    Bind();
}

void Mesh::ExtrudeProfile() {
    if (Profile == nullptr) return;

    HoveredVertexIndex = CameraTargetVertexIndex = -1;
    TetMesh.Clear();
    TetMeshPath.clear();
    ExcitableVertexIndices.clear();
    TriangularMesh.ExtrudeProfile(Profile->GetVertices(), Profile->NumRadialSlices, Profile->ClosePath);
    ActiveViewMeshType = MeshType_Triangular;
    Bind();
}

void Mesh::SetCameraDistance(float distance) {
    // Extract the eye position from inverse camera view matrix and update the camera view based on the new distance.
    const vec3 eye = glm::inverse(CameraView)[3];
    CameraView = glm::lookAt(eye * (distance / CameraDistance), Origin, Up);
    CameraDistance = distance;
}
void Mesh::UpdateCameraProjection(const ImVec2 &size) {
    CameraProjection = glm::perspective(glm::radians(fov * 2), size.x / size.y, 0.1f, 100.f);
}

void Mesh::Bind() {
    GetActiveInstance().Bind();
}

static void InterpolateColors(GLfloat a[], GLfloat b[], float interpolation, GLfloat result[]) {
    for (int i = 0; i < 4; i++) {
        result[i] = a[i] * (1.0 - interpolation) + b[i] * interpolation;
    }
}
static void SetColor(GLfloat to[], GLfloat result[]) {
    for (int i = 0; i < 4; i++) result[i] = to[i];
}

void Mesh::DrawGl() const {
    glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &CameraProjection[0][0]);
    glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(CameraView * ObjectMatrix)[0][0]);

    glUniform4fv(lightpos, NumLights, LightPositions);
    glUniform4fv(lightcol, NumLights, LightColors);

    glUniform4fv(ambientcol, 1, Ambient);
    glUniform4fv(diffusecol, 1, Diffusion);
    glUniform4fv(specularcol, 1, Specular);
    glUniform1f(shininesscol, Shininess);

    const auto &instance = GetActiveInstance();
    const int num_indices = instance.Indices.size();
    glBindVertexArray(instance.VertexArray);
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
        const static GLfloat black[4] = {0, 0, 0, 0}, white[4] = {1, 1, 1, 1};

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);
        glUniform4fv(diffusecol, 1, black);
        glUniform4fv(specularcol, 1, white);

        glPointSize(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);

        glLineWidth(2.5);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // All render modes:
    glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_INT, 0);

    if (ViewMeshType == MeshType_Tetrahedral && !Audio::FaustState::Is2DModel && ShowExcitableVertices && !ExcitableVertexIndices.empty()) {
        for (size_t i = 0; i < ExcitableVertexIndices.size(); i++) {
            static GLfloat DisabledExcitableVertexColor[4] = {0.3, 0.3, 0.3, 1}; // For when DSP has not been initialized.
            static GLfloat ExcitableVertexColor[4] = {1, 1, 1, 1}; // Based on `NumExcitableVertices`.
            static GLfloat ActiveExciteVertexColor[4] = {0, 1, 0, 1}; // The most recent excited vertex.
            static GLfloat ExcitedVertexBaseColor[4] = {1, 0, 0, 1}; // The color of the excited vertex when the gate has abs value of 1.

            static GLfloat vertex_color[4] = {1, 1, 1, 1}; // Initialized once and filled for every excitable vertex.
            if (!Audio::FaustState::IsRunning()) {
                SetColor(DisabledExcitableVertexColor, vertex_color);
            } else if (Audio::FaustState::ExcitePos != nullptr && int(i) == int(*Audio::FaustState::ExcitePos)) {
                InterpolateColors(ActiveExciteVertexColor, ExcitedVertexBaseColor, std::min(1.f, std::abs(*Audio::FaustState::ExciteValue)), vertex_color);
            } else {
                SetColor(ExcitableVertexColor, vertex_color);
            }

            glUniform4fv(diffusecol, 1, vertex_color);
            glUniform4fv(specularcol, 1, vertex_color);

            // Draw the excite vertex as a single point
            glPointSize(8.0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            glDrawArrays(GL_POINTS, ExcitableVertexIndices[i], 1);
        }
        // Restore the original material properties
        glUniform4fv(diffusecol, 1, Diffusion);
        glUniform4fv(specularcol, 1, Specular);
    }

    glBindVertexArray(0);
}

static string FormatTime(seconds_t seconds) {
    return fmt::format("{:%m-%d %H:%M:%S %Z}", seconds);
}

static seconds_t GetTimeFromPath(const fs::path &file_path) {
    return seconds_t{std::chrono::seconds(std::stoi(file_path.stem()))};
}

string Mesh::GetTetMeshName(fs::path file_path) {
    return FormatTime(GetTimeFromPath(file_path));
}

static const fs::path TempDir = "tmp";
static const fs::path TetSaveDir = "saved_tet_meshes";
static const int MaxSavedTetMeshes = 8;

static vector<seconds_t> GetSavedTetMeshTimes() {
    vector<seconds_t> saved_tet_mesh_times;
    for (const auto &entry : fs::directory_iterator(TetSaveDir)) {
        saved_tet_mesh_times.push_back(GetTimeFromPath(entry.path()));
    }
    // Sort by most recent first.
    std::sort(saved_tet_mesh_times.begin(), saved_tet_mesh_times.end(), std::greater<>());
    return saved_tet_mesh_times;
}

void Mesh::UpdateExcitableVertices() {
    // Linearly sample excitable vertices from all available vertices.
    ExcitableVertexIndices.resize(NumExcitableVertices);
    for (int i = 0; i < NumExcitableVertices; i++) {
        const float t = float(i) / (NumExcitableVertices - 1);
        ExcitableVertexIndices[i] = int(t * (TetMesh.Vertices.size() - 1));
    }
}

void Mesh::LoadTetMesh(const vector<cinolib::vec3d> &tet_vecs, const vector<vector<uint>> &tet_polys) {
    HoveredVertexIndex = CameraTargetVertexIndex = -1;
    TetMesh.Clear();
    TetMesh.Vertices.reserve(tet_vecs.size());
    TetMesh.Normals.reserve(tet_vecs.size());
    TetMesh.Indices.reserve(tet_polys.size() * 12);

    // Bind vertices, normals, and indices to the tetrahedral mesh.
    for (size_t i = 0; i < tet_vecs.size(); i++) {
        const auto &v = tet_vecs[i];
        const float x = v[0], y = v[1], z = v[2];
        const float angle = atan2(z, x);
        TetMesh.Vertices.push_back({x, y, z});
        TetMesh.Normals.push_back({cos(angle), 0, sin(angle)});
        // const auto &normal = tet_mesh.face_data(fid).normal;
        // Normals.push_back({normal.x(), normal.y(), normal.z()});
    }
    for (const auto &p : tet_polys) {
        // Turn tetrahedron into 4 triangles with element-relative indices:
        // 0, 1, 2; 3, 0, 1; 2, 3, 0; 1, 2, 3;
        for (int j = 0; j < 12; j++) {
            TetMesh.Indices.push_back(p[j % 4]);
        }
    }
    // Creating a `cinolib::Tetmesh` resolves redundant faces, which creates much fewer indices.
    // However, it's very slow.
    // const cinolib::Tetmesh tet_mesh{tet_vecs, tet_polys};
    // for (uint fid = 0; fid < tet_mesh.num_faces(); ++fid) {
    //     const auto &tes = tet_mesh.face_tessellation(fid);
    //     for (uint i = 0; i < tes.size(); ++i) {
    //         TetMesh.Indices.push_back(tes.at(i));
    //     }
    // }

    TetMesh.UpdateBounds();
    TetMesh.Center();
    UpdateExcitableVertices();

    ViewMeshType = MeshType_Tetrahedral; // Automatically switch to tetrahedral view.
    ActiveViewMeshType = MeshType_Triangular; // Force an automatic rebind on the next render (xxx crappy way of doing this).
}

void Mesh::LoadTetMesh(fs::path file_path) {
    vector<cinolib::vec3d> tet_vecs;
    vector<vector<uint>> tet_polys;
    read_MESH(file_path.c_str(), tet_vecs, tet_polys);
    LoadTetMesh(tet_vecs, tet_polys);
    TetMeshPath = file_path;
}

void Mesh::GenerateTetMesh() {
    static vector<cinolib::vec3d> tet_vecs;
    static vector<uint> tet_indices;

    // Write to an obj file and read back into a cinolib tetmesh.
    fs::create_directory(TempDir); // Create the temp dir if it doesn't exist.
    const fs::path tmp_obj_path = TempDir / "tmp.obj";
    TriangularMesh.Save(tmp_obj_path);
    cinolib::Polygonmesh<> poly_mesh(tmp_obj_path.c_str());
    vector<uint> edges_in; // Not used.
    tetgen_wrap(poly_mesh.vector_verts(), poly_mesh.vector_polys(), edges_in, QualityTetMesh ? "q" : "", tet_vecs, tet_indices);
    fs::remove(tmp_obj_path); // Delete the temporary file.

    // Write the tet mesh to disk, deleting the oldest tet mesh(es) if this puts us past the max save limit.
    fs::create_directory(TetSaveDir);
    const auto &saved_tet_mesh_times = GetSavedTetMeshTimes();
    for (int delete_i = saved_tet_mesh_times.size() - 1; delete_i >= MaxSavedTetMeshes - 1; delete_i--) {
        const seconds_t oldest_time = saved_tet_mesh_times.at(delete_i);
        fs::remove(TetSaveDir / (to_string(oldest_time.time_since_epoch().count()) + ".mesh"));
    }
    const int epoch_seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const fs::path file_path = TetSaveDir / (to_string(epoch_seconds) + ".mesh");
    const auto &tet_polys = cinolib::polys_from_serialized_vids(tet_indices, 4);
    write_MESH(file_path.c_str(), tet_vecs, tet_polys);

    LoadTetMesh(tet_vecs, tet_polys);
    TetMeshPath = file_path;
}

string Mesh::GenerateDsp() const {
    if (!HasTetMesh()) return ""; // Tet mesh must be generated first.

    // This is a copy of `tet_vecs`, but using raw doubles, since there is no matching `tetgen_wrap` method.
    static vector<double> tet_vertices;
    const auto &tet_vecs = TetMesh.Vertices;
    tet_vertices.resize(tet_vecs.size() * 3);
    for (size_t i = 0; i < tet_vecs.size(); i++) {
        tet_vertices[i * 3 + 0] = tet_vecs[i].x;
        tet_vertices[i * 3 + 1] = tet_vecs[i].y;
        tet_vertices[i * 3 + 2] = tet_vecs[i].z;
    }

    // Convert the tetrahedral mesh into a VegaFEM tetmesh.
    // We do this as a one-off every time, so that this is the only method that needs to be aware of VegaFEM types.
    ::TetMesh volumetric_mesh{
        int(tet_vecs.size()), tet_vertices.data(), int(TetMesh.Indices.size() / 4), (int *)TetMesh.Indices.data(),
        Material.YoungModulus, Material.PoissonRatio, Material.Density};

    m2f::CommonArguments args{
        "modalModel",
        true, // freq control activated
        20, // lowest mode freq
        10000, // highest mode freq
        20, // number of synthesized modes (default is 20)
        50, // number of modes to be computed for the finite element analysis (default is 100)
        ExcitableVertexIndices, // specific excitation positions
        int(ExcitableVertexIndices.size()) // number of excitation positions (default is max: -1)
    };
    return m2f::mesh2faust(&volumetric_mesh, args);
}

static constexpr float VertexHoverRadius = 5.f;
static constexpr float VertexHoverRadiusSquared = VertexHoverRadius * VertexHoverRadius;

using namespace ImGui;

void Mesh::Render() {
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
    if (ViewMeshType == MeshType_Tetrahedral && !HasTetMesh()) {
        ViewMeshType = MeshType_Triangular;
    }
    if (ActiveViewMeshType != ViewMeshType) {
        ActiveViewMeshType = ViewMeshType;
        Bind();
    }
    DrawGl();

    // Render the mesh to an OpenGl texture and display it.
    unsigned int texture_id = gl_canvas.Render();
    Image((void *)(intptr_t)texture_id, content_region, {0, 1}, {1, 0});

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

    const auto &instance = GetActiveInstance();
    const auto &vertices = instance.Vertices;

    // Find the hovered vertex, favoring the one nearest to the camera if multiple are hovered.
    HoveredVertexIndex = -1;
    const vec3 camera_position = glm::inverse(CameraView)[3];
    if (window_hovered) {
        // Transform each vertex position to screen space and check if the mouse is hovering over it.
        const auto &mouse_pos = io.MousePos;
        const auto &content_pos = window_pos + GetWindowContentRegionMin();
        const auto &view_projection = CameraProjection * CameraView;

        float min_vertex_camera_distance = FLT_MAX;
        for (size_t i = 0; i < vertices.size(); i++) {
            const auto &v = vertices[i];
            const vec4 pos_clip_space = view_projection * vec4{v.x, v.y, v.z, 1.0f};
            const vec4 tmp = (pos_clip_space / pos_clip_space.w) * 0.5f + 0.5f;
            const auto vertex_screen = ImVec2{tmp.x, 1.0f - tmp.y} * content_region + content_pos;
            const float screen_dist_squared = pow(mouse_pos.x - vertex_screen.x, 2) + pow(mouse_pos.y - vertex_screen.y, 2);
            if (screen_dist_squared <= VertexHoverRadiusSquared) {
                const float vertex_camera_distance = glm::distance(camera_position, v);
                if (vertex_camera_distance < min_vertex_camera_distance) {
                    min_vertex_camera_distance = vertex_camera_distance;
                    HoveredVertexIndex = i;
                }
            }
        }
    }

    // Visualize the hovered index.
    if (HoveredVertexIndex >= 0) {
        const auto &content_pos = window_pos + GetWindowContentRegionMin();
        const auto &view_projection = CameraProjection * CameraView;
        auto *dl = GetWindowDrawList();
        const auto &hovered_vertex = vertices[HoveredVertexIndex];
        const vec4 pos_clip_space = view_projection * vec4{hovered_vertex.x, hovered_vertex.y, hovered_vertex.z, 1.0f};
        const vec4 tmp = (pos_clip_space / pos_clip_space.w) * 0.5f + 0.5f;
        const auto vertex_screen = ImVec2{tmp.x, 1.0f - tmp.y} * content_region + content_pos;

        // Adjust the circle radius based on the distance
        static const float base_radius = 1.0f;
        static const float distance_scale_factor = 2.f;
        const float vertex_camera_distance = glm::distance(camera_position, hovered_vertex);
        const float scaled_radius = base_radius + (1.f / vertex_camera_distance) * distance_scale_factor;
        dl->AddCircleFilled(vertex_screen, scaled_radius, IM_COL32(255, 0, 0, 255));
    }

    // Handle mouse interactions.
    const bool mouse_clicked = IsMouseClicked(0), mouse_released = IsMouseReleased(0);
    if (window_hovered) {
        if (mouse_clicked) CameraTargetVertexIndex = -1;
        else if (mouse_released) CameraTargetVertexIndex = HoveredVertexIndex;

        // On click, trigger the nearest excitation vertex nearest to the clicked vertex.
        if (mouse_clicked && HoveredVertexIndex >= 0 && !ExcitableVertexIndices.empty() && Audio::FaustState::IsRunning() && !Audio::FaustState::Is2DModel) {
            const auto &hovered_vertex = vertices[HoveredVertexIndex];
            int nearest_excite_vertex_pos = -1; // Position in the excitation vertex indices.
            float min_dist = FLT_MAX;
            for (size_t i = 0; i < ExcitableVertexIndices.size(); i++) {
                const auto &excite_vertex_index = ExcitableVertexIndices[i];
                const auto &excite_vertex = vertices[excite_vertex_index];
                const float dist = glm::distance(excite_vertex, hovered_vertex);
                if (dist < min_dist) {
                    min_dist = dist;
                    nearest_excite_vertex_pos = i;
                }
            }
            if (nearest_excite_vertex_pos >= 0) { // Shouldn't ever be false, but sanity check.
                *Audio::FaustState::ExcitePos = nearest_excite_vertex_pos;
                *Audio::FaustState::ExciteValue = 1.f;
            }
        }
    }

    // Any time mouse is released, release the excitation trigger
    // (even if mouse was pressed, dragged outside the window, and released).
    if (mouse_released && Audio::FaustState::IsRunning()) *Audio::FaustState::ExciteValue = 0.f;

    // Rotate the camera to the targeted vertex.
    if (CameraTargetVertexIndex >= 0) {
        static const float CameraMovementSpeed = 0.5;

        const auto &target_vertex = vertices[CameraTargetVertexIndex];
        const vec3 target_direction = glm::normalize(target_vertex - Origin);
        // Calculate the direction from the origin to the camera position
        const vec3 current_direction = glm::normalize(camera_position - Origin);
        // Create quaternions representing the two directions
        const glm::quat current_quat = glm::quatLookAt(current_direction, Up);
        const glm::quat target_quat = glm::quatLookAt(target_direction, Up);
        // Interpolate linearly between the two quaternions along the sphere defined by the camera distance.
        const float lerp_factor = glm::clamp(CameraMovementSpeed / CameraDistance, 0.0f, 1.0f);
        const glm::quat new_quat = glm::slerp(current_quat, target_quat, lerp_factor);
        const vec3 new_camera_direction = new_quat * glm::vec3(0.0f, 0.0f, -1.0f);
        CameraView = glm::lookAt(Origin + new_camera_direction * CameraDistance, Origin, Up);
    }
}

static const string GenerateTetMsg = "Generating tetrahedral mesh...";

void Mesh::RenderConfig() {
    if (BeginTabBar("MeshConfigTabBar")) {
        if (BeginTabItem("Mesh")) {
            const auto &center = GetMainViewport()->GetCenter();
            const auto &popup_size = GetMainViewport()->Size / 4;
            SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
            SetNextWindowSize(popup_size);
            if (BeginPopupModal(GenerateTetMsg.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                const auto &ws = GetWindowSize();
                const float spinner_dim = std::min(ws.x, ws.y) / 2;
                SetCursorPos((ws - ImVec2{spinner_dim, spinner_dim}) / 2 + ImVec2(0, GetTextLineHeight()));
                ImSpinner::SpinnerMultiFadeDots(GenerateTetMsg.c_str(), spinner_dim / 2, 3);
                if (HasTetMesh()) {
                    if (GeneratorThread.joinable()) GeneratorThread.join();
                    CloseCurrentPopup();
                }
                EndPopup();
            }

            SeparatorText("Create");
            Checkbox("Quality mode", &QualityTetMesh);
            const string generate_mesh_label = HasTetMesh() ? "Regenerate tetrahedral mesh" : "Generate tetrahedral mesh";
            if (Button(generate_mesh_label.c_str())) {
                OpenPopup(GenerateTetMsg.c_str());
                if (GeneratorThread.joinable()) GeneratorThread.join();
                GeneratorThread = std::thread([&] { GenerateTetMesh(); });
            }
            // Dropdown to select from saved tet meshes and load.
            if (BeginCombo("Saved tet meshes", nullptr)) {
                const auto &saved_tet_mesh_times = GetSavedTetMeshTimes();
                if (saved_tet_mesh_times.empty()) TextUnformatted("No saved tet meshes");
                for (const seconds_t tet_mesh_time : saved_tet_mesh_times) {
                    const string formatted_time = FormatTime(tet_mesh_time);
                    if (Selectable(formatted_time.c_str(), false)) {
                        LoadTetMesh(TetSaveDir / (to_string(tet_mesh_time.time_since_epoch().count()) + ".mesh"));
                    }
                }
                EndCombo();
            }

            SeparatorText("Current mesh");
            Text("File: %s", FilePath.c_str());
            if (HasTetMesh()) {
                Checkbox("Show excitable vertices", &ShowExcitableVertices);
                if (SliderInt("Num. excitable vertices", &NumExcitableVertices, 1, std::min(200, int(TetMesh.Vertices.size())))) {
                    UpdateExcitableVertices();
                }

                const string name = GetTetMeshName(TetMeshPath);
                Text(
                    "Current tetrahedral mesh:\n\tGenerated: %s\n\tVertices: %lu\n\tIndices: %lu",
                    name.c_str(), TetMesh.Vertices.size(), TetMesh.Indices.size()
                );
                TextUnformatted("View mesh type");
                RadioButton("Triangular", &ViewMeshType, MeshType_Triangular);
                SameLine();
                RadioButton("Tetrahedral", &ViewMeshType, MeshType_Tetrahedral);
            } else {
                TextUnformatted("No tetrahedral mesh loaded");
            }

            SeparatorText("Modify");
            if (Button("Center")) Center();
            Text("Flip");
            SameLine();
            if (Button("X##Flip")) Flip(true, false, false);
            SameLine();
            if (Button("Y##Flip")) Flip(false, true, false);
            SameLine();
            if (Button("Z##Flip")) Flip(false, false, true);

            Text("Rotate 90 deg.");
            SameLine();
            if (Button("X##Rotate")) Rotate({1, 0, 0}, 90);
            SameLine();
            if (Button("Y##Rotate")) Rotate({0, 1, 0}, 90);
            SameLine();
            if (Button("Z##Rotate")) Rotate({0, 0, 1}, 90);

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

            SeparatorText("Debug");
            if (HoveredVertexIndex >= 0) {
                const auto &vertex = GetActiveInstance().Vertices[HoveredVertexIndex];
                Text("Hovered vertex:\n\tIndex: %d\n\tPosition:\n\t\tx: %f\n\t\ty: %f\n\t\tz: %f", HoveredVertexIndex, vertex.x, vertex.y, vertex.z);
            }

            EndTabItem();
        }
        if (BeginTabItem("Mesh profile")) {
            RenderProfileConfig();
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
            Checkbox("Custom colors", &CustomColor);
            if (CustomColor) {
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

void Mesh::RenderProfile() {
    if (Profile == nullptr) Text("The current mesh was not loaded from a 2D profile.");
    else if (Profile->Render()) ExtrudeProfile();
}

void Mesh::RenderProfileConfig() {
    if (Profile == nullptr) Text("The current mesh was not loaded from a 2D profile.");
    else if (Profile->RenderConfig()) ExtrudeProfile();
}
