#include "RealImpact.h"

#include "imgui.h"

using namespace ImGui;

RealImpact::RealImpact(fs::path directory) : Directory(directory), SampleData(npy::read_npy<float>(Directory / SampleDataFileName)) {}

void RealImpact::Render() {
    Text("Real impact");
}
