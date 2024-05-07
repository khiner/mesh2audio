#pragma once

#include <map>
#include <string>

// Defaults to steel.
struct MaterialProperties {
    double Density, YoungModulus, PoissonRatio, Alpha, Beta;
};

inline const std::map<std::string, MaterialProperties> MaterialPresets = {
    {"Ceramic", {2700, 7.2E10, 0.19, 6, 1E-7}},
    {"Glass", {2600, 6.2E10, 0.20, 1, 1E-7}},
    {"Wood", {750, 1.1E10, 0.25, 60, 2E-6}},
    {"Plastic", {1070, 1.4E9, 0.35, 30, 1E-6}},
    {"Iron", {8000, 2.1E11, 0.28, 5, 1E-7}},
    {"Polycarbonate", {1190, 2.4E9, 0.37, 0.5, 4E-7}},
    {"Steel", {7850, 2.0E11, 0.29, 5, 3E-8}}
};
inline MaterialProperties Material{MaterialPresets.at("Steel")};
