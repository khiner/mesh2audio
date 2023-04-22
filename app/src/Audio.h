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

inline static Audio Audio{}; // Global instance

inline static string GeneratedDsp; // The most recently generated DSP code.
// DSP code in addition to the model, to be appended to make it playable.
inline static const string FaustInstrumentDsp = R"(

freq = hslider("Frequency[scale:log][tooltip: Fundamental frequency of the model]",220,60,8000,1) : ba.sAndH(gate);
exPos = nentry("exPos",0,0,6,1) : ba.sAndH(gate);
t60Scale = hslider("t60[tooltip: Resonance duration (s) of the lowest mode.]",30,0,100,0.01) : ba.sAndH(gate);

t60Decay = hslider("t60 Decay[tooltip: Decay of modes as a function of their frequency, in t60 units.
At 1, the t60 of the highest mode will be close to 0 seconds.]",1,0,1,0.01) : ba.sAndH(gate);

t60Slope = hslider("t60 Slope[tooltip: Power of the function used to compute the decay of modes t60 in function of their frequency.
At 1, decay is linear. At 2, decay slope has degree 2, etc.]",3,1,6,0.01) : ba.sAndH(gate);
hammerHardness = hslider("hammerHardness",0.9,0,1,0.01) : ba.sAndH(gate);
hammerSize = hslider("hammerSize",0.3,0,1,0.01) : ba.sAndH(gate);
gain = hslider("gain",0.1,0,1,0.01);
gate = button("gate");

hammer(trig,hardness,size) = en.ar(att,att,trig)*no.noise : fi.lowpass(3,ctoff)
with{
  ctoff = (1-size)*9500+500;
  att = (1-hardness)*0.01+0.001;
};

process = hammer(gate,hammerHardness,hammerSize) : modalModel(freq,exPos,t60Scale,t60Decay,t60Slope)*gain;
)";

inline static string GenerateDspMsg = "Generating DSP code...";
