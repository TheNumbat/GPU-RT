
#pragma once

#include "material.h"
#include "pose.h"
#include <vk/render.h>

class Object {
public:
    Object() = default;
    Object(const Object& src) = delete;
    Object(Object&& src) = default;

    Object(unsigned int id, Pose p, VK::Mesh&& m, Material mat);

    ~Object() = default;

    Object& operator=(const Object& src) = delete;
    Object& operator=(Object&& src) = default;

    void render(const Mat4& view);

    unsigned int id() const;
    const VK::Mesh& mesh() const;

    Pose pose;
    Material material;

private:
    unsigned int _id = 0;
    VK::Mesh _mesh;
};
