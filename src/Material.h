#pragma once

#include <map>
#include <string>

// Defaults to aluminum.
struct MaterialProperties {
    double YoungModulus, PoissonRatio, Density;
};

extern std::map<std::string, MaterialProperties> MaterialPresets;
extern MaterialProperties Material; // Global.
