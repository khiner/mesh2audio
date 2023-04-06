#include <GL/glew.h>

#include <iostream>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "ImGuizmo.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "implot.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <nfd.h>

#include "Audio.h"
#include "GlCanvas.h"
#include "Mesh.h"
#include "Window.h"

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static Audio Audio;
static WindowsState Windows;

static std::unique_ptr<Mesh> mesh;

static ImGuizmo::OPERATION GizmoOp(ImGuizmo::TRANSLATE);
static const mat4 Identity(1.f);

static int RenderMode = 0;
static bool ShowCameraGizmo = true, ShowGrid = false, ShowMeshGizmo = false, ShowBounds = false;

int main(int, char **) {
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMEPAD) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Enable native IME.
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_Window *window = SDL_CreateWindow("mesh2audio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    if (glewInit() != GLEW_OK) {
        std::cout << "Error initializing `glew`.\n";
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;
    io.IniFilename = nullptr; // Disable ImGui's .ini file saving

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize file dialog & audio device.
    NFD_Init();
    Audio.Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != NULL);

    mesh = std::make_unique<Mesh>(fs::path("res") / "svg" / "std.svg");
    // Alternatively, we could initialize with a mesh obj file:
    // mesh.Load(fs::path("res") / "obj" / "car.obj");

    // Or generate a profile parametrically, and extrude it around the y axis.
    // const vector<vec2> trianglePath = {{1.0f, 1.0f}, {2.0f, 0.0f}, {1.0f, -1.0f}};
    // mesh.SetProfile(trianglePath);
    // mesh.ExtrudeProfile(100);

    mesh->Bind();

    glEnable(GL_DEPTH_TEST);
    static GlCanvas gl_canvas;

    // Main loop
    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = NULL;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        auto dockspace_id = ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        if (ImGui::GetFrameCount() == 1) {
            auto audio_node_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
            auto faust_code_node_id = ImGui::DockBuilderSplitNode(audio_node_id, ImGuiDir_Right, 0.5f, nullptr, &audio_node_id);
            ImGui::DockBuilderDockWindow(Windows.AudioDevice.Name, audio_node_id);
            ImGui::DockBuilderDockWindow(Windows.FaustCode.Name, faust_code_node_id);
            auto demo_node_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);
            ImGui::DockBuilderDockWindow(Windows.ImGuiDemo.Name, demo_node_id);
            ImGui::DockBuilderDockWindow(Windows.ImPlotDemo.Name, demo_node_id);
            auto mesh_node_id = dockspace_id;
            auto mesh_controls_node_id = ImGui::DockBuilderSplitNode(mesh_node_id, ImGuiDir_Left, 0.3f, nullptr, &mesh_node_id);
            auto mesh_profile_node_id = ImGui::DockBuilderSplitNode(mesh_node_id, ImGuiDir_Right, 0.6f, nullptr, &mesh_node_id);
            ImGui::DockBuilderDockWindow(Windows.MeshControls.Name, mesh_controls_node_id);
            ImGui::DockBuilderDockWindow(Windows.MeshProfile.Name, mesh_profile_node_id);
            ImGui::DockBuilderDockWindow(Windows.Mesh.Name, mesh_node_id);
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load mesh", nullptr)) {
                    nfdchar_t *file_path;
                    nfdfilteritem_t filter[] = {{"Mesh object", "obj"}, {"SVG profile", "svg"}};
                    nfdresult_t result = NFD_OpenDialog(&file_path, filter, 2, "res/");
                    if (result == NFD_OKAY) {
                        mesh = std::make_unique<Mesh>(file_path);
                        mesh->Bind();

                        NFD_FreePath(file_path);
                    } else if (result != NFD_CANCEL) {
                        std::cerr << "Error: " << NFD_GetError() << '\n';
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem(Windows.AudioDevice.Name, nullptr, &Windows.AudioDevice.Visible);
                ImGui::MenuItem(Windows.FaustCode.Name, nullptr, &Windows.FaustCode.Visible);
                ImGui::MenuItem(Windows.MeshControls.Name, nullptr, &Windows.MeshControls.Visible);
                ImGui::MenuItem(Windows.Mesh.Name, nullptr, &Windows.Mesh.Visible);
                ImGui::MenuItem(Windows.MeshProfile.Name, nullptr, &Windows.MeshProfile.Visible);
                ImGui::MenuItem(Windows.ImGuiDemo.Name, nullptr, &Windows.ImGuiDemo.Visible);
                ImGui::MenuItem(Windows.ImPlotDemo.Name, nullptr, &Windows.ImPlotDemo.Visible);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (Windows.ImGuiDemo.Visible) ImGui::ShowDemoWindow(&Windows.ImGuiDemo.Visible);
        if (Windows.ImPlotDemo.Visible) ImPlot::ShowDemoWindow(&Windows.ImPlotDemo.Visible);

        if (Windows.MeshControls.Visible) {
            ImGui::Begin(Windows.MeshControls.Name, &Windows.MeshControls.Visible);

            if (ImGui::BeginTabBar("MeshControlsTabBar")) {
                if (ImGui::BeginTabItem("Mesh")) {
                    ImGui::SeparatorText("Render mode");
                    ImGui::RadioButton("Smooth", &RenderMode, 0);
                    ImGui::SameLine();
                    ImGui::RadioButton("Lines", &RenderMode, 1);
                    ImGui::RadioButton("Point cloud", &RenderMode, 2);
                    ImGui::SameLine();
                    ImGui::RadioButton("Mesh", &RenderMode, 3);
                    ImGui::NewLine();
                    ImGui::SeparatorText("Gizmo");
                    ImGui::Checkbox("Show gizmo", &ShowMeshGizmo);
                    if (ShowMeshGizmo) {
                        const string interaction_text = "Interaction: " +
                            string(ImGuizmo::IsUsing() ? "Using Gizmo" : ImGuizmo::IsOver(ImGuizmo::TRANSLATE) ? "Translate hovered" :
                                       ImGuizmo::IsOver(ImGuizmo::ROTATE)                                      ? "Rotate hovered" :
                                       ImGuizmo::IsOver(ImGuizmo::SCALE)                                       ? "Scale hovered" :
                                       ImGuizmo::IsOver()                                                      ? "Hovered" :
                                                                                                                 "Not interacting");
                        ImGui::Text(interaction_text.c_str());

                        if (ImGui::IsKeyPressed(ImGuiKey_T)) GizmoOp = ImGuizmo::TRANSLATE;
                        if (ImGui::IsKeyPressed(ImGuiKey_R)) GizmoOp = ImGuizmo::ROTATE;
                        if (ImGui::IsKeyPressed(ImGuiKey_S)) GizmoOp = ImGuizmo::SCALE;
                        if (ImGui::RadioButton("Translate (T)", GizmoOp == ImGuizmo::TRANSLATE)) GizmoOp = ImGuizmo::TRANSLATE;
                        if (ImGui::RadioButton("Rotate (R)", GizmoOp == ImGuizmo::ROTATE)) GizmoOp = ImGuizmo::ROTATE;
                        if (ImGui::RadioButton("Scale (S)", GizmoOp == ImGuizmo::SCALE)) GizmoOp = ImGuizmo::SCALE;
                        if (ImGui::RadioButton("Universal", GizmoOp == ImGuizmo::UNIVERSAL)) GizmoOp = ImGuizmo::UNIVERSAL;
                        ImGui::Checkbox("Bound sizing", &ShowBounds);
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Camera")) {
                    ImGui::Checkbox("Show gizmo", &ShowCameraGizmo);
                    ImGui::SameLine();
                    ImGui::Checkbox("Grid", &ShowGrid);
                    ImGui::SliderFloat("FOV", &Mesh::fov, 20.f, 110.f);

                    float cameraDistance = Mesh::CameraDistance;
                    if (ImGui::SliderFloat("Distance", &cameraDistance, 1.f, 10.f)) {
                        Mesh::SetCameraDistance(cameraDistance);
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Lighing")) {
                    ImGui::SeparatorText("Colors");
                    ImGui::Checkbox("Custom colors", &Mesh::CustomColor);
                    if (Mesh::CustomColor) {
                        ImGui::SliderFloat3("Ambient R, G, B", &Mesh::Ambient[0], 0.0f, 1.0f);
                        ImGui::SliderFloat3("Diffusion R, G, B", &Mesh::Diffusion[0], 0.0f, 1.0f);
                        ImGui::SliderFloat3("Specular R, G, B", &Mesh::Specular[0], 0.0f, 1.0f);
                        ImGui::SliderFloat("Shininess", &Mesh::Shininess, 0.0f, 150.0f);
                    } else {
                        for (int i = 1; i < 3; i++) {
                            Mesh::Ambient[i] = Mesh::Ambient[0];
                            Mesh::Diffusion[i] = Mesh::Diffusion[0];
                            Mesh::Specular[i] = Mesh::Specular[0];
                        }
                        ImGui::SliderFloat("Ambient", &Mesh::Ambient[0], 0.0f, 1.0f);
                        ImGui::SliderFloat("Diffusion", &Mesh::Diffusion[0], 0.0f, 1.0f);
                        ImGui::SliderFloat("Specular", &Mesh::Specular[0], 0.0f, 1.0f);
                        ImGui::SliderFloat("Shininess", &Mesh::Shininess, 0.0f, 150.0f);
                    }

                    ImGui::SeparatorText("Lights");
                    for (int i = 0; i < Mesh::NumLights; i++) {
                        ImGui::Separator();
                        ImGui::PushID(i);
                        ImGui::Text("Light %d", i + 1);
                        ImGui::SliderFloat3("Positions", &Mesh::LightPositions[4 * i], -25.0f, 25.0f);
                        ImGui::ColorEdit3("Color", &Mesh::LightColors[4 * i]);
                        ImGui::PopID();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::End();
        }

        if (Windows.Mesh.Visible) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            ImGui::Begin(Windows.Mesh.Name, &Windows.Mesh.Visible);

            const auto content_region = ImGui::GetContentRegionAvail();
            Mesh::UpdateCameraProjection(content_region);
            if (mesh != nullptr && content_region.x > 0 && content_region.y > 0) {
                const auto bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
                gl_canvas.SetupRender(content_region.x, content_region.y, bg.x, bg.y, bg.z, bg.w);
                mesh->Render(RenderMode);
                unsigned int texture_id = gl_canvas.Render();
                ImGui::Image((void *)(intptr_t)texture_id, content_region, {0, 1}, {1, 0});
            }
            if (ShowMeshGizmo || ShowCameraGizmo || ShowGrid) {
                ImGuizmo::BeginFrame();
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y + ImGui::GetTextLineHeightWithSpacing(), content_region.x, content_region.y);
            }
            if (ShowGrid) {
                ImGuizmo::DrawGrid(&Mesh::CameraView[0][0], &Mesh::CameraProjection[0][0], &Identity[0][0], 100.f);
            }
            if (ShowMeshGizmo) {
                // This is how you would draw a test cube:
                // ImGuizmo::DrawCubes(&Mesh::CameraView[0][0], &Mesh::CameraProjection[0][0], &Mesh::ObjectMatrix[0][0], 1);
                ImGuizmo::Manipulate(
                    &Mesh::CameraView[0][0], &Mesh::CameraProjection[0][0], GizmoOp, ImGuizmo::LOCAL, &Mesh::ObjectMatrix[0][0], nullptr,
                    nullptr, ShowBounds ? Mesh::Bounds : nullptr, nullptr
                );
            }
            if (ShowCameraGizmo) {
                static const float view_manipulate_size = 128;
                const auto viewManipulatePos = ImGui::GetWindowPos() +
                    ImVec2{
                        ImGui::GetWindowContentRegionMax().x - view_manipulate_size,
                        ImGui::GetWindowContentRegionMin().y,
                    };
                ImGuizmo::ViewManipulate(&Mesh::CameraView[0][0], Mesh::CameraDistance, viewManipulatePos, {view_manipulate_size, view_manipulate_size}, 0);
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }
        if (Windows.MeshProfile.Visible) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            ImGui::Begin(Windows.MeshProfile.Name, &Windows.MeshProfile.Visible);

            if (mesh != nullptr) mesh->RenderProfile();
            else ImGui::Text("No mesh has been loaded.");

            ImGui::End();
            ImGui::PopStyleVar();
        }

        Audio.Update();
        if (Windows.AudioDevice.Visible) {
            ImGui::Begin(Windows.AudioDevice.Name, &Windows.AudioDevice.Visible);
            Audio.Device.Render();
            ImGui::End();
        }
        if (Windows.FaustCode.Visible) {
            ImGui::Begin(Windows.FaustCode.Name, &Windows.FaustCode.Visible);
            ImGui::InputTextMultiline("##Faust", &Audio.Faust.Code, ImGui::GetContentRegionAvail());
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    Audio.Destroy();
    NFD_Quit();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
