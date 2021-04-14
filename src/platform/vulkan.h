
#pragma once

#include <SDL2/SDL.h>
#include <lib/lib.h>
#include <vulkan/vulkan.h>

namespace VK {

static constexpr char vk_name[] = "Vulkan";
using alloc = Mallocator<vk_name>;

// For rendering quad, move to abstraction
struct Vertex {
    Vec3 pos;
    Vec3 color;
    Vec2 tex_coord;
    static VkVertexInputBindingDescription bind_desc();
    static Array<VkVertexInputAttributeDescription, 3> attr_descs();
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

    Vec<VkPresentModeKHR, alloc> modes;
    Vec<VkSurfaceFormatKHR, alloc> fmts;
    Vec<VkExtensionProperties, alloc> exts;
    Vec<VkQueueFamilyProperties, alloc> queue_families;

    i32 graphics_idx = 0, present_idx = 0;
    bool supports(const Vec<const char*, alloc>& extensions);
};

struct Info {
    VkInstance instance = {};
    Vec<VkExtensionProperties, alloc> extensions;
    Vec<const char*, alloc> inst_ext, dev_ext, layers;
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
    Vec<Swapchain_Image, alloc> images;

    f32 aspect_ratio();
    Pair<u32,u32> dim();
};

struct Frame {
    VkFence fence;
    VkSemaphore avail, finish;
    VkCommandBuffer primary;
    Vec<VkCommandBuffer, alloc> secondary;
    static constexpr u32 MAX_IN_FLIGHT = 2;
};

using Frames = Vec<Frame, alloc>;

struct Current_GPU {
    GPU* data = null;
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
    Pair<VkBuffer, VkDeviceMemory> vertex_buffer, index_buffer;
    Pair<VkImage, VkDeviceMemory> texture;
    VkImageView texture_view;
    VkSampler texture_sampler;

    // per frame in flight
    Vec<VkDescriptorSet, alloc> descriptor_sets;
    Vec<Pair<VkBuffer, VkDeviceMemory>, alloc> uniform_buffers;
    // basic pipeline for rendering quad
    VkPipeline graphics_pipeline;
    VkPipelineLayout pipeline_layout;
    Pair<VkImage, VkDeviceMemory> depth_image;
    VkImageView depth_view;

    u32 current_img = 0, current_frame = 0;
    bool needs_resize = false, minimized = false;

    Info info;
    Frames frames;
    Swapchain swapchain;
    VkCommandPool command_pool = {};
    VkDescriptorPool descriptor_pool = {};
    VkRenderPass output_pass;

    Current_GPU gpu;
    Vec<GPU, alloc> gpus;

    SDL_Window* window = null;

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
    void submit_frame(Vec<VkCommandBuffer, alloc>&& buffers);

    VkShaderModule create_shader(const Vec<u8>& data);
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    Pair<VkBuffer, VkDeviceMemory> create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    void buffer_to_image(VkBuffer buffer, VkImage image, u32 w, u32 h);
    VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect);
    void transition_image(VkImage image, VkFormat format, VkImageLayout old_l, VkImageLayout new_l);
    Pair<VkImage, VkDeviceMemory> create_image(u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

    VkFormat find_depth_format();
    bool format_has_stencil(VkFormat format);

    VkExtent2D choose_surface_extent();
    u32 choose_memory_type(u32 filter, VkMemoryPropertyFlags properties);
    VkFormat choose_supported_format(const Vec<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags features);
};

} // namespace VK
