
#pragma once

#include <array>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <variant>
#include <vector>

#include <SDL2/SDL.h>
#include <lib/mathlib.h>
#include <util/image.h>
#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

// porting to exile2
//      swap back to custom lib
//          vector, map, maybe
//      add quat/camera to lib
//      literals instead of strings
//      single % in logging
//      use u32, u8, etc.

#define VK_CHECK(f)                                                                                \
    do {                                                                                           \
        VkResult res = (f);                                                                        \
        if(res != VK_SUCCESS) {                                                                    \
            DEBUG_BREAK;                                                                           \
            die("VK_CHECK: %s", vk_err_str(res).c_str());                                          \
        }                                                                                          \
    } while(0)

namespace VK {

inline unsigned int align_up(unsigned int v, unsigned int a) {
    return (v + a - 1) & ~(a - 1);
}

class Manager;

struct Buffer;
struct Image;
struct ImageView;
struct Sampler;
struct Shader;
struct Framebuffer;
struct Accel;
template<typename T> struct Drop;

struct Mesh;
struct MeshPipe;

Manager& vk();
std::string vk_err_str(VkResult errorCode);

struct Buffer {

    Buffer() = default;
    Buffer(VkDeviceSize size, VkBufferUsageFlags buf_usage, VmaMemoryUsage mem_usage);
    Buffer(const Buffer&) = delete;
    Buffer(Buffer&& src);
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&& src);
    ~Buffer();

    void recreate(VkDeviceSize size, VkBufferUsageFlags buf_usage, VmaMemoryUsage mem_usage);
    void destroy();

    VkDeviceAddress address() const;
    void* map() const;
    void unmap() const;

    void copy_to(const Buffer& dst);
    void write(const void* data, size_t size);
    void read(void* data, size_t size);
    void write_staged(const void* data, size_t dsize);

    void to_image(VkCommandBuffer& cmds, const Image& image);

    VkBuffer buf = VK_NULL_HANDLE;

private:
    VkDeviceSize size = 0;
    VkBufferUsageFlags buf_usage = {};
    VmaMemoryUsage mem_usage = {};
    VmaAllocation mem = VK_NULL_HANDLE;
};

struct Image {

    Image() = default;
    Image(unsigned int width, unsigned int height, VkFormat format, VkImageTiling tiling,
          VkImageUsageFlags img_usage, VmaMemoryUsage mem_usage);
    ~Image();

    Image(const Image&) = delete;
    Image(Image&& src);
    Image& operator=(const Image&) = delete;
    Image& operator=(Image&& src);

    void recreate(unsigned int width, unsigned int height, VkFormat format, VkImageTiling tiling,
                  VkImageUsageFlags img_usage, VmaMemoryUsage mem_usage);
    void destroy();

    void transition(VkImageLayout new_l);
    void transition(VkCommandBuffer& cmds, VkImageLayout new_l);
    void transition(VkCommandBuffer& cmds, VkImageLayout old_l, VkImageLayout new_l);

    void write(Util::Image& img);

    VkImage img = VK_NULL_HANDLE;
    unsigned int w = 0;
    unsigned int h = 0;
    VkFormat format = {};
    VkImageLayout layout = {};

private:
    VkImageTiling tiling = {};
    VkImageUsageFlags img_usage = {};
    VmaMemoryUsage mem_usage = {};
    VmaAllocation mem = VK_NULL_HANDLE;

    friend struct Buffer;
    friend struct ImageView;
};

struct ImageView {

    ImageView() = default;
    ImageView(Image& image, VkImageAspectFlags aspect);
    ~ImageView();

    ImageView(const ImageView&) = delete;
    ImageView(ImageView&& src);
    ImageView& operator=(const ImageView&) = delete;
    ImageView& operator=(ImageView&& src);

    void recreate(Image& image, VkImageAspectFlags aspect);
    void recreate(VkImage image, VkFormat format, VkImageAspectFlags aspect);
    void destroy();

    const Image& img() const {
        assert(image);
        return *image;
    }
    Image& img() {
        assert(image);
        return *image;
    }

    VkImageView view = VK_NULL_HANDLE;

private:
    Image* image = nullptr;
    VkImageAspectFlags aspect = {};
};

struct Sampler {

    Sampler() = default;
    Sampler(VkFilter min, VkFilter mag);
    ~Sampler();

    Sampler(const Sampler&) = delete;
    Sampler(Sampler&& src);
    Sampler& operator=(const Sampler&) = delete;
    Sampler& operator=(Sampler&& src);

    void recreate(VkFilter min, VkFilter mag);
    void destroy();

    VkSampler sampler = VK_NULL_HANDLE;
};

struct Shader {

    Shader() = default;
    Shader(const std::vector<unsigned char>& data);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader(Shader&& src);
    Shader& operator=(const Shader&) = delete;
    Shader& operator=(Shader&& src);

    void recreate(const std::vector<unsigned char>& data);
    void destroy();

    VkShaderModule shader = VK_NULL_HANDLE;
};

struct Pass {

    struct Subpass {
        VkPipelineBindPoint bind = {};
        VkAttachmentReference depth = {};
        std::vector<VkAttachmentReference> color;
    };
    struct Info {
        std::vector<VkAttachmentDescription> attachments;
        std::vector<Subpass> subpasses;
        std::vector<VkSubpassDependency> dependencies;
    };

    Pass() = default;
    Pass(const Info& info);
    ~Pass();

    Pass(const Pass&) = delete;
    Pass(Pass&& src);
    Pass& operator=(const Pass&) = delete;
    Pass& operator=(Pass&& src);

    void begin(VkCommandBuffer& cmds, Framebuffer& fb, const std::vector<VkClearValue>& clears);
    void end(VkCommandBuffer& cmds);

    void recreate(const Info& info);
    void destroy();

    VkRenderPass pass = {};
};

struct Framebuffer {

    Framebuffer() = default;
    Framebuffer(unsigned int width, unsigned int height, const Pass& pass,
                const std::vector<std::reference_wrapper<ImageView>>& views);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& src);
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer& operator=(Framebuffer&& src);

    void recreate(unsigned int width, unsigned int height, const Pass& pass,
                  const std::vector<std::reference_wrapper<ImageView>>& views);
    void destroy();

    unsigned int w = 0;
    unsigned int h = 0;
    VkFramebuffer buf = VK_NULL_HANDLE;

private:
    void recreate(unsigned int width, unsigned int height, VkRenderPass pass,
                  const std::vector<std::reference_wrapper<ImageView>>& views);
    friend class Manager;
};

struct Accel {

    Accel() = default;
    Accel(const Mesh& mesh);
    Accel(const std::vector<Drop<Accel>>& blas, const std::vector<Mat4>& inst);
    ~Accel();

    Accel(const Accel&) = delete;
    Accel(Accel&& src);
    Accel& operator=(const Accel&) = delete;
    Accel& operator=(Accel&& src);

    void recreate(const Mesh& mesh);
    void recreate(const std::vector<Drop<Accel>>& blas, const std::vector<Mat4>& inst);
    void recreate(const Drop<Accel>& blas, Mat4 inst);
    void destroy();

    VkAccelerationStructureKHR accel = {};

private:
    void create_and_build(VkAccelerationStructureCreateInfoKHR create_info,
                          VkAccelerationStructureBuildGeometryInfoKHR build_info,
                          VkAccelerationStructureBuildRangeInfoKHR offset);

    Buffer abuf;
    Buffer ibuf;
    VkBuildAccelerationStructureFlagsKHR flags = {};
    VkAccelerationStructureBuildSizesInfoKHR size = {};
};

struct PipeData {

    PipeData() = default;
    ~PipeData();

    PipeData(const PipeData&) = delete;
    PipeData(PipeData&& src);
    PipeData& operator=(const PipeData&) = delete;
    PipeData& operator=(PipeData&& src);

    void destroy_swap();
    void destroy();

    VkPipeline pipe = {};
    VkPipelineLayout p_layout = {};
    VkDescriptorSetLayout d_layout = {};
    std::vector<VkDescriptorSet> descriptor_sets;
};

template<typename T> struct Drop {

    Drop() = default;
    Drop(T&& resource) : resource(std::move(resource)) {
    }
    ~Drop();

    Drop(const Drop&) = delete;
    Drop(Drop&& src) = default;
    Drop& operator=(const Drop&) = delete;
    Drop& operator=(Drop&& src) = default;

    T* operator->() {
        return &resource;
    }
    const T* operator->() const {
        return &resource;
    }

    operator T&() {
        return resource;
    }
    operator T const &() const {
        return resource;
    }

    void drop();

private:
    T resource;
    friend class Manager;
};

class Manager {
public:
    void init(SDL_Window* window);
    void destroy();

    bool begin_frame();
    void end_frame(ImageView& img);

    void trigger_resize();

    VkCommandBuffer begin();
    VkCommandBuffer begin_one_time();
    void end_one_time(VkCommandBuffer cmds);

    unsigned int frame() const;

    template<typename T> void drop(T&& resource) {
        std::lock_guard<std::mutex> lock(erase_mut[current_frame]);
        erase_queue[current_frame].push_back({std::move(resource)});
    }

    VkDevice device() const {
        return gpu.device;
    }

    VkDescriptorPool pool() const {
        return descriptor_pool;
    }

    VkExtent2D extent() const {
        return swapchain.extent;
    }

    template<typename F> void on_resize(F&& f) {
        resize_callbacks.push_back(std::forward<F&&>(f));
    }

    VkFormat find_depth_format();

    static constexpr unsigned int MAX_IN_FLIGHT = 2;

    struct RTX {
        PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2;
        PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
        PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
        PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
        PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
        PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
        PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
        PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties = {};
    };
    RTX rtx;

private:
    Manager() = default;
    ~Manager() = default;

    struct Info {
        VkInstance instance = {};
        std::vector<VkExtensionProperties> extensions;
        std::vector<const char*> inst_ext, dev_ext, layers;
        VkDebugUtilsMessengerEXT debug_callback_info = {};
    };

    struct GPU {
        VkPhysicalDevice device = {};
        VkPhysicalDeviceFeatures2 features = {};
        VkPhysicalDeviceBufferDeviceAddressFeatures addr_features = {};
        VkSurfaceCapabilitiesKHR surf_caps = {};
        VkPhysicalDeviceProperties dev_prop = {};
        VkPhysicalDeviceMemoryProperties mem_prop = {};
        VkPhysicalDeviceProperties2KHR prop_2 = {};

        std::vector<VkPresentModeKHR> modes;
        std::vector<VkSurfaceFormatKHR> fmts;
        std::vector<VkExtensionProperties> exts;
        std::vector<VkQueueFamilyProperties> queue_families;

        int graphics_idx = 0, present_idx = 0;
        bool supports(const std::vector<const char*>& extensions);
    };

    struct Swapchain_Slot {
        VkImage image;
        VkFence frame_fence;
        ImageView view;
    };

    struct Swapchain {
        VkExtent2D extent;
        VkFormat depth_format;
        VkSurfaceKHR surface = {};
        VkSwapchainKHR swapchain = {};
        VkSurfaceFormatKHR format = {};
        VkPresentModeKHR present_mode = {};
        std::vector<Swapchain_Slot> slots;

        float aspect_ratio();
        std::pair<unsigned int, unsigned int> dim();
    };

    struct Frame {
        VkFence fence;
        VkSemaphore avail, finish;
        VkCommandBuffer composite;
        std::vector<VkCommandBuffer> buffers;
    };

    using Frames = std::vector<Frame>;

    struct Current_GPU {
        GPU* data = nullptr;
        VkDevice device = {};
        VkQueue graphics_queue = {}, present_queue = {};
    };

    struct Compositor {

        void init();
        void destroy();
        void create_swap();
        void destroy_swap();

        void composite(VkCommandBuffer& cmds, ImageView& img);
        VkRenderPass get_pass() {
            return pass;
        }

    private:
        Sampler sampler;
        VkRenderPass pass;
        VkPipeline pipeline;
        VkPipelineLayout p_layout;
        VkDescriptorSetLayout d_layout;

        std::vector<VkDescriptorSet> descriptor_sets;
        std::vector<Framebuffer> framebuffers;

        VkRenderPassBeginInfo pass_info();
        void update_img(const ImageView& view);
        void create_pipe();
        void create_pass();
        void create_fbs();
        void create_desc();
    };

    Info info;
    Frames frames;
    Swapchain swapchain;
    Compositor compositor;

    VkCommandPool command_pool = {};
    VkDescriptorPool descriptor_pool = {};

    unsigned int current_img = 0, current_frame = 0;
    bool needs_resize = false, minimized = false;

    Current_GPU gpu;
    std::vector<GPU> gpus;
    VmaAllocator gpu_alloc;
    SDL_Window* window = nullptr;

    void select_gpu();
    void enumerate_gpus();
    void create_instance();

    void destroy_swapchain();
    void recreate_swapchain();

    void init_debug_callback();
    void destroy_debug_callback();
    void init_rt();

    void create_frames();
    void create_pipeline();
    void create_gpu_alloc();
    void create_swapchain();
    void create_output_pass();
    void create_framebuffers();
    void create_command_pool();
    void create_descriptor_pool();
    void create_logical_device_and_queues();

    void init_imgui();
    VkCommandBuffer render_imgui();
    void submit_frame(ImageView& img);

    VkExtent2D choose_surface_extent();
    unsigned int choose_memory_type(unsigned int filter, VkMemoryPropertyFlags properties);
    VkFormat choose_supported_format(const std::vector<VkFormat>& formats, VkImageTiling tiling,
                                     VkFormatFeatureFlags features);

    friend struct Buffer;
    friend struct Image;
    friend struct ImageView;
    friend struct Shader;
    friend struct Framebuffer;
    friend struct Sampler;
    friend struct Accel;
    template<typename T> friend struct Drop;

    // TODO: REMOVE
    friend struct Pipeline;

    using Resource =
        std::variant<Buffer, Image, ImageView, Shader, Framebuffer, Sampler, Accel, Pass, PipeData>;
    std::vector<Resource> erase_queue[MAX_IN_FLIGHT];
    std::mutex erase_mut[MAX_IN_FLIGHT];

    std::vector<std::function<void()>> resize_callbacks;

    void do_erase() {
        std::lock_guard<std::mutex> lock(erase_mut[current_frame]);
        erase_queue[current_frame].clear();
    }

    friend Manager& vk();
};

template<typename T> Drop<T>::~Drop() {
    drop();
}
template<typename T> void Drop<T>::drop() {
    vk().drop(std::move(resource));
}

} // namespace VK
