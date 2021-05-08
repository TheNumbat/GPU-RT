
#pragma once

#include <lib/mathlib.h>
#include <util/camera.h>
#include <vector>

#include "vulkan.h"

class Scene;

namespace VK {

struct RTPipe {

    RTPipe() = default;
    RTPipe(const Scene& scene);
    ~RTPipe();

    RTPipe(const RTPipe&) = delete;
    RTPipe(RTPipe&& src) = default;
    RTPipe& operator=(const RTPipe&) = delete;
    RTPipe& operator=(RTPipe&& src) = default;

    void recreate(const Scene& scene);
    void destroy();

    void recreate_swap(const Scene& scene);
    void update_uniforms(const Camera& cam);
    void use_accel(const Accel& tlas);
    void use_image(const ImageView& out);
    void reset_frame();

    bool trace(const Camera& cam, VkCommandBuffer& cmds, VkExtent2D ext);

    Drop<PipeData> pipe;

    int max_frames = 256;
    int samples_per_frame = 8;
    int max_depth = 8;
    Vec3 clear = Vec3{0.3f};
    Vec3 env = Vec3{0.1f};
    float env_scale = 1.f;

private:
    struct alignas(16) Scene_Desc {
        Mat4 model;
        Mat4 modelIT;
        Vec4 albedo;
        Vec4 emissive;
        Vec4 metal_rough;
        int albedo_tex;
        int emissive_tex;
        int metal_rough_tex;
        unsigned int index;
    };
    struct RTPipe_Constants {
        Vec4 clearColor;
        Vec4 envlight;
        int frame = -1;
        int samples;
        int depth;
    };

    std::vector<Drop<Buffer>> camera_uniforms;
    
    Drop<Buffer> sbt;
    Drop<Buffer> desc_buf;

    std::vector<VK::Drop<VK::Image>> textures;
    std::vector<VK::Drop<VK::ImageView>> texture_views;
    VK::Drop<VK::Sampler> texture_sampler;

    RTPipe_Constants consts;
    Cam_Uniforms old_cam;

    void create_sbt();
    void create_pipe();
    void create_desc(const Scene& scene);
    void build_desc(const Scene& scene);
    void build_textures(const Scene& scene);
};

} // namespace VK