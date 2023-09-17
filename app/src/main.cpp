#include <GL/glew.h>

#include <fstream>
#include <iostream>
#include <thread>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"
#include "implot.h"
#include "imspinner.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <nfd.h>

#include "Audio.h"
#include "Mesh.h"
#include "RealImpact.h"
#include "Window.h"

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static WindowsState Windows;

static std::unique_ptr<Mesh> mesh;

static fs::path DspTetMeshPath; // Path to the tet mesh used for the most recent DSP generation.
static std::thread DspGeneratorThread; // Worker thread for generating DSP code.
static bool IsGeneratedDsp2d = false; // `false` means 3D.
static string GeneratedDsp; // The most recently generated DSP code.
static string GenerateDspMsg = "Generating DSP code...";

::Audio Audio{};

using namespace ImGui;

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

    auto window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED;
    SDL_Window *window = SDL_CreateWindowWithPosition("mesh2audio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    const int glew_result = glewInit();
    // GLEW_ERROR_NO_GLX_DISPLAY seems to be a false alarm - still works for me!
    if (glew_result != GLEW_OK && glew_result != GLEW_ERROR_NO_GLX_DISPLAY) {
        std::cout << "Error initializing `glew`: Error " << glew_result << '\n';
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    CreateContext();
    ImPlot::CreateContext();

    ImGuiIO &io = GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;
    io.IniFilename = nullptr; // Disable ImGui's .ini file saving

    // Setup Dear ImGui style
    StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        // Smoother circles and curves.
        style.CircleTessellationMaxError = 0.1f;
        style.CurveTessellationTol = 0.1f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize file dialog & audio device.
    NFD_Init();
    Audio.Init();
    Audio.Run();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use PushFont()/PopFont() to select them.
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

    mesh = std::make_unique<Mesh>(fs::path("res") / "svg" / "bell" / "std.svg");
    // Alternatively, we could initialize with a mesh obj file:
    // mesh = std::make_unique<Mesh>(fs::path("res") / "obj" / "bell" / "english.obj");

    glEnable(GL_DEPTH_TEST);

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
        NewFrame();

        auto dockspace_id = DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        if (GetFrameCount() == 1) {
            auto audio_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
            auto audio_model_node_id = DockBuilderSplitNode(audio_node_id, ImGuiDir_Right, 0.5f, nullptr, &audio_node_id);
            DockBuilderDockWindow(Windows.AudioDevice.Name, audio_node_id);
            DockBuilderDockWindow(Windows.AudioModel.Name, audio_model_node_id);
            auto demo_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);
            DockBuilderDockWindow(Windows.ImGuiDemo.Name, demo_node_id);
            DockBuilderDockWindow(Windows.ImPlotDemo.Name, demo_node_id);
            auto mesh_node_id = dockspace_id;
            auto mesh_controls_node_id = DockBuilderSplitNode(mesh_node_id, ImGuiDir_Left, 0.4f, nullptr, &mesh_node_id);
            auto mesh_profile_node_id = DockBuilderSplitNode(mesh_node_id, ImGuiDir_Right, 0.8f, nullptr, &mesh_node_id);
            DockBuilderDockWindow(Windows.MeshControls.Name, mesh_controls_node_id);
            DockBuilderDockWindow(Windows.MeshProfile.Name, mesh_profile_node_id);
            DockBuilderDockWindow(Windows.Mesh.Name, mesh_node_id);
        }
        if (BeginMainMenuBar()) {
            if (BeginMenu("File")) {
                if (MenuItem("Load mesh", nullptr)) {
                    nfdchar_t *file_path;
                    nfdfilteritem_t filter[] = {{"Mesh object", "obj"}, {"SVG profile", "svg"}};
                    nfdresult_t result = NFD_OpenDialog(&file_path, filter, 2, "res/");
                    if (result == NFD_OKAY) {
                        mesh = std::make_unique<Mesh>(file_path);
                        NFD_FreePath(file_path);
                    } else if (result != NFD_CANCEL) {
                        std::cerr << "Error: " << NFD_GetError() << '\n';
                    }
                }
                if (MenuItem("Export mesh as obj", nullptr, false, mesh != nullptr)) {
                    nfdchar_t *save_path;
                    nfdfilteritem_t filter[] = {{"Mesh object", "obj"}};
                    nfdresult_t result = NFD_SaveDialog(&save_path, filter, 1, nullptr, "res/");
                    if (result == NFD_OKAY) {
                        mesh->Save(save_path);
                        NFD_FreePath(save_path);
                    } else if (result != NFD_CANCEL) {
                        std::cerr << "Error: " << NFD_GetError() << '\n';
                    }
                }
                if (MenuItem("Export profile as obj", nullptr, false, mesh != nullptr && mesh->HasProfile())) {
                    nfdchar_t *save_path;
                    nfdfilteritem_t filter[] = {{"Mesh profile object", "obj"}};
                    nfdresult_t result = NFD_SaveDialog(&save_path, filter, 1, nullptr, "res/");
                    if (result == NFD_OKAY) {
                        mesh->SaveProfile(save_path);
                        NFD_FreePath(save_path);
                    } else if (result != NFD_CANCEL) {
                        std::cerr << "Error: " << NFD_GetError() << '\n';
                    }
                }
                EndMenu();
            }
            if (BeginMenu("Windows")) {
                MenuItem(Windows.AudioDevice.Name, nullptr, &Windows.AudioDevice.Visible);
                MenuItem(Windows.AudioModel.Name, nullptr, &Windows.AudioModel.Visible);
                MenuItem(Windows.MeshControls.Name, nullptr, &Windows.MeshControls.Visible);
                MenuItem(Windows.Mesh.Name, nullptr, &Windows.Mesh.Visible);
                MenuItem(Windows.MeshProfile.Name, nullptr, &Windows.MeshProfile.Visible);
                MenuItem(Windows.ImGuiDemo.Name, nullptr, &Windows.ImGuiDemo.Visible);
                MenuItem(Windows.ImPlotDemo.Name, nullptr, &Windows.ImPlotDemo.Visible);
                EndMenu();
            }
            EndMainMenuBar();
        }

        if (Windows.ImGuiDemo.Visible) ShowDemoWindow(&Windows.ImGuiDemo.Visible);
        if (Windows.ImPlotDemo.Visible) ImPlot::ShowDemoWindow(&Windows.ImPlotDemo.Visible);

        if (Windows.MeshControls.Visible) {
            Begin(Windows.MeshControls.Name, &Windows.MeshControls.Visible);
            if (mesh == nullptr) {
                Text("No mesh has been loaded.");
            } else {
                mesh->RenderConfig();
            }
            End();
        }

        if (Windows.Mesh.Visible) {
            PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            Begin(Windows.Mesh.Name, &Windows.Mesh.Visible);

            const auto content_region = GetContentRegionAvail(); // Must be called before mesh render.
            if (mesh != nullptr) mesh->Render();
            End();
            PopStyleVar();
        }
        if (Windows.MeshProfile.Visible) {
            PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            Begin(Windows.MeshProfile.Name, &Windows.MeshProfile.Visible);

            if (mesh != nullptr) mesh->RenderProfile();
            else Text("No mesh has been loaded.");

            End();
            PopStyleVar();
        }

        if (Windows.AudioDevice.Visible) {
            Begin(Windows.AudioDevice.Name, &Windows.AudioDevice.Visible);
            Audio.Render();
            End();
        }
        if (Windows.AudioModel.Visible) {
            Begin(Windows.AudioModel.Name, &Windows.AudioModel.Visible);

            if (!DspTetMeshPath.empty()) {
                const string name = Mesh::GetTetMeshName(DspTetMeshPath);
                Text("Current DSP generated from tetrahedral mesh\ncreated at %s", name.c_str());
            }
            if (BeginTabBar("Audio model")) {
                if (BeginTabItem("Model")) {
                    const bool has_tetrahedral_mesh = mesh->HasTetMesh();
                    const bool has_profile = mesh->HasProfile();
                    if (!has_tetrahedral_mesh) BeginDisabled();
                    const bool generate_tet_dsp = Button("Generate 3D DSP");
                    if (!has_tetrahedral_mesh) {
                        SameLine();
                        TextUnformatted("Run |Mesh Controls|->|Mesh|->|Generate tetrahedral mesh|.");
                        EndDisabled();
                    }
                    if (!has_profile) BeginDisabled();
                    const bool generate_axisymmetric_dsp = Button("Generate axisymmetric DSP");
                    if (!has_profile) {
                        SameLine();
                        TextUnformatted("Run |File|->|Open mesh| and select an SVG file.");
                        EndDisabled();
                    }
                    if (generate_tet_dsp || generate_axisymmetric_dsp) {
                        OpenPopup(GenerateDspMsg.c_str());
                        if (DspGeneratorThread.joinable()) DspGeneratorThread.join();
                        IsGeneratedDsp2d = !generate_tet_dsp;
                        DspGeneratorThread = std::thread([&] {
                            const int num_excitations = generate_tet_dsp ? mesh->Num3DExcitationVertices() : mesh->Num2DExcitationVertices();
                            const string model_dsp = generate_tet_dsp ? mesh->GenerateDsp() : mesh->GenerateDspAxisymmetric();
                            if (!model_dsp.empty()) {
                                // Cache the path to the tet mesh that was used to generate the most recent DSP.
                                DspTetMeshPath = mesh->TetMeshPath;
                                GeneratedDsp = Audio::FaustState::GenerateModelInstrumentDsp(model_dsp, num_excitations);
                            } else {
                                DspTetMeshPath = "";
                                GeneratedDsp = "process = _;";
                            }
                        });
                    }
                    const auto &center = GetMainViewport()->GetCenter();
                    const auto &popup_size = GetMainViewport()->Size / 4;
                    SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
                    SetNextWindowSize(popup_size);
                    if (BeginPopupModal(GenerateDspMsg.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                        const auto &ws = GetWindowSize();
                        const float spinner_dim = std::min(ws.x, ws.y) / 2;
                        SetCursorPos((ws - ImVec2{spinner_dim, spinner_dim}) / 2 + ImVec2(0, GetTextLineHeight()));
                        ImSpinner::SpinnerWaveDots(GenerateDspMsg.c_str(), spinner_dim / 2, 3);
                        if (!GeneratedDsp.empty()) {
                            Audio.Faust.Code = GeneratedDsp;
                            Audio::FaustState::Is2DModel = IsGeneratedDsp2d;
                            GeneratedDsp = "";
                            if (DspGeneratorThread.joinable()) DspGeneratorThread.join();
                            CloseCurrentPopup();
                        }
                        EndPopup();
                    }
                    if (has_tetrahedral_mesh || has_profile) {
                        SeparatorText("Material properties");
                        // Presets
                        static std::string selected_preset = "Bell";
                        if (BeginCombo("Presets", selected_preset.c_str())) {
                            for (const auto &[preset_name, material] : MaterialPresets) {
                                bool is_selected = (preset_name == selected_preset);
                                if (Selectable(preset_name.c_str(), is_selected)) {
                                    selected_preset = preset_name;
                                    Material = material;
                                }
                                if (is_selected) SetItemDefaultFocus();
                            }
                            EndCombo();
                        }
                        Text("Young's modulus (Pa)");
                        InputDouble("##Young's modulus", &Material.YoungModulus, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue);
                        Text("Poisson's ratio");
                        InputDouble("##Poisson's ratio", &Material.PoissonRatio, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue);
                        Text("Density (kg/m^3)");
                        InputDouble("##Density", &Material.Density, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue);
                    }
                    EndTabItem();
                }
                if (!Audio.Faust.Code.empty()) {
                    if (BeginTabItem("Code")) {
                        if (Button("Export to file")) {
                            nfdchar_t *save_path;
                            nfdfilteritem_t filter[] = {{"Faust DSP", "dsp"}};
                            nfdresult_t result = NFD_SaveDialog(&save_path, filter, 1, nullptr, "model");
                            if (result == NFD_OKAY) {
                                // Write the Faust code to the file.
                                std::ofstream file(save_path);
                                file << Audio.Faust.Code;
                                file.close();
                                NFD_FreePath(save_path);
                            } else if (result != NFD_CANCEL) {
                                std::cerr << "Error: " << NFD_GetError() << '\n';
                            }
                        }
                        TextUnformatted(Audio.Faust.Code.c_str());
                        EndTabItem();
                    }
                    if (BeginTabItem("Control")) {
                        Audio.Faust.Render();
                        EndTabItem();
                    }
                }
                EndTabBar();
            }
            End();
        }

        // Rendering
        Render();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            UpdatePlatformWindows();
            RenderPlatformWindowsDefault();
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
    DestroyContext();

    Audio.Stop();
    if (DspGeneratorThread.joinable()) DspGeneratorThread.join();
    NFD_Quit();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
