#include "RealImpact.h"

#include "imgui.h"

using namespace ImGui;

RealImpact::RealImpact(fs::path directory)
    : Directory(directory),
      SampleData(npy::read_npy<float>(Directory / SampleDataFileName)),
      ListenerXYZs(npy::read_npy<double>(Directory / ListenerXYZsFileName)) {}

void RealImpact::Render() {
    Text("Directory: %s", Directory.c_str());
    Text("Sample data shape:\n\t(%lu, %lu)", SampleData.shape[0], SampleData.shape[1]);
}
