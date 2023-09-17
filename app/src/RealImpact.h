#pragma once

#include <filesystem>
#include <string>

#include "npy.hpp"

namespace fs = std::filesystem;

struct RealImpact {
    inline static const std::string SampleDataFileName = "deconvolved_0db.npy";

    RealImpact(fs::path directory);

    void Render();

    fs::path Directory;
    npy::npy_data<float> SampleData;
};
