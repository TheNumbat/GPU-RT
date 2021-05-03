
#pragma once

#include <vector>

#include <lib/mathlib.h>
#include <scene/scene.h>
#include <scene/bvh.h>
#include <util/camera.h>

#include "render.h"
#include "vulkan.h"

class BVH;

namespace VK {

constexpr size_t BRUTE_MAX = 10000;

enum class BVH_Type { none, threaded, stack, stackless, wide, wide_max, wide_sort, RTX, count };

struct BVHPipe {

    BVHPipe() = default;
    ~BVHPipe();

    BVHPipe(const BVHPipe&) = delete;
    BVHPipe(BVHPipe&& src) = default;
    BVHPipe& operator=(const BVHPipe&) = delete;
    BVHPipe& operator=(BVHPipe&& src) = default;

    void recreate();
    void destroy();

    void build(const Mesh& mesh, int leaf_size);
    BBox box();

    std::vector<Vec4> cpqs(BVH_Type type, const std::vector<Vec4>& queries, std::chrono::milliseconds& time, int w = 1);
    std::vector<Vec4> rays(BVH_Type type, const std::vector<std::pair<Vec4, Vec4>>& queries, std::chrono::milliseconds& time, int w = 1);

    Drop<PipeData> threaded_pipe;
    Drop<PipeData> brute_pipe;
    Drop<PipeData> stack_pipe;
    Drop<PipeData> rt_pipe;

    static constexpr int WIDE_PIPES = 4;
    std::array<Drop<PipeData>, WIDE_PIPES> wide_pipe;

private:
    Drop<Buffer> sbt;
    
    BVH bvh;
    WBVH<1> wbvh1;
    WBVH<2> wbvh2;
    WBVH<3> wbvh3;
    WBVH<4> wbvh4;

    std::vector<Drop<Accel>> BLAS;
    Drop<Accel> TLAS;

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

    std::vector<Vec4> run_threaded(const std::vector<Vec4>& queries, bool rays, std::chrono::milliseconds& time);
    std::vector<Vec4> run_brute(const std::vector<Vec4>& queries, bool rays, std::chrono::milliseconds& time);
    std::vector<Vec4> run_stack(const std::vector<Vec4>& queries, bool rays, bool stackless, std::chrono::milliseconds& time);
    std::vector<Vec4> run_rtx(const std::vector<Vec4>& queries, std::chrono::milliseconds& time);
    std::vector<Vec4> run_wide(const std::vector<Vec4>& queries, bool rays, int sort, std::chrono::milliseconds& time, int w);

    std::pair<void*,size_t> pick_wide_bvh(int w);

    VkWriteDescriptorSet write_buf(const Buffer& buf, const PipeData& pipe, int bind);
    std::array<VkDescriptorBufferInfo, 16> buf_infos;

    void create_pipe();
    void create_desc();
};

} // namespace VK
