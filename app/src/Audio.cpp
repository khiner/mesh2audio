#include <fmt/core.h>
#include <locale>
#include <stdexcept>
#include <string_view>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "imgui.h"

#include "State.h"

using fmt::format;
using std::string_view, std::to_string;

static string Capitalize(string copy) {
    if (copy.empty()) return "";

    copy[0] = toupper(copy[0], std::locale());
    return copy;
}

static ma_context AudioContext;
static ma_device MaDevice;
static ma_device_config DeviceConfig;
static ma_device_info DeviceInfo;
static ma_audio_buffer_ref InputBuffer;

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
    // ma_node_graph_read_pcm_frames(&NodeGraph, output, frame_count, nullptr);
    (void)device; // unused
}

void Audio::Init() {
    int result = ma_context_init(nullptr, 0, nullptr, &AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio context: {}", result));

    static u32 PlaybackDeviceCount, CaptureDeviceCount;
    static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
    result = ma_context_get_devices(&AudioContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error getting audio devices: {}", result));
}

void Audio::Destroy() {
}

void Audio::Device::Init() {
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

void Audio::Device::Update() {
    if (ma_device_is_started(&MaDevice)) ma_device_set_master_volume(&MaDevice, Volume);
}

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

void Audio::Device::Render() {
    Checkbox("On", &On);
    if (!ma_device_is_started(&MaDevice)) {
        TextUnformatted("No audio device started yet");
        return;
    }
    Checkbox("Muted", &Muted);
    SameLine();
    SliderFloat("Volume", &Volume, 0, 1, nullptr);
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

void Audio::Device::Destroy() {
    ma_device_uninit(&MaDevice);
}

void Audio::Device::Start() const {
    const int result = ma_device_start(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error starting audio device: {}", +result));
}
void Audio::Device::Stop() const {
    const int result = ma_device_stop(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error stopping audio device: {}", result));
}
