struct State {
    struct WindowState {
        const char *Name{""};
        bool Visible{true};
    };

    struct WindowsState {
        WindowState Main{"Main"};
        WindowState ImGuiDemo{"Dear ImGui Demo"};
        WindowState ImPlotDemo{"ImPlot Demo"};
    };

    WindowsState Windows{};
};
