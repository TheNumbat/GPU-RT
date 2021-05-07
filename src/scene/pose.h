
#pragma once

#include <lib/mathlib.h>

constexpr float GLOBAL_SCALE = 0.05f;

struct Pose {
    Vec3 pos;
    Vec3 euler;
    Vec3 scale = Vec3{1.0f};

    Mat4 transform() const;
    Mat4 rotation_mat() const;
    Quat rotation_quat() const;

    void clamp_euler();
    bool valid() const;

    static Pose rotated(Vec3 angles);
    static Pose moved(Vec3 t);
    static Pose scaled(Vec3 s);
    static Pose id();
};
