
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
    Vec3 env = Vec3{1.0f};
    float env_scale = 0.0f;
    bool use_normal_map = false;
    bool use_rr = true;
    bool use_metalness = false;
    bool use_qmc = false;
    bool reset_res = false;
    bool use_temporal = true;
    int integrator = 0;
    int brdf = 0;
    int debug_view = 0;
    int res_samples = 4;

    Drop<Image> pos_image[2], norm_image[2], alb_image[2];
    Drop<ImageView> pos_image_view[2], norm_image_view[2], alb_image_view[2];

private:
    struct alignas(16) Reservoir {
        Vec4 pos;
	    Vec4 normal;
        Vec4 emissive;
	    float w_sum;
        float w;
	    unsigned int n_seen;
    };
    struct alignas(16) Scene_Desc {
        Mat4 model;
        Mat4 modelIT;
        Vec4 albedo;
        Vec4 emissive;
        Vec4 metal_rough;
        int albedo_tex;
        int emissive_tex;
        int metal_rough_tex;
        int normal_tex;
        unsigned int index;
    };
    struct alignas(16) Scene_Light {
        Vec4 bmin;
        Vec4 bmax;
        unsigned int index;
        unsigned int n_triangles;
    };
    struct RTPipe_Constants {
        Vec4 clear_col;
        Vec4 env_light;
        int frame = -1;
        int samples;
        int max_frame;
        int qmc;
        int max_depth;
        int use_normal_map;
        int use_metalness;
        int reset_res;
        int integrator;
        int brdf;
        int debug_view;
        int use_rr;
        int n_lights;
        int n_objs;
    };
    
    struct ReSTIRConstants {
        Mat4 prev_PV;
        unsigned int new_samples;
        unsigned int temporal_multiplier;
    };

    struct CameraConstants {
        Mat4 V, P, iV, iP;
    };

    struct UBO {
        CameraConstants camera;
        ReSTIRConstants restir;
    };

    std::vector<Drop<Buffer>> ubos;
    
    Drop<Buffer> sbt;
    Drop<Buffer> desc_buf, light_buf;
    Drop<Buffer> res0, res1;

    Drop<Sampler> gbuf_sampler;

    std::vector<Drop<Image>> textures;
    std::vector<Drop<ImageView>> texture_views;
    Drop<Sampler> texture_sampler;

    RTPipe_Constants consts;
    CameraConstants old_cam = {};
    VkExtent2D prev_ext = {};

    void create_sbt();
    void create_pipe();
    void resize_temporal_stuff();
    void bind_temporal_stuff(VkCommandBuffer cmds);
    void create_desc(const Scene& scene);
    void build_desc(const Scene& scene);
    void build_textures(const Scene& scene);
};

} // namespace VK
