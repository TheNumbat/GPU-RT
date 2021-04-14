
#pragma once

#include <lib/mathlib.h>

enum class Material_Type : int { lambertian, mirror, refract, glass, diffuse_light, count };
extern const char* Material_Type_Names[(int)Material_Type::count];

class Material {
public:
    Material() = default;
    Material(Material_Type type) : type(type) {}
    Material(const Material& src) = default;
    Material(Material&& src) = default;
    ~Material() = default;

    Material& operator=(const Material& src) = default;
    Material& operator=(Material&& src) = default;

    Material_Type type = Material_Type::lambertian;
    Spectrum albedo = Spectrum(1.0f);
    Spectrum reflectance = Spectrum(1.0f);
    Spectrum transmittance = Spectrum(1.0f);
    Spectrum emissive = Spectrum(1.0f);
    float intensity = 1.0f;
    float ior = 1.2f;
};
