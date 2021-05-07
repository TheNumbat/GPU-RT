
#pragma once

#include <lib/mathlib.h>

class Material {
public:
    Material() = default;
    Material(const Material& src) = default;
    Material(Material&& src) = default;
    ~Material() = default;

    Material& operator=(const Material& src) = default;
    Material& operator=(Material&& src) = default;

    Vec3 albedo;
    int albedo_tex;
    Vec3 emissive;
    int emissive_tex;
    Vec2 metal_rough;
    int metal_rough_tex;
};
