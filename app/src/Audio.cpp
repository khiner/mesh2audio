#include <filesystem>
#include <fmt/core.h>
#include <locale>
#include <stdexcept>
#include <string_view>
#include <vector>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "faust/dsp/llvm-dsp.h"
using Sample = float;
#ifndef FAUSTFLOAT
#define FAUSTFLOAT Sample
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "Audio.h"

using fmt::format;
using std::string_view, std::vector;

namespace fs = std::filesystem;

static string Capitalize(string copy) {
    if (copy.empty()) return "";

    copy[0] = toupper(copy[0], std::locale());
    return copy;
}

namespace FaustContext {
static dsp *Dsp = nullptr;

static void Init(Audio::FaustState &faust, unsigned int sample_rate) {
    createLibContext();

    int argc = 0;
    const char **argv = new const char *[8];
    argv[argc++] = "-I";
    argv[argc++] = fs::relative("../lib/faust/libraries").c_str();
    if (std::is_same_v<Sample, double>) argv[argc++] = "-double";

    static int num_inputs, num_outputs;
    static string error_msg;
    const Box box = DSPToBoxes("FlowGrid", faust.Code, argc, argv, &num_inputs, &num_outputs, error_msg);

    static llvm_dsp_factory *dsp_factory;
    if (box && error_msg.empty()) {
        static const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromBoxes("FlowGrid", box, argc, argv, "", error_msg, optimize_level);
    }
    if (!box && error_msg.empty()) error_msg = "`DSPToBoxes` returned no error but did not produce a result.";

    if (dsp_factory && error_msg.empty()) {
        Dsp = dsp_factory->createDSPInstance();
        if (!Dsp) error_msg = "Could not create Faust DSP.";
        else {
            Dsp->init(sample_rate);
            // Ui = make_unique<FaustParams>();
            // Dsp->buildUserInterface(Ui.get());
        }
    }

    if (!error_msg.empty()) faust.Error = error_msg;
    else if (!faust.Error.empty()) faust.Error = "";

    // OnBoxChange(box);
    // OnUiChange(Ui.get());
}

static void Destroy() {
    // OnBoxChange(nullptr);
    // OnUiChange(nullptr);

    if (Dsp) {
        delete Dsp;
        Dsp = nullptr;
        deleteAllDSPFactories(); // There should only be one factory, but using this instead of `deleteDSPFactory` avoids storing another file-scoped variable.
    }

    destroyLibContext();
}

static bool NeedsRestart(const Audio::FaustState &faust) {
    static string PreviousFaustCode = faust.Code;

    const bool needs_restart = faust.Code != PreviousFaustCode;
    PreviousFaustCode = faust.Code;
    return needs_restart;
}

static void Update(Audio::FaustState &faust, u32 sample_rate) {
    // Faust setup is only dependent on the faust code.
    const bool is_faust_initialized = !faust.Code.empty() && faust.Error.empty();
    const bool faust_needs_restart = NeedsRestart(faust); // Don't inline! Must run during every update.
    if (!Dsp && is_faust_initialized) {
        Init(faust, sample_rate);
    } else if (Dsp && !is_faust_initialized) {
        Destroy();
    } else if (faust_needs_restart) {
        Destroy();
        Init(faust, sample_rate);
    }
}
} // namespace FaustContext

static ma_context AudioContext;
static ma_device MaDevice;
static ma_device_config DeviceConfig;
static ma_device_info DeviceInfo;
static ma_audio_buffer_ref InputBuffer;
static ma_node_graph NodeGraph;
static ma_node_graph_config NodeGraphConfig;
static ma_node *OutputNode;
static ma_node_base FaustNode{};
static ma_data_source_node InputNode{};

enum IO_ {
    IO_None = -1,
    IO_In,
    IO_Out
};
using IO = IO_;
constexpr IO IO_All[] = {IO_In, IO_Out};
constexpr int IO_Count = 2;

static vector<ma_device_info *> DeviceInfos[IO_Count];
static vector<string> DeviceNames[IO_Count];
const vector<u32> PrioritizedSampleRates = {std::begin(g_maStandardSampleRatePriorities), std::end(g_maStandardSampleRatePriorities)};
static vector<ma_format> NativeFormats;
static vector<u32> NativeSampleRates;

static const ma_device_id *GetDeviceId(IO io, string_view device_name) {
    for (const ma_device_info *info : DeviceInfos[io]) {
        if (info->name == device_name) return &(info->id);
    }
    return nullptr;
}

void DataCallback(ma_device *device, void *output, const void *input, ma_uint32 frame_count) {
    ma_audio_buffer_ref_set_data(&InputBuffer, input, frame_count);
    ma_node_graph_read_pcm_frames(&NodeGraph, output, frame_count, nullptr);
    (void)device; // unused
}

void FaustProcess(ma_node *node, const float **const_bus_frames_in, ma_uint32 *frame_count_in, float **bus_frames_out, ma_uint32 *frame_count_out) {
    // ma_pcm_rb_init_ex()
    // ma_deinterleave_pcm_frames()
    float **bus_frames_in = const_cast<float **>(const_bus_frames_in); // Faust `compute` expects a non-const buffer: https://github.com/grame-cncm/faust/pull/850
    if (FaustContext::Dsp) FaustContext::Dsp->compute(*frame_count_out, bus_frames_in, bus_frames_out);

    (void)node; // unused
    (void)frame_count_in; // unused
}

void Audio::Init() {
    for (const IO io : IO_All) {
        DeviceInfos[io].clear();
        DeviceNames[io].clear();
    }

    int result = ma_context_init(nullptr, 0, nullptr, &AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio context: {}", result));

    static u32 PlaybackDeviceCount, CaptureDeviceCount;
    static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
    result = ma_context_get_devices(&AudioContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error getting audio devices: {}", result));

    for (u32 i = 0; i < CaptureDeviceCount; i++) {
        DeviceInfos[IO_In].emplace_back(&CaptureDeviceInfos[i]);
        DeviceNames[IO_In].push_back(CaptureDeviceInfos[i].name);
    }
    for (u32 i = 0; i < PlaybackDeviceCount; i++) {
        DeviceInfos[IO_Out].emplace_back(&PlaybackDeviceInfos[i]);
        DeviceNames[IO_Out].push_back(PlaybackDeviceInfos[i].name);
    }

    Device.Init();
    FaustContext::Update(Faust, Device.SampleRate);
    Graph.Init();
    Device.Start();

    NeedsRestart(); // xxx Updates cached values as side effect.
    Update();
}

void Audio::Destroy() {
    Device.Stop();
    Graph.Destroy();
    Device.Destroy();

    const int result = ma_context_uninit(&AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error shutting down audio context: {}", result));
}

void Audio::Update() {
    FaustContext::Update(Faust, Device.SampleRate);
    const bool is_initialized = Device.IsStarted();
    const bool needs_restart = NeedsRestart(); // Don't inline! Must run during every update.
    if (Device.On && !is_initialized) {
        Init();
    } else if (!Device.On && is_initialized) {
        Destroy();
    } else if (needs_restart && is_initialized) {
        Destroy();
        Init();
    }
    if (Device.IsStarted()) {
        // Not working? Setting Faust node volume instead.
        // ma_device_set_master_volume(&MaDevice, Volume);
        ma_node_set_output_bus_volume(&FaustNode, 0, Device.Muted ? 0.0f : Device.Volume);
    }
}

bool Audio::NeedsRestart() const {
    static string PreviousInDeviceName = Device.InDeviceName, PreviousOutDeviceName = Device.OutDeviceName;
    static int PreviousInFormat = Device.InFormat, PreviousOutFormat = Device.OutFormat;
    static u32 PreviousSampleRate = Device.SampleRate;

    const bool needs_restart =
        PreviousInDeviceName != Device.InDeviceName ||
        PreviousOutDeviceName != Device.OutDeviceName ||
        PreviousInFormat != Device.InFormat || PreviousOutFormat != Device.OutFormat ||
        PreviousSampleRate != Device.SampleRate;

    PreviousInDeviceName = Device.InDeviceName;
    PreviousOutDeviceName = Device.OutDeviceName;
    PreviousInFormat = Device.InFormat;
    PreviousOutFormat = Device.OutFormat;
    PreviousSampleRate = Device.SampleRate;

    return needs_restart;
}

void Audio::AudioDevice::Init() {
    DeviceConfig = ma_device_config_init(ma_device_type_duplex);
    DeviceConfig.capture.pDeviceID = GetDeviceId(IO_In, InDeviceName);
    DeviceConfig.capture.format = ma_format_f32;
    DeviceConfig.capture.channels = 1; // Temporary (2)
    DeviceConfig.capture.shareMode = ma_share_mode_shared;
    DeviceConfig.playback.pDeviceID = GetDeviceId(IO_Out, OutDeviceName);
    DeviceConfig.playback.format = ma_format_f32;
    DeviceConfig.playback.channels = 1; // Temporary (2)
    DeviceConfig.dataCallback = DataCallback;
    DeviceConfig.sampleRate = SampleRate;

    int result = ma_device_init(nullptr, &DeviceConfig, &MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio device: {}", result));

    result = ma_context_get_device_info(MaDevice.pContext, MaDevice.type, nullptr, &DeviceInfo);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error getting audio device info: {}", result));

    for (u32 i = 0; i < DeviceInfo.nativeDataFormatCount; i++) {
        const auto &native_format = DeviceInfo.nativeDataFormats[i];
        NativeFormats.emplace_back(native_format.format);
        NativeSampleRates.emplace_back(native_format.sampleRate);
    }

    if (MaDevice.capture.name != InDeviceName) InDeviceName = MaDevice.capture.name;
    if (MaDevice.playback.name != OutDeviceName) OutDeviceName = MaDevice.playback.name;
    if (MaDevice.capture.format != InFormat) InFormat = MaDevice.capture.format;
    if (MaDevice.playback.format != OutFormat) OutFormat = MaDevice.playback.format;
    if (MaDevice.sampleRate != SampleRate) SampleRate = MaDevice.sampleRate;
}

bool Audio::AudioDevice::IsStarted() const { return ma_device_is_started(&MaDevice); }

const string GetFormatName(const int format) {
    const bool is_native = std::find(NativeFormats.begin(), NativeFormats.end(), format) != NativeFormats.end();
    return fmt::format("{}{}", ma_get_format_name((ma_format)format), is_native ? "*" : "");
}
const string GetSampleRateName(const u32 sample_rate) {
    const bool is_native = std::find(NativeSampleRates.begin(), NativeSampleRates.end(), sample_rate) != NativeSampleRates.end();
    return format("{}{}", sample_rate, is_native ? "*" : "");
}

using namespace ImGui;
using ImGui::Text;

static string to_string(const IO io, const bool shorten = false) {
    switch (io) {
        case IO_In: return shorten ? "in" : "input";
        case IO_Out: return shorten ? "out" : "output";
        case IO_None: return "none";
    }
}

void Audio::AudioDevice::Render() {
    Checkbox("On", &On);
    if (!IsStarted()) {
        TextUnformatted("No audio device started yet");
        return;
    }
    Checkbox("Muted", &Muted);
    SameLine();
    if (Muted) BeginDisabled();
    SliderFloat("Volume", &Volume, 0, 1, nullptr);
    if (Muted) EndDisabled();
    if (BeginCombo("Sample rate", GetSampleRateName(SampleRate).c_str())) {
        for (u32 option : PrioritizedSampleRates) {
            const bool is_selected = option == SampleRate;
            if (Selectable(GetSampleRateName(option).c_str(), is_selected)) SampleRate = option;
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    for (const IO io : IO_All) {
        TextUnformatted(Capitalize(to_string(io)).c_str());
        const bool is_in = io == IO_In;
        const auto &device_name = is_in ? InDeviceName : OutDeviceName;
        const auto &device_names = DeviceNames[io];
        if (BeginCombo("Device", device_name.c_str())) {
            for (const auto &option : device_names) {
                const bool is_selected = option == device_name;
                if (Selectable(option.c_str(), is_selected)) is_in ? InDeviceName = option : OutDeviceName = option;
                if (is_selected) SetItemDefaultFocus();
            }
            EndCombo();
        }
        // No format selection - always using f32 format.
    }
    if (TreeNode("Info")) {
        auto *device = &MaDevice;
        assert(device->type == ma_device_type_duplex || device->type == ma_device_type_loopback);

        Text("[%s]", ma_get_backend_name(device->pContext->backend));

        static char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
        ma_device_get_name(device, device->type == ma_device_type_loopback ? ma_device_type_playback : ma_device_type_capture, name, sizeof(name), nullptr);
        if (TreeNode(format("{} ({})", name, "Capture").c_str())) {
            Text("Format: %s -> %s", ma_get_format_name(device->capture.internalFormat), ma_get_format_name(device->capture.format));
            Text("Channels: %d -> %d", device->capture.internalChannels, device->capture.channels);
            Text("Sample Rate: %d -> %d", device->capture.internalSampleRate, device->sampleRate);
            Text("Buffer Size: %d*%d (%d)\n", device->capture.internalPeriodSizeInFrames, device->capture.internalPeriods, (device->capture.internalPeriodSizeInFrames * device->capture.internalPeriods));
            if (TreeNodeEx("Conversion", ImGuiTreeNodeFlags_DefaultOpen)) {
                Text("Pre Format Conversion: %s\n", device->capture.converter.hasPreFormatConversion ? "YES" : "NO");
                Text("Post Format Conversion: %s\n", device->capture.converter.hasPostFormatConversion ? "YES" : "NO");
                Text("Channel Routing: %s\n", device->capture.converter.hasChannelConverter ? "YES" : "NO");
                Text("Resampling: %s\n", device->capture.converter.hasResampler ? "YES" : "NO");
                Text("Passthrough: %s\n", device->capture.converter.isPassthrough ? "YES" : "NO");
                {
                    char channel_map[1024];
                    ma_channel_map_to_string(device->capture.internalChannelMap, device->capture.internalChannels, channel_map, sizeof(channel_map));
                    Text("Channel Map In: {%s}\n", channel_map);

                    ma_channel_map_to_string(device->capture.channelMap, device->capture.channels, channel_map, sizeof(channel_map));
                    Text("Channel Map Out: {%s}\n", channel_map);
                }
                TreePop();
            }
            TreePop();
        }

        if (device->type == ma_device_type_loopback) return;

        ma_device_get_name(device, ma_device_type_playback, name, sizeof(name), nullptr);
        if (TreeNode(format("{} ({})", name, "Playback").c_str())) {
            Text("Format: %s -> %s", ma_get_format_name(device->playback.format), ma_get_format_name(device->playback.internalFormat));
            Text("Channels: %d -> %d", device->playback.channels, device->playback.internalChannels);
            Text("Sample Rate: %d -> %d", device->sampleRate, device->playback.internalSampleRate);
            Text("Buffer Size: %d*%d (%d)", device->playback.internalPeriodSizeInFrames, device->playback.internalPeriods, (device->playback.internalPeriodSizeInFrames * device->playback.internalPeriods));
            if (TreeNodeEx("Conversion", ImGuiTreeNodeFlags_DefaultOpen)) {
                Text("Pre Format Conversion:  %s", device->playback.converter.hasPreFormatConversion ? "YES" : "NO");
                Text("Post Format Conversion: %s", device->playback.converter.hasPostFormatConversion ? "YES" : "NO");
                Text("Channel Routing: %s", device->playback.converter.hasChannelConverter ? "YES" : "NO");
                Text("Resampling: %s", device->playback.converter.hasResampler ? "YES" : "NO");
                Text("Passthrough: %s", device->playback.converter.isPassthrough ? "YES" : "NO");
                {
                    char channel_map[1024];
                    ma_channel_map_to_string(device->playback.channelMap, device->playback.channels, channel_map, sizeof(channel_map));
                    Text("Channel Map In: {%s}", channel_map);

                    ma_channel_map_to_string(device->playback.internalChannelMap, device->playback.internalChannels, channel_map, sizeof(channel_map));
                    Text("Channel Map Out: {%s}", channel_map);
                }
                TreePop();
            }
            TreePop();
        }
        TreePop();
    }
}

void Audio::AudioDevice::Destroy() {
    ma_device_uninit(&MaDevice);
}

void Audio::AudioDevice::Start() const {
    const int result = ma_device_start(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error starting audio device: {}", +result));
}
void Audio::AudioDevice::Stop() const {
    const int result = ma_device_stop(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error stopping audio device: {}", result));
}

void Audio::Graph::Init() {
    NodeGraphConfig = ma_node_graph_config_init(MaDevice.capture.channels);
    int result = ma_node_graph_init(&NodeGraphConfig, nullptr, &NodeGraph);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize node graph: {}", result));

    OutputNode = ma_node_graph_get_endpoint(&NodeGraph);
    result = ma_audio_buffer_ref_init(MaDevice.capture.format, MaDevice.capture.channels, nullptr, 0, &InputBuffer);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize input audio buffer: {}", result));

    static ma_data_source_node_config Config{};
    Config = ma_data_source_node_config_init(&InputBuffer);
    result = ma_data_source_node_init(&NodeGraph, &Config, nullptr, &InputNode);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize the input node: {}", result));

    if (FaustContext::Dsp) {
        const u32 in_channels = FaustContext::Dsp->getNumInputs();
        const u32 out_channels = FaustContext::Dsp->getNumOutputs();
        if (in_channels == 0 && out_channels == 0) return;

        static ma_node_vtable vtable{};
        vtable = {FaustProcess, nullptr, ma_uint8(in_channels > 0 ? 1 : 0), ma_uint8(out_channels > 0 ? 1 : 0), 0};

        static ma_node_config config;
        config = ma_node_config_init();
        config.pInputChannels = in_channels > 0 ? (u32[]){in_channels} : nullptr; // One input bus with N channels, or zero input busses.
        config.pOutputChannels = out_channels > 0 ? (u32[]){out_channels} : nullptr; // One output bus with M channels, or zero output busses.
        config.vtable = &vtable;

        const int result = ma_node_init(&NodeGraph, &config, nullptr, &FaustNode);
        if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize the Faust node: {}", result));
    }

    ma_node_attach_output_bus(&FaustNode, 0, OutputNode, 0);
    // ma_node_attach_output_bus(&InputNode, 0, &FaustNode, 0);
}

void Audio::Graph::Destroy() {
    FaustContext::Destroy();
    ma_data_source_node_uninit(&InputNode, nullptr);
    ma_audio_buffer_ref_uninit(&InputBuffer);
    ma_node_graph_uninit(&NodeGraph, nullptr); // Graph endpoint is already uninitialized in `Nodes.Uninit`.
}
