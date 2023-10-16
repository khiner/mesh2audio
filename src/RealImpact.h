#pragma once

#include <filesystem>
#include <string>

#include <glm/vec3.hpp>

#include "npy.hpp"

namespace fs = std::filesystem;

struct RealImpact {
    inline static const std::string SampleDataFileName = "deconvolved_0db.npy";
    inline static const std::string ListenerXYZsFileName = "listenerXYZ.npy";

    RealImpact(fs::path directory);

    void Render();

    size_t NumListenerPoints() const { return ListenerXYZs.shape[0]; }
    glm::vec3 ListenerPoint(size_t i) const { return {ListenerXYZs.data[i * 3], ListenerXYZs.data[i * 3 + 1], ListenerXYZs.data[i * 3 + 2]}; }

    fs::path Directory;
    npy::npy_data<float> SampleData;
    npy::npy_data<double> ListenerXYZs;
};
