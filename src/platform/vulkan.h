
#pragma once

#include <vector>
#include <array>

#include <SDL2/SDL.h>
#include <lib/mathlib.h>
#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

namespace VK {

class Mesh {
public:
    typedef unsigned int Index;
    struct Vert {
        Vec3 pos;
        Vec3 norm;
    };

    Mesh() = default;
    Mesh(std::vector<Vert>&& vertices, std::vector<Index>&& indices)
        : _verts(std::move(vertices)), _idxs(std::move(indices)) {
    }
    Mesh(const Mesh& src) = delete;
    Mesh(Mesh&& src) = default;
    ~Mesh() = default;

    Mesh& operator=(const Mesh& src) = delete;
    Mesh& operator=(Mesh&& src) = default;

private:
    std::vector<Vert> _verts;
    std::vector<Index> _idxs;
};

// For rendering quad, move to abstraction
struct Vertex {
    Vec3 pos;
    Vec3 color;
    Vec2 tex_coord;
    static VkVertexInputBindingDescription bind_desc();
    static std::array<VkVertexInputAttributeDescription, 3> attr_descs();
};

struct Uniforms {
    alignas(16) Mat4 M, V, P;
};

struct GPU {
    VkPhysicalDevice device = {};
    VkPhysicalDeviceFeatures features = {};
    VkSurfaceCapabilitiesKHR surf_caps = {};
    VkPhysicalDeviceProperties dev_prop = {};
    VkPhysicalDeviceMemoryProperties mem_prop = {};

    std::vector<VkPresentModeKHR> modes;
    std::vector<VkSurfaceFormatKHR> fmts;
    std::vector<VkExtensionProperties> exts;
    std::vector<VkQueueFamilyProperties> queue_families;

    int graphics_idx = 0, present_idx = 0;
    bool supports(const std::vector<const char*>& extensions);
};

struct Info {
    VkInstance instance = {};
    std::vector<VkExtensionProperties> extensions;
    std::vector<const char*> inst_ext, dev_ext, layers;
    VkDebugUtilsMessengerEXT debug_callback_info = {};
};

struct Swapchain_Image {
    VkImage image;
    VkFence frame_fence;
    VkImageView view;
    VkFramebuffer framebuffer;
};

struct Swapchain {
    VkExtent2D extent;
    VkFormat depth_format;
    VkSurfaceKHR surface = {};
    VkSwapchainKHR swapchain = {};
    VkSurfaceFormatKHR format = {};
    VkPresentModeKHR present_mode = {};
    std::vector<Swapchain_Image> images;

    float aspect_ratio();
    std::pair<unsigned int, unsigned int> dim();
};

struct Frame {
    VkFence fence;
    VkSemaphore avail, finish;
    VkCommandBuffer primary;
    std::vector<VkCommandBuffer> secondary;
    static constexpr unsigned int MAX_IN_FLIGHT = 2;
};

using Frames = std::vector<Frame>;

struct Current_GPU {
    GPU* data = nullptr;
    VkDevice device = {};
    VkQueue graphics_queue = {}, present_queue = {};
};

struct Manager {

    void init(SDL_Window* window);
    void destroy();

    void begin_frame();
    void end_frame();

    void trigger_resize();

private:
    // move this into real abstractions lole
    // for rendering quad
    VkDescriptorSetLayout descriptor_layout;
    std::pair<VkBuffer,VmaAllocation> vertex_buffer, index_buffer;
    std::pair<VkImage,VmaAllocation> texture;
    VkImageView texture_view;
    VkSampler texture_sampler;

    VmaAllocator gpu_alloc;

    // per frame in flight
    std::vector<VkDescriptorSet> descriptor_sets;
    std::vector<std::pair<VkBuffer, VmaAllocation>> uniform_buffers;
    // basic pipeline for rendering quad
    VkPipeline graphics_pipeline;
    VkPipelineLayout pipeline_layout;
    std::pair<VkImage, VmaAllocation> depth_image;
    VkImageView depth_view;

    unsigned int current_img = 0, current_frame = 0;
    bool needs_resize = false, minimized = false;

    Info info;
    Frames frames;
    Swapchain swapchain;
    VkCommandPool command_pool = {};
    VkDescriptorPool descriptor_pool = {};
    VkRenderPass output_pass;

    Current_GPU gpu;
    std::vector<GPU> gpus;

    SDL_Window* window = nullptr;

    void select_gpu();
    void enumerate_gpus();
    void create_instance();

    void update_uniforms();
    void destroy_swapchain();
    void recreate_swapchain();

    void init_debug_callback();
    void destroy_debug_callback();

    void create_texture();
    void create_pipeline();
    void create_depth_buf();
    void create_data_buffers();
    void create_uniform_buffers();
    void create_descriptor_sets();
    void create_descriptor_set_layout();
    void create_texture_view_and_sampler();

    void create_frames();
    void create_gpu_alloc();
    void create_swapchain();
    void create_output_pass();
    void create_framebuffers();
    void create_command_pool();
    void create_descriptor_pool();
    void create_logical_device_and_queues();

    void init_imgui();
    VkCommandBuffer render_imgui();
    VkCommandBuffer render_square();

    VkCommandBuffer allocate_buf_ots();
    VkCommandBuffer allocate_buf_secondary();
    void run_wait_free_buf(VkCommandBuffer cmds);
    void submit_frame(std::vector<VkCommandBuffer>&& buffers);

    VkShaderModule create_shader(const std::vector<unsigned char>& data);
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    std::pair<VkBuffer, VmaAllocation> create_buffer(VkDeviceSize size, VkBufferUsageFlags buf_usage,
                                                     VmaMemoryUsage mem_usage);

    void write_gpu(VmaAllocation buffer, const void* data, size_t size);

    void buffer_to_image(VkBuffer buffer, VkImage image, unsigned int w, unsigned int h);
    VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect);
    void transition_image(VkImage image, VkFormat format, VkImageLayout old_l, VkImageLayout new_l);
    std::pair<VkImage, VmaAllocation> create_image(unsigned int width, unsigned int height,
                                                    VkFormat format, VkImageTiling tiling,
                                                    VkImageUsageFlags img_usage,
                                                    VmaMemoryUsage mem_usage);

    VkFormat find_depth_format();
    bool format_has_stencil(VkFormat format);

    VkExtent2D choose_surface_extent();
    unsigned int choose_memory_type(unsigned int filter, VkMemoryPropertyFlags properties);
    VkFormat choose_supported_format(const std::vector<VkFormat>& formats, VkImageTiling tiling,
                                     VkFormatFeatureFlags features);
};

} // namespace VK
