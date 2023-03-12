#pragma once

#include <string>
#include <vector>

using std::string, std::vector;
using u32 = unsigned int;

struct Window {
    const char *Name{""};
    bool Visible{true};
};

struct WindowsState {
    Window AudioDevice{"Audio Device"};
    Window MeshControls{"Mesh Controls"};
    Window Mesh{"Mesh"};
    Window ImGuiDemo{"Dear ImGui Demo"};
    Window ImPlotDemo{"ImPlot Demo"};
};

struct Faust {
    string Code;
    string Error;
};

struct State;

struct Audio {
    struct Device {
        void Init();
        void Update(); // Update device based on current settings.
        void Destroy();

        void Render();

        void Start() const;
        void Stop() const;
        bool IsStarted() const;

        bool On = true;
        bool Muted = true;
        float Volume = 1.0; // Master volume. Corresponds to `ma_device_set_master_volume`.
        string InDeviceName, OutDeviceName;
        int InFormat, OutFormat;
        u32 SampleRate;
    };

    struct Graph {
        void Init();
        void Destroy();
    };

    void Init();
    void Update(State &s); // Update device based on current settings.
    void Destroy();
    bool NeedsRestart() const;

    Device Device;
    Graph Graph;
};

struct State {
    Audio Audio;
    Faust Faust;
    WindowsState Windows{};
};
