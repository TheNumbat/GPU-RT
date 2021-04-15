
#pragma once

#include <lib/mathlib.h>
#include <util/camera.h>
#include <vector>

#include "vulkan.h"

namespace VK {

struct Mesh {
    typedef unsigned int Index;
    struct Vertex {
        Vec3 pos;
        Vec3 norm;
        Vec2 tex_coord;

        static VkVertexInputBindingDescription bind_desc();
        static std::array<VkVertexInputAttributeDescription, 3> attr_descs();
    };

    Mesh() = default;
    Mesh(std::vector<Vertex>&& vertices, std::vector<Index>&& indices);
    Mesh(const Mesh& src) = delete;
    Mesh(Mesh&& src);
    ~Mesh() = default;

    Mesh& operator=(const Mesh& src) = delete;
    Mesh& operator=(Mesh&& src);

    void recreate(std::vector<Vertex>&& vertices, std::vector<Index>&& indices);
    void render(const Pipeline& pipe, VkCommandBuffer cmds) const;
    void sync() const;

private:
    std::vector<Vertex> _verts;
    std::vector<Index> _idxs;
    mutable Ref<Buffer> vbuf, ibuf;
    mutable bool dirty = false;
};

struct Uniforms {
    alignas(16) Mat4 M, V, P;
};

struct Pipeline {

    // TODO: finish extracting this from Manager
    // Manager needs its own pipeline for final compositing
    // and this one should be more abstract...

    void init();
    void destroy();
    void destroy_swap();

    VkPipeline graphics_pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_layout;
    std::vector<Buffer> uniform_buffers;
    std::vector<VkDescriptorSet> descriptor_sets;
    Image depth_image;
    ImageView depth_view;
    void update_uniforms(const Camera& cam);
    void create_pipeline();
    void create_depth_buf();
    void create_uniform_buffers();
    void create_descriptor_sets();
    void create_descriptor_set_layout();
};

} // namespace VK
