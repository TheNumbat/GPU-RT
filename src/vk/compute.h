
#pragma once

#include <vector>

#include <lib/mathlib.h>
#include <scene/scene.h>
#include <util/camera.h>

#include "render.h"
#include "vulkan.h"

class BVH;

namespace VK {

constexpr size_t BRUTE_MAX = 10000;
constexpr size_t MAX_BATCH = 100000;

enum class BVH_Type { none, threaded, stack, stackless };

struct BVHPipe {

    BVHPipe() = default;
    ~BVHPipe();

    BVHPipe(const BVHPipe&) = delete;
    BVHPipe(BVHPipe&& src) = default;
    BVHPipe& operator=(const BVHPipe&) = delete;
    BVHPipe& operator=(BVHPipe&& src) = default;

    void recreate();
    void destroy();

    std::vector<Vec4> cpqs(BVH_Type type, const BVH& bvh, const std::vector<Vec4>& queries);
    std::vector<Vec4> rays(BVH_Type type, const BVH& bvh,
                           const std::vector<std::pair<Vec4, Vec4>>& queries);

    Drop<PipeData> threaded_pipe;
    Drop<PipeData> brute_pipe;
    Drop<PipeData> stack_pipe;

private:
    struct GPU_Tri {
        Vec4 v0, v1, v2;
    };
    struct Threaded_Node {
        Vec4 bmin;
        Vec4 bmax;
        int start, size, hit, miss;
    };
    struct Stack_Node {
        Vec4 bmin;
        Vec4 bmax;
        int l, r, start, size;
    };
    struct Constants {
        int n_nodes = 0;
        int n_tris = 0;
        int start = 0;
        int trace_rays = 0;
        int stackless = 0;
        int sort_children = 0;
    };

    std::vector<Vec4> run_threaded(const BVH& bvh, const std::vector<Vec4>& queries, bool rays);
    std::vector<Vec4> run_brute(const BVH& bvh, const std::vector<Vec4>& queries, bool rays);
    std::vector<Vec4> run_stack(const BVH& bvh, const std::vector<Vec4>& queries, bool rays,
                                bool stackless);

    VkWriteDescriptorSet write_buf(const Buffer& buf, const PipeData& pipe, int bind);
    std::array<VkDescriptorBufferInfo, 16> buf_infos;

    void create_pipe();
    void create_desc();
};

} // namespace VK
