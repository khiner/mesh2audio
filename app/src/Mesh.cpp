#include "Mesh.h"

#include <format>

// mesh2faust/vega
#include "mesh2faust.h"
#include "tetMesher.h"

#include "date.h"

#include <cinolib/meshes/meshes.h>
#include <cinolib/tetgen_wrap.h>

#include "Audio.h"

// auto start = std::chrono::high_resolution_clock::now();
// auto end = std::chrono::high_resolution_clock::now();
// auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
// std::cout << std::format("{}", duration) << std::endl;

using glm::vec2, glm::vec3, glm::vec4, glm::mat4;
using std::string, std::to_string;
using seconds_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>; // Alias for epoch seconds.

Mesh::Mesh(::Scene &scene, fs::path file_path) : Scene(scene) {
    const bool is_svg = file_path.extension() == ".svg";
    const bool is_obj = file_path.extension() == ".obj";
    if (!is_svg && !is_obj) throw std::runtime_error("Unsupported file type: " + file_path.string());

    FilePath = file_path; // Store the most recent file path.
    if (is_svg) {
        Profile = std::make_unique<MeshProfile>(FilePath);
        TriangularMesh.ExtrudeProfile(Profile->GetVertices(), Profile->NumRadialSlices, Profile->ClosePath);
    } else {
        TriangularMesh.Load(FilePath);
    }

    HoveredVertexArrow.SetColor({1, 0, 0, 1});
    UpdateExcitableVertices();
}

Mesh::~Mesh() {}

const Geometry &Mesh::GetActiveGeometry() const {
    if (ActiveViewMeshType == MeshType_Triangular) return TriangularMesh;
    return TetMesh;
}
Geometry &Mesh::GetActiveGeometry() {
    if (ActiveViewMeshType == MeshType_Triangular) return TriangularMesh;
    return TetMesh;
}

void Mesh::Save(fs::path file_path) const {
    GetActiveGeometry().Save(file_path);
}

mat4 Mesh::GetTransform() const {
    mat4 transform = Identity;
    transform = glm::translate(transform, Translation);

    mat4 rot_x = glm::rotate(Identity, glm::radians(RotationAngles.x), {1, 0, 0});
    mat4 rot_y = glm::rotate(Identity, glm::radians(RotationAngles.y), {0, 1, 0});
    mat4 rot_z = glm::rotate(Identity, glm::radians(RotationAngles.z), {0, 0, 1});
    transform *= (rot_z * rot_y * rot_x);

    return glm::scale(transform, Scale);
}

void Mesh::ApplyTransform() {
    const mat4 transform = GetTransform();
    TriangularMesh.SetTransform(transform);
    TetMesh.SetTransform(transform);
    Scene.GizmoTransform = transform;
}

void Mesh::ExtrudeProfile() {
    if (Profile == nullptr) return;

    HoveredVertexIndex = CameraTargetVertexIndex = -1;
    TetMesh.Clear();
    TetMeshPath.clear();
    UpdateExcitableVertices();
    TriangularMesh.ExtrudeProfile(Profile->GetVertices(), Profile->NumRadialSlices, Profile->ClosePath);
    ActiveViewMeshType = MeshType_Triangular;
}

static void InterpolateColors(const float a[], const float b[], float interpolation, float result[]) {
    for (int i = 0; i < 4; i++) {
        result[i] = a[i] * (1.0 - interpolation) + b[i] * interpolation;
    }
}
static void SetColor(const float to[], float result[]) {
    for (int i = 0; i < 4; i++) result[i] = to[i];
}

void Mesh::UpdateExcitableVertexColors() {
    if (ViewMeshType == MeshType_Tetrahedral && !Audio::FaustState::Is2DModel && ShowExcitableVertices && !ExcitableVertexIndices.empty()) {
        for (size_t i = 0; i < ExcitableVertexIndices.size(); i++) {
            // todo update to use vec4 for all colors.
            static const float DisabledExcitableVertexColor[4] = {0.3, 0.3, 0.3, 1}; // For when DSP has not been initialized.
            static const float ExcitableVertexColor[4] = {1, 1, 1, 1}; // Based on `NumExcitableVertices`.
            static const float ActiveExciteVertexColor[4] = {0, 1, 0, 1}; // The most recent excited vertex.
            static const float ExcitedVertexBaseColor[4] = {1, 0, 0, 1}; // The color of the excited vertex when the gate has abs value of 1.

            static float vertex_color[4] = {1, 1, 1, 1}; // Initialized once and filled for every excitable vertex.
            if (!Audio::FaustState::IsRunning()) {
                SetColor(DisabledExcitableVertexColor, vertex_color);
            } else if (Audio::FaustState::ExcitePos != nullptr && int(i) == int(*Audio::FaustState::ExcitePos)) {
                InterpolateColors(ActiveExciteVertexColor, ExcitedVertexBaseColor, std::min(1.f, std::abs(*Audio::FaustState::ExciteValue)), vertex_color);
            } else {
                SetColor(ExcitableVertexColor, vertex_color);
            }
            ExcitableVertexPoints.InstanceColors[i] = {vertex_color[0], vertex_color[1], vertex_color[2], vertex_color[3]};
        }
    }
}

void Mesh::UpdateExcitableVertices() {
    ExcitableVertexPoints.InstanceModels.clear();
    ExcitableVertexPoints.InstanceColors.clear();
    ExcitableVertexIndices.clear();
    if (TetMesh.Vertices.empty()) return;

    // Linearly sample excitable vertices from all available vertices.
    ExcitableVertexIndices.resize(NumExcitableVertices);
    for (int i = 0; i < NumExcitableVertices; i++) {
        const float t = float(i) / (NumExcitableVertices - 1);
        ExcitableVertexIndices[i] = int(t * (TetMesh.Vertices.size() - 1));
    }

    for (auto vertex_index : ExcitableVertexIndices) {
        ExcitableVertexPoints.InstanceModels.push_back(glm::translate(Identity, TetMesh.Vertices[vertex_index]));
        ExcitableVertexPoints.InstanceColors.push_back({1, 1, 1, 1});
    }
}

static string FormatTime(seconds_t seconds) { return date::format("{:%m-%d %H:%M:%S %Z}", seconds); }
static seconds_t GetTimeFromPath(const fs::path &file_path) { return seconds_t{std::chrono::seconds(std::stoi(file_path.stem()))}; }

string Mesh::GetTetMeshName(fs::path file_path) { return FormatTime(GetTimeFromPath(file_path)); }

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

    TetMesh.CenterVertices();
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
        40, // number of synthesized modes (default is 20)
        80, // number of modes to be computed for the finite element analysis (default is 100)
        ExcitableVertexIndices, // specific excitation positions
        int(ExcitableVertexIndices.size()), // number of excitation positions (default is max: -1)
    };
    return m2f::mesh2faust(&volumetric_mesh, args);
}

static constexpr float VertexHoverRadius = 5.f;
static constexpr float VertexHoverRadiusSquared = VertexHoverRadius * VertexHoverRadius;

using namespace ImGui;

void Mesh::UpdateHoveredVertex() {
    const auto &geometry = GetActiveGeometry();

    // Find the hovered vertex, favoring the one nearest to the camera if multiple are hovered.
    HoveredVertexIndex = -1;
    if (IsWindowHovered()) {
        const vec3 camera_position = glm::inverse(Scene.CameraView)[3];
        const auto content_region = GetContentRegionAvail();
        const auto &io = ImGui::GetIO();
        // Transform each vertex position to screen space and check if the mouse is hovering over it.
        const auto &mouse_pos = io.MousePos;
        const auto &content_pos = GetWindowPos() + GetWindowContentRegionMin();
        const auto &view_projection = Scene.CameraProjection * Scene.CameraView;
        float min_vertex_camera_distance = FLT_MAX;
        const auto &vertices = geometry.Vertices;
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

    HoveredVertexArrow.InstanceModels.clear();
    if (HoveredVertexIndex >= 0 && HoveredVertexIndex < int(geometry.Vertices.size())) {
        // Point the arrow at the hovered vertex.
        const auto &hovered_vertex = geometry.Vertices[HoveredVertexIndex];
        const auto &hovered_normal = geometry.Normals[HoveredVertexIndex];

        mat4 translate = glm::translate(Identity, hovered_vertex);
        mat4 rotate = glm::toMat4(glm::rotation(Up, glm::normalize(hovered_normal)));
        HoveredVertexArrow.InstanceModels.push_back({translate * rotate});
    }
}

void Mesh::Render() {
    if (ViewMeshType == MeshType_Tetrahedral && !HasTetMesh()) {
        ViewMeshType = MeshType_Triangular;
    }
    if (ActiveViewMeshType != ViewMeshType) {
        ActiveViewMeshType = ViewMeshType;
    }

    UpdateHoveredVertex();
    UpdateExcitableVertexColors();

    const auto &geometry = GetActiveGeometry();
    Scene.Draw(geometry);
    Scene.Draw(ExcitableVertexPoints);
    Scene.Draw(HoveredVertexArrow);
    if (RealImpact) Scene.Draw(RealImpactListenerPoints);
    Scene.Render();

    const auto &vertices = geometry.Vertices;

    // Handle mouse interactions.
    const bool mouse_clicked = IsMouseClicked(0), mouse_released = IsMouseReleased(0);
    if (!Scene.ShowGizmo && IsWindowHovered()) {
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

    // When mouse is released, release the excitation trigger
    // (even if mouse was pressed, dragged outside the window, and released).
    if (mouse_released && Audio::FaustState::IsRunning()) *Audio::FaustState::ExciteValue = 0.f;

    // Rotate the camera to the targeted vertex.
    if (CameraTargetVertexIndex >= 0) {
        static const float CameraMovementSpeed = 0.5;

        const vec3 camera_position = glm::inverse(Scene.CameraView)[3];
        const auto &target_vertex = vertices[CameraTargetVertexIndex];
        const vec3 target_direction = glm::normalize(target_vertex - Origin);
        // Calculate the direction from the origin to the camera position
        const vec3 current_direction = glm::normalize(camera_position - Origin);
        // Create quaternions representing the two directions
        const glm::quat current_quat = glm::quatLookAt(current_direction, Up);
        const glm::quat target_quat = glm::quatLookAt(target_direction, Up);
        // Interpolate linearly between the two quaternions along the sphere defined by the camera distance.
        const float lerp_factor = glm::clamp(CameraMovementSpeed / Scene.CameraDistance, 0.0f, 1.0f);
        const glm::quat new_quat = glm::slerp(current_quat, target_quat, lerp_factor);
        const vec3 new_camera_direction = new_quat * glm::vec3(0.0f, 0.0f, -1.0f);
        Scene.CameraView = glm::lookAt(Origin + new_camera_direction * Scene.CameraDistance, Origin, Up);
    }
}

void Mesh::RenderConfig() {
    if (BeginTabBar("MeshConfigTabBar")) {
        if (BeginTabItem("Mesh")) {
            const auto &center = GetMainViewport()->GetCenter();
            SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
            SetNextWindowSize(GetMainViewport()->Size / 4);
            if (TetGenerator.Render()) {
                // Tet generation completed.
            }
            SeparatorText("Create");

            const bool can_generate_tet_mesh = !MeshProfile::ClosePath;
            if (!can_generate_tet_mesh) {
                BeginDisabled();
                TextUnformatted("Disable |MeshProfile|->|ClosePath| to generate tet mesh.\n(Meshes with holes can only be used for axisymmetric simulations.)");
            }
            Checkbox("Quality mode", &QualityTetMesh);
            TetGenerator.RenderLauncher(HasTetMesh() ? "Regenerate tetrahedral mesh" : "Generate tetrahedral mesh");
            if (!can_generate_tet_mesh) EndDisabled();

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

            SeparatorText("Transform");
            if (Checkbox("Gizmo##Transform", &Scene.ShowGizmo)) {
                if (Scene.ShowGizmo) {
                    Scene.GizmoTransform = GetTransform();
                    Scene.GizmoCallback = [this](const glm::mat4 &transform) {
                        TriangularMesh.SetTransform(transform);
                        TetMesh.SetTransform(transform);
                        Translation = glm::vec3{transform[3]};
                        Scale = glm::vec3{glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2])};
                        RotationAngles = glm::eulerAngles(glm::quat_cast(transform));
                    };
                } else {
                    Scene.GizmoCallback = nullptr;
                }
            }
            Scene.RenderGizmoDebug();

            Text("Translate");
            if (SliderFloat("x##Translate", &Translation.x, -1, 1)) ApplyTransform();
            if (SliderFloat("y##Translate", &Translation.y, -1, 1)) ApplyTransform();
            if (SliderFloat("z##Translate", &Translation.z, -1, 1)) ApplyTransform();

            Text("Scale");
            static bool lock_scale = true;
            Checkbox("Lock##Scale", &lock_scale);
            if (lock_scale) {
                if (SliderFloat("Scale##Scale", &Scale.x, 0.1, 10, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                    Scale.y = Scale.z = Scale.x;
                    ApplyTransform();
                }
            } else {
                if (SliderFloat("x##Scale", &Scale.x, 0.1, 10, "%.3f", ImGuiSliderFlags_Logarithmic)) ApplyTransform();
                if (SliderFloat("y##Scale", &Scale.y, 0.1, 10, "%.3f", ImGuiSliderFlags_Logarithmic)) ApplyTransform();
                if (SliderFloat("z##Scale", &Scale.z, 0.1, 10, "%.3f", ImGuiSliderFlags_Logarithmic)) ApplyTransform();
            }

            Text("Rotate");
            if (SliderFloat("x##Rotate", &RotationAngles.x, -180.f, 180.f)) ApplyTransform();
            if (SliderFloat("y##Rotate", &RotationAngles.y, -180.f, 180.f)) ApplyTransform();
            if (SliderFloat("z##Rotate", &RotationAngles.z, -180.f, 180.f)) ApplyTransform();

            SeparatorText("Debug");
            if (HoveredVertexIndex >= 0) {
                const auto &vertex = GetActiveGeometry().Vertices[HoveredVertexIndex];
                Text("Hovered vertex:\n\tIndex: %d\n\tPosition:\n\t\tx: %f\n\t\ty: %f\n\t\tz: %f", HoveredVertexIndex, vertex.x, vertex.y, vertex.z);
            }

            EndTabItem();
        }
        if (BeginTabItem("Mesh profile")) {
            RenderProfileConfig();
            EndTabItem();
        }
        if (BeginTabItem("RealImpact")) {
            if (!RealImpact) {
                if (std::filesystem::exists(FilePath.parent_path() / RealImpact::SampleDataFileName)) {
                    RealImpactLoader.RenderLauncher();
                } else {
                    Text("No RealImpact data found in the same directory as the mesh.");
                }
            } else {
                RealImpact->Render();
            }
            if (RealImpactLoader.Render() && RealImpact) {
                const size_t num_points = RealImpact->NumListenerPoints();
                RealImpactListenerPoints.InstanceModels.clear();
                RealImpactListenerPoints.InstanceColors.clear();
                for (size_t i = 0; i < num_points; i++) {
                    RealImpactListenerPoints.InstanceModels.push_back(glm::translate(Identity, RealImpact->ListenerPoint(i)));
                    RealImpactListenerPoints.InstanceColors.push_back({1, 1, 1, 1});
                }
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
