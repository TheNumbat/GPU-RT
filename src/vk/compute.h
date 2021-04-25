
#pragma once

#include <vector>

#include <lib/mathlib.h>
#include <util/camera.h>
#include <scene/scene.h>

#include "vulkan.h"
#include "render.h"

namespace VK {

struct CPQPipe {

    CPQPipe() = default;
    ~CPQPipe();

    CPQPipe(const CPQPipe&) = delete;
    CPQPipe(CPQPipe&& src) = default;
    CPQPipe& operator=(const CPQPipe&) = delete;
    CPQPipe& operator=(CPQPipe&& src) = default;

    void recreate();
    void destroy();

    std::vector<Vec4> run(const Mesh& mesh, const std::vector<Vec4>& queries);

    Drop<PipeData> pipe;

private:
    VkWriteDescriptorSet write_buf(const Buffer& buf, int bind);
    std::array<VkDescriptorBufferInfo,4> buf_infos;
    void create_pipe();
    void create_desc();
};

}
