#pragma once

struct Window {
    const char *Name{""};
    bool Visible{true};
};

struct WindowsState {
    Window AudioDevice{"Audio device"};
    Window AudioModel{"Audio model"};
    Window SceneControls{"Scene controls"};
    Window MeshControls{"Mesh controls"};
    Window Scene{"Scene"};
    Window MeshProfile{"Mesh profile"};
    // By default, these demo windows are docked, but not visible.
    Window ImGuiDemo{"Dear ImGui demo", false};
    Window ImPlotDemo{"ImPlot demo", false};
};
