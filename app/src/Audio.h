#pragma once

#include <string>

using std::string;
using u32 = unsigned int;

namespace AudioStatusMessage {
static const string Initializing = "Initializing...";
static const string Running = "Running";
static const string Compiling = "Compiling DSP...";
static const string NoDsp = "No DSP";
static const string Stopped = "Stopped";
} // namespace AudioStatusMessage

struct Audio {
    struct FaustState {
        string Code = "";
        string Error;

        void Render() const;
    };

    struct AudioDevice {
        void Init();
        void Destroy();

        void Render();

        void Start() const;
        void Stop() const;
        bool IsInitialized() const;
        bool IsStarted() const;

        bool On = true;
        bool Muted = false;
        float Volume = 1.0; // Master volume. Corresponds to `ma_device_set_master_volume`.
        string InDeviceName, OutDeviceName;
        int InFormat, OutFormat;
        u32 SampleRate;
    };

    struct Graph {
        void Init();
        void Destroy();
    };

    void Render();

    void Init();
    void Run();
    void Stop();
    void Update();
    void Destroy();
    bool NeedsRestart() const;

    string Status = AudioStatusMessage::Stopped;
    AudioDevice Device;
    Graph Graph;
    FaustState Faust;
};
