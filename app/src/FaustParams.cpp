#include "FaustParams.h"
#include "imgui.h"

#include "Audio.h"

using namespace ImGui;

FaustParams *interface;

void DrawUiItem(const FaustParams::Item &item) {
    const auto type = item.type;
    const auto *label = item.label.c_str();
    if (!item.items.empty()) {
        if (!item.label.empty()) SeparatorText(label);
        for (const auto &it : item.items) {
            DrawUiItem(it);
        }
    }
    if (type == ItemType_Button) {
        Button(label);
        if (IsItemActivated() && *item.zone == 0.0) *item.zone = 1.0;
        else if (IsItemDeactivated() && *item.zone == 1.0) *item.zone = 0.0;
        // *item.zone = Real(IsItemActive()); // Send 1.0 when pressed, 0.0 otherwise.
    } else if (type == ItemType_CheckButton) {
        auto value = bool(*item.zone);
        if (Checkbox(label, &value)) *item.zone = Real(value);
    } else if (type == ItemType_NumEntry) {
        auto value = int(*item.zone);
        if (InputInt(label, &value, int(item.step))) *item.zone = Real(value);
    } else if (type == ItemType_Knob || type == ItemType_HSlider || type == ItemType_VSlider || type == ItemType_HBargraph || type == ItemType_VBargraph) {
        auto value = float(*item.zone);
        ImGuiSliderFlags flags = item.logscale ? ImGuiSliderFlags_Logarithmic : ImGuiSliderFlags_None;
        if (SliderFloat(label, &value, float(item.min), float(item.max), nullptr, flags)) *item.zone = Real(value);
    } else if (type == ItemType_HRadioButtons || type == ItemType_VRadioButtons) {
    } else if (type == ItemType_Menu) {
        auto value = float(*item.zone);
        const auto &names_and_values = interface->names_and_values[item.zone];
        // todo handle not present
        const auto selected_index = find(names_and_values.values.begin(), names_and_values.values.end(), value) - names_and_values.values.begin();
        if (BeginCombo(label, names_and_values.names[selected_index].c_str())) {
            for (int i = 0; i < int(names_and_values.names.size()); i++) {
                const Real choice_value = names_and_values.values[i];
                const bool is_selected = value == choice_value;
                if (Selectable(names_and_values.names[i].c_str(), is_selected)) *item.zone = Real(choice_value);
            }
            EndCombo();
        }
    }
    if (item.tooltip) {
        SameLine();
        TextDisabled("(?)");
        if (IsItemHovered() && BeginTooltip()) {
            PushTextWrapPos(GetFontSize() * 35);
            TextUnformatted(item.tooltip);
            PopTextWrapPos();
            EndTooltip();
        }
    }
}

void Audio::FaustState::Render() const {
    if (!interface) return;

    DrawUiItem(interface->ui);
}

void OnUiChange(FaustParams *ui) {
    interface = ui;
}
