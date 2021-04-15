
#include "vulkan.h"
#include "imgui_impl_vulkan.h"

#include <SDL2/SDL_vulkan.h>
#include <lib/log.h>
#include <util/files.h>
#include <util/image.h>

#define VMA_IMPLEMENTATION
#include "render.h"
#include "vk_mem_alloc.h"

namespace VK {

Manager& get() {
    static Manager singleton;
    return singleton;
}

std::string vk_err_str(VkResult errorCode) {
    switch(errorCode) {
#define STR(r)                                                                                     \
    case VK_##r: return #r
        STR(NOT_READY);
        STR(TIMEOUT);
        STR(EVENT_SET);
        STR(EVENT_RESET);
        STR(INCOMPLETE);
        STR(ERROR_OUT_OF_HOST_MEMORY);
        STR(ERROR_OUT_OF_DEVICE_MEMORY);
        STR(ERROR_INITIALIZATION_FAILED);
        STR(ERROR_DEVICE_LOST);
        STR(ERROR_MEMORY_MAP_FAILED);
        STR(ERROR_LAYER_NOT_PRESENT);
        STR(ERROR_EXTENSION_NOT_PRESENT);
        STR(ERROR_FEATURE_NOT_PRESENT);
        STR(ERROR_INCOMPATIBLE_DRIVER);
        STR(ERROR_TOO_MANY_OBJECTS);
        STR(ERROR_FORMAT_NOT_SUPPORTED);
        STR(ERROR_SURFACE_LOST_KHR);
        STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(SUBOPTIMAL_KHR);
        STR(ERROR_OUT_OF_DATE_KHR);
        STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(ERROR_VALIDATION_FAILED_EXT);
        STR(ERROR_INVALID_SHADER_NV);
#undef STR
    default: return "UNKNOWN_ERROR";
    }
}

static std::string vk_obj_type(VkObjectType type) {
    switch(type) {
#define STR(r)                                                                                     \
    case VK_OBJECT_TYPE_##r: return #r
        STR(UNKNOWN);
        STR(INSTANCE);
        STR(PHYSICAL_DEVICE);
        STR(DEVICE);
        STR(QUEUE);
        STR(SEMAPHORE);
        STR(COMMAND_BUFFER);
        STR(FENCE);
        STR(DEVICE_MEMORY);
        STR(BUFFER);
        STR(IMAGE);
        STR(EVENT);
        STR(QUERY_POOL);
        STR(BUFFER_VIEW);
        STR(IMAGE_VIEW);
        STR(SHADER_MODULE);
        STR(PIPELINE_CACHE);
        STR(PIPELINE_LAYOUT);
        STR(RENDER_PASS);
        STR(PIPELINE);
        STR(DESCRIPTOR_SET_LAYOUT);
        STR(SAMPLER);
        STR(DESCRIPTOR_POOL);
        STR(DESCRIPTOR_SET);
        STR(FRAMEBUFFER);
        STR(COMMAND_POOL);
        STR(SAMPLER_YCBCR_CONVERSION);
        STR(DESCRIPTOR_UPDATE_TEMPLATE);
        STR(SURFACE_KHR);
        STR(SWAPCHAIN_KHR);
        STR(DISPLAY_KHR);
        STR(DISPLAY_MODE_KHR);
        STR(DEBUG_REPORT_CALLBACK_EXT);
        STR(DEBUG_UTILS_MESSENGER_EXT);
        STR(VALIDATION_CACHE_EXT);
#undef STR
    default: return "UNKNOWN_ERROR";
    }
}

static void vk_check_fn(VkResult result) {
    VK_CHECK(result);
}

static bool format_has_stencil(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

Buffer::~Buffer() {
    destroy();
}

Buffer::Buffer(Buffer&& src) {
    *this = std::move(src);
}

Buffer& Buffer::operator=(Buffer&& src) {
    destroy();
    std::memcpy(this, &src, sizeof(Buffer));
    std::memset(&src, 0, sizeof(Buffer));
    return *this;
}

Buffer::Buffer(VkDeviceSize size, VkBufferUsageFlags buf_usage, VmaMemoryUsage mem_usage) {
    recreate(size, buf_usage, mem_usage);
}

void Buffer::destroy() {
    if(buf && mem) vmaDestroyBuffer(get().gpu_alloc, buf, mem);
    std::memset(this, 0, sizeof(Buffer));
}

void Buffer::recreate(VkDeviceSize sz, VkBufferUsageFlags busage, VmaMemoryUsage musage) {

    destroy();
    size = sz;
    buf_usage = busage;
    mem_usage = musage;

    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = size;
    buf_info.usage = buf_usage;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = mem_usage;

    VK_CHECK(vmaCreateBuffer(get().gpu_alloc, &buf_info, &alloc_info, &buf, &mem, nullptr));
}

void Buffer::copy_to(const Buffer& dst) {

    VkCommandBuffer cmds = get().begin_one_time();

    VkBufferCopy region = {};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(cmds, buf, dst.buf, 1, &region);

    get().end_one_time(cmds);
}

void Buffer::to_image(const Image& image) {

    VkCommandBuffer cmds = get().begin_one_time();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {image.w, image.h, 1};

    vkCmdCopyBufferToImage(cmds, buf, image.img, image.layout, 1, &region);

    get().end_one_time(cmds);
}

void Buffer::write(const void* data, size_t dsize) {
    assert(dsize <= size);

    void* map;
    VK_CHECK(vmaMapMemory(get().gpu_alloc, mem, &map));
    std::memcpy(map, data, dsize);
    vmaUnmapMemory(get().gpu_alloc, mem);
}

void Buffer::write_staged(const void* data, size_t dsize) {
    
    assert(dsize <= size);

    Buffer staging(dsize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    staging.write(data, dsize);
    staging.copy_to(*this);
}

Image::~Image() {
    destroy();
}

Image::Image(Image&& src) {
    *this = std::move(src);
}

Image& Image::operator=(Image&& src) {
    destroy();
    std::memcpy(this, &src, sizeof(Image));
    std::memset(&src, 0, sizeof(Image));
    return *this;
}

void Image::destroy() {
    if(img && mem) vmaDestroyImage(get().gpu_alloc, img, mem);
    std::memset(this, 0, sizeof(Image));
}

void Image::recreate(unsigned int width, unsigned int height, VkFormat fmt, VkImageTiling tlg,
                     VkImageUsageFlags iusage, VmaMemoryUsage musage) {
    destroy();

    w = width;
    h = height;
    format = fmt;
    tiling = tlg;
    img_usage = iusage;
    mem_usage = musage;

    VkImageCreateInfo img_info = {};
    img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_info.imageType = VK_IMAGE_TYPE_2D;
    img_info.extent.width = width;
    img_info.extent.height = height;
    img_info.extent.depth = 1;
    img_info.mipLevels = 1;
    img_info.arrayLayers = 1;
    img_info.format = format;
    img_info.tiling = tiling;
    img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_info.usage = img_usage;
    img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    img_info.flags = 0;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = mem_usage;

    VK_CHECK(vmaCreateImage(get().gpu_alloc, &img_info, &alloc_info, &img, &mem, nullptr));
}

void Image::transition(VkImageLayout new_l) {

    VkImageLayout old_l = layout;
    layout = new_l;

    VkCommandBuffer cmds = get().begin_one_time();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_l;
    barrier.newLayout = new_l;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img;

    if(new_l == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if(format_has_stencil(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage, dst_stage;

    if(old_l == VK_IMAGE_LAYOUT_UNDEFINED && new_l == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {

        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    } else if(old_l == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
              new_l == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    } else if(old_l == VK_IMAGE_LAYOUT_UNDEFINED &&
              new_l == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {

        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    } else {
        die("Unsupported image layout transition!");
    }

    vkCmdPipelineBarrier(cmds, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    get().end_one_time(cmds);
}

ImageView::ImageView(const Image& image, VkImageAspectFlags aspect) {
    recreate(image, aspect);
}

ImageView::~ImageView() {
    destroy();
}

ImageView::ImageView(ImageView&& src) {
    *this = std::move(src);
}

ImageView& ImageView::operator=(ImageView&& src) {
    destroy();
    image = src.image;
    aspect = src.aspect;
    view = src.view;
    std::memset(&src, 0, sizeof(ImageView));
    return *this;
}

void ImageView::recreate(const Image& img, VkImageAspectFlags asp) {
    recreate(img.img, img.format, asp);
    image = &img;
}

void ImageView::recreate(VkImage img, VkFormat format, VkImageAspectFlags asp) {
    destroy();
    aspect = asp;

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = img;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_R;
    view_info.components.g = VK_COMPONENT_SWIZZLE_G;
    view_info.components.b = VK_COMPONENT_SWIZZLE_B;
    view_info.components.a = VK_COMPONENT_SWIZZLE_A;
    view_info.subresourceRange.aspectMask = aspect;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(get().gpu.device, &view_info, nullptr, &view));
}

void ImageView::destroy() {
    if(view) vkDestroyImageView(get().gpu.device, view, nullptr);
    std::memset(this, 0, sizeof(ImageView));
}

Sampler::Sampler(VkFilter min, VkFilter mag) {
    recreate(min, mag);
}

Sampler::~Sampler() {
    destroy();
}

Sampler::Sampler(Sampler&& src) {
    *this = std::move(src);
}

Sampler& Sampler::operator=(Sampler&& src) {
    destroy();
    std::memcpy(this, &src, sizeof(Sampler));
    std::memset(&src, 0, sizeof(Sampler));
    return *this;
}

void Sampler::recreate(VkFilter min, VkFilter mag) {
    destroy();

    VkSamplerCreateInfo sample_info = {};
    sample_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sample_info.magFilter = mag;
    sample_info.minFilter = min;
    sample_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sample_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sample_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sample_info.anisotropyEnable = VK_TRUE;
    sample_info.maxAnisotropy = get().gpu.data->dev_prop.limits.maxSamplerAnisotropy;
    sample_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sample_info.unnormalizedCoordinates = VK_FALSE;
    sample_info.compareEnable = VK_FALSE;
    sample_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sample_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sample_info.mipLodBias = 0.0f;
    sample_info.minLod = 0.0f;
    sample_info.maxLod = 0.0f;

    VK_CHECK(vkCreateSampler(get().gpu.device, &sample_info, nullptr, &sampler));
}

void Sampler::destroy() {
    if(sampler) vkDestroySampler(get().gpu.device, sampler, nullptr);
    sampler = VK_NULL_HANDLE;
}

Shader::Shader(const std::vector<unsigned char>& data) {
    recreate(data);
}

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& src) {
    *this = std::move(src);
}

Shader& Shader::operator=(Shader&& src) {
    destroy();
    std::memcpy(this, &src, sizeof(Shader));
    std::memset(&src, 0, sizeof(Shader));
    return *this;
}

void Shader::recreate(const std::vector<unsigned char>& data) {

    destroy();

    VkShaderModuleCreateInfo mod_info = {};
    mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mod_info.codeSize = data.size();
    mod_info.pCode = (const uint32_t*)data.data();

    VK_CHECK(vkCreateShaderModule(get().gpu.device, &mod_info, nullptr, &shader));
}

void Shader::destroy() {
    if(shader) vkDestroyShaderModule(get().gpu.device, shader, nullptr);
    std::memset(this, 0, sizeof(Shader));
}

Framebuffer::Framebuffer(unsigned int width, unsigned int height, VkRenderPass pass,
                         const std::vector<std::reference_wrapper<ImageView>>& views) {
    recreate(width, height, pass, views);
}

Framebuffer::~Framebuffer() {
    destroy();
}

Framebuffer::Framebuffer(Framebuffer&& src) {
    *this = std::move(src);
}

Framebuffer& Framebuffer::operator=(Framebuffer&& src) {
    destroy();
    std::memcpy(this, &src, sizeof(Framebuffer));
    std::memset(&src, 0, sizeof(Framebuffer));
    return *this;
}

void Framebuffer::recreate(unsigned int width, unsigned int height, VkRenderPass pass,
                           const std::vector<std::reference_wrapper<ImageView>>& ivs) {

    destroy();

    w = width;
    h = height;

    std::vector<VkImageView> views;
    for(auto& iv : ivs) views.push_back(iv.get().view);

    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = pass;
    fb_info.attachmentCount = views.size();
    fb_info.pAttachments = views.data();
    fb_info.width = width;
    fb_info.height = height;
    fb_info.layers = 1;

    VK_CHECK(vkCreateFramebuffer(get().gpu.device, &fb_info, nullptr, &buf));
}

void Framebuffer::destroy() {
    if(buf) vkDestroyFramebuffer(get().gpu.device, buf, nullptr);
    std::memset(this, 0, sizeof(Framebuffer));
}

Manager::Manager(unsigned int first_id) : next_id(first_id) {}

void Manager::begin_frame() {

    if(minimized) return;

    Frame& frame = frames[current_frame];
    Swapchain_Slot& image = swapchain.slots[current_img];

    vkWaitForFences(gpu.device, 1, &frame.fence, VK_TRUE, UINT64_MAX);

    erased_during[current_frame].clear();

    VkResult result = vkAcquireNextImageKHR(gpu.device, swapchain.swapchain, UINT64_MAX,
                                            frame.avail, nullptr, &current_img);

    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    } else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        die("Failed to acquire next image: %s", vk_err_str(result).c_str());
    }

    if(image.frame_fence != VK_NULL_HANDLE) {
        vkWaitForFences(gpu.device, 1, &image.frame_fence, VK_TRUE, UINT64_MAX);
    }
    image.frame_fence = frame.fence;

    ImGui_ImplVulkan_NewFrame();

    if(frame.secondary.size()) {
        vkFreeCommandBuffers(gpu.device, command_pool, frame.secondary.size(),
                             frame.secondary.data());
        frame.secondary.clear();
    }
}

void Manager::submit_frame() {

    Frame& frame = frames[current_frame];
    Swapchain_Slot& image = swapchain.slots[current_img];

    for(VkCommandBuffer& buf : frame.secondary) {
        VK_CHECK(vkEndCommandBuffer(buf));
    }

    VK_CHECK(vkResetCommandBuffer(frame.primary, 0));

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(frame.primary, &begin_info));

    VkRenderPassBeginInfo pass_info = {};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass_info.renderPass = output_pass;
    pass_info.framebuffer = image.framebuffer.buf;
    pass_info.renderArea.offset = {0, 0};
    pass_info.renderArea.extent = swapchain.extent;

    VkClearValue clears[2] = {};
    clears[0].color = {0.22f, 0.22f, 0.22f, 1.0f};
    clears[1].depthStencil = {0.0f, 0};
    pass_info.clearValueCount = 2;
    pass_info.pClearValues = clears;

    vkCmdBeginRenderPass(frame.primary, &pass_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    vkCmdExecuteCommands(frame.primary, frame.secondary.size(), frame.secondary.data());
    vkCmdEndRenderPass(frame.primary);

    VK_CHECK(vkEndCommandBuffer(frame.primary));

    VkSubmitInfo sub_info = {};
    sub_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_sems[] = {frame.avail};
    VkSemaphore sig_sems[] = {frame.finish};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    sub_info.waitSemaphoreCount = 1;
    sub_info.pWaitSemaphores = wait_sems;
    sub_info.pWaitDstStageMask = wait_stages;
    sub_info.commandBufferCount = 1;
    sub_info.pCommandBuffers = &frame.primary;
    sub_info.signalSemaphoreCount = 1;
    sub_info.pSignalSemaphores = sig_sems;

    vkResetFences(gpu.device, 1, &frame.fence);

    VK_CHECK(vkQueueSubmit(gpu.graphics_queue, 1, &sub_info, frame.fence));
}

std::pair<unsigned int, unsigned int> Manager::Swapchain::dim() {
    return {extent.width, extent.height};
}

float Manager::Swapchain::aspect_ratio() {
    return (float)extent.width / extent.height;
}

void Manager::trigger_resize() {
    needs_resize = true;
}

void Manager::end_frame() {

    render_imgui();
    submit_frame();

    if(minimized) {
        recreate_swapchain();
        return;
    }

    Frame& frame = frames[current_frame];

    VkSemaphore sig_sems[] = {frame.finish};

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = sig_sems;

    VkSwapchainKHR swapchains[] = {swapchain.swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &current_img;
    present_info.pResults = nullptr; // Optional

    VkResult result = vkQueuePresentKHR(gpu.present_queue, &present_info);
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || needs_resize) {
        recreate_swapchain();
    } else if(result != VK_SUCCESS) {
        die("Failed to present swapchain image: %s", vk_err_str(result).c_str());
    }

    current_frame = (current_frame + 1) % Frame::MAX_IN_FLIGHT;
}

VkCommandBuffer Manager::render_imgui() {
    
    ImGui::Render();
    VkCommandBuffer cmds = begin_secondary();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmds);
    return cmds;
}

VkCommandBuffer Manager::begin_secondary() {

    unsigned int i = current_img;
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(gpu.device, &alloc_info, &command_buffer);

    VkCommandBufferInheritanceInfo inherit_info = {};
    inherit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inherit_info.framebuffer = swapchain.slots[i].framebuffer.buf;
    inherit_info.renderPass = output_pass;
    inherit_info.subpass = 0;

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    begin_info.pInheritanceInfo = &inherit_info;

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

    frames[current_frame].secondary.push_back(command_buffer);
    return command_buffer;
}

VkExtent2D Manager::choose_surface_extent() {

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.data->device, swapchain.surface,
                                                       &gpu.data->surf_caps));

    VkExtent2D ext;
    if((int)gpu.data->surf_caps.currentExtent.width == -1) {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        ext.width = w;
        ext.height = h;
    } else {
        ext = gpu.data->surf_caps.currentExtent;
    }

    ext.width = clamp(ext.width, gpu.data->surf_caps.minImageExtent.width,
                      gpu.data->surf_caps.maxImageExtent.width);
    ext.height = clamp(ext.height, gpu.data->surf_caps.minImageExtent.height,
                       gpu.data->surf_caps.maxImageExtent.height);
    return ext;
}

void Manager::recreate_swapchain() {

    VkExtent2D ext = choose_surface_extent();
    minimized = ext.width == 0 && ext.height == 0;

    if(minimized) return;

    info("Recreating swapchain.");
    vkDeviceWaitIdle(gpu.device);

    destroy_swapchain();
    create_swapchain();
    create_output_pass();

    pipeline->create_depth_buf();
    create_framebuffers();
    pipeline->create_pipeline();

    ImGui_ImplVulkan_Shutdown();
    init_imgui();
    needs_resize = false;
}

void Manager::init(SDL_Window* sdl_window) {

    assert(sdl_window);
    window = sdl_window;

    create_instance();
    init_debug_callback();

    enumerate_gpus();
    select_gpu();

    create_logical_device_and_queues();
    create_gpu_alloc();

    create_command_pool();
    create_descriptor_pool();
    create_frames();

    create_swapchain();
    create_output_pass();

    pipeline = new Pipeline();
    pipeline->init();

    create_framebuffers();

    init_imgui();
}

void Manager::create_gpu_alloc() {

    VmaAllocatorCreateInfo alloc_info = {};
    alloc_info.vulkanApiVersion = VK_API_VERSION_1_2;
    alloc_info.physicalDevice = gpu.data->device;
    alloc_info.device = gpu.device;
    alloc_info.instance = info.instance;

    VK_CHECK(vmaCreateAllocator(&alloc_info, &gpu_alloc));
}

void Manager::init_imgui() {

    ImGui_ImplVulkan_InitInfo init = {};
    init.Instance = info.instance;
    init.PhysicalDevice = gpu.data->device;
    init.Device = gpu.device;
    init.QueueFamily = gpu.data->graphics_idx;
    init.Queue = gpu.graphics_queue;
    init.DescriptorPool = descriptor_pool;
    init.MinImageCount = swapchain.slots.size();
    init.ImageCount = swapchain.slots.size();
    init.CheckVkResultFn = vk_check_fn;
    ImGui_ImplVulkan_Init(&init, output_pass);

    VkCommandBuffer create_buf = begin_one_time();
    ImGui_ImplVulkan_CreateFontsTexture(create_buf);
    end_one_time(create_buf);

    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Manager::end_one_time(VkCommandBuffer cmds) {

    vkEndCommandBuffer(cmds);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmds;

    VK_CHECK(vkQueueSubmit(gpu.graphics_queue, 1, &submit_info, nullptr));
    VK_CHECK(vkQueueWaitIdle(gpu.graphics_queue));

    vkFreeCommandBuffers(gpu.device, command_pool, 1, &cmds);
}

VkCommandBuffer Manager::begin_one_time() {

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    VK_CHECK(vkAllocateCommandBuffers(gpu.device, &alloc_info, &command_buffer));

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
    return command_buffer;
}

void Manager::destroy_swapchain() {

    pipeline->destroy_swap();

    vkDestroyRenderPass(gpu.device, output_pass, nullptr);
    output_pass = {};

    for(Swapchain_Slot& image : swapchain.slots) {
        image.view.destroy();
        image.framebuffer.destroy();
    }
    vkDestroySwapchainKHR(gpu.device, swapchain.swapchain, nullptr);
    swapchain.slots.clear();
}

void Manager::destroy() {

    VK_CHECK(vkDeviceWaitIdle(gpu.device));

    for(unsigned int i = 0; i < Frame::MAX_IN_FLIGHT; i++) {
        erased_during[i].clear();
    }
    resources.clear();

    ImGui_ImplVulkan_Shutdown();
    destroy_swapchain();

    pipeline->destroy();

    for(Frame& frame : frames) {
        vkDestroyFence(gpu.device, frame.fence, nullptr);
        vkDestroySemaphore(gpu.device, frame.avail, nullptr);
        vkDestroySemaphore(gpu.device, frame.finish, nullptr);
        vkFreeCommandBuffers(gpu.device, command_pool, frame.secondary.size(),
                             frame.secondary.data());
        vkFreeCommandBuffers(gpu.device, command_pool, 1, &frame.primary);
    }
    frames.clear();

    vkDestroyCommandPool(gpu.device, command_pool, nullptr);
    vkDestroyDescriptorPool(gpu.device, descriptor_pool, nullptr);

    vmaDestroyAllocator(gpu_alloc);

    vkDestroyDevice(gpu.device, nullptr);
    vkDestroySurfaceKHR(info.instance, swapchain.surface, nullptr);
    destroy_debug_callback();

    vkDestroyInstance(info.instance, nullptr);

    gpu_alloc = {};
    gpu = {};
    command_pool = {};
    descriptor_pool = {};

    info = Info();
    swapchain = Swapchain();
}

static VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
                               VkDebugUtilsMessageTypeFlagsEXT type,
                               const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
    // Ignore VUID-VkSwapchainCreateInfoKHR-imageExtent-01274
    // cf. https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/1340
    if(data->messageIdNumber == 0x7cd0911d) return false;

    std::string message(data->pMessage);

    // Ignore these
    if(message.find("Device Extension") != std::string::npos ||
       message.find("Unloading layer library") != std::string::npos ||
       message.find("Loading layer library") != std::string::npos) {
        return false;
    }

    if(sev == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT ||
       sev == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        info("[VK] %s (%d)", message.c_str(), data->messageIdNumber);
    else
        warn("[VK] %s", message.c_str());

    for(unsigned int i = 0; i < data->queueLabelCount; i++)
        info("\tduring %s", data->pQueueLabels[i].pLabelName);
    for(unsigned int i = 0; i < data->cmdBufLabelCount; i++)
        info("\tinside %s", data->pCmdBufLabels[i].pLabelName);

    for(unsigned int i = 0; i < data->objectCount; i++) {
        const VkDebugUtilsObjectNameInfoEXT* obj = &data->pObjects[i];
        info("\tusing %s: %s (%zu)", vk_obj_type(obj->objectType).c_str(),
             obj->pObjectName ? obj->pObjectName : "?", obj->objectHandle);
    }

    if(sev == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        DEBUG_BREAK;
    }

    bool is_error = (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) &&
                    (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT);
    return is_error;
}

void Manager::init_debug_callback() {

    VkDebugUtilsMessengerCreateInfoEXT callback = {};
    callback.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    callback.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    callback.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    callback.pfnUserCallback = &debug_callback;
    callback.pUserData = this;

    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(info.instance,
                                                                  "vkCreateDebugUtilsMessengerEXT");

    if(func) {
        VK_CHECK(func(info.instance, &callback, nullptr, &info.debug_callback_info));
    } else {
        warn("Could not find vkCreateDebugUtilsMessengerEXT");
    }
}

void Manager::destroy_debug_callback() {

    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            info.instance, "vkDestroyDebugUtilsMessengerEXT");

    if(func) {
        func(info.instance, info.debug_callback_info, nullptr);
    } else {
        warn("Could not find vkDestroyDebugUtilsMessengerEXT");
    }
}

void Manager::create_instance() {

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "GPURT";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "GPURT";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    info.inst_ext.clear();
    info.dev_ext.clear();
    info.layers.clear();

    info.inst_ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    info.dev_ext.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    info.dev_ext.push_back("VK_KHR_shader_float_controls");
    info.dev_ext.push_back("VK_KHR_spirv_1_4");
    info.dev_ext.push_back("VK_EXT_descriptor_indexing");
    info.dev_ext.push_back("VK_KHR_buffer_device_address");
    info.dev_ext.push_back("VK_KHR_deferred_host_operations");
    info.dev_ext.push_back("VK_KHR_acceleration_structure");
    info.dev_ext.push_back("VK_KHR_ray_tracing_pipeline");
    info.dev_ext.push_back("VK_KHR_ray_query");

    unsigned int sdl_count = 0;
    if(!SDL_Vulkan_GetInstanceExtensions(window, &sdl_count, nullptr)) {
        die("Failed to get required SDL vk instance extensions: %", SDL_GetError());
    }
    info.inst_ext.insert(info.inst_ext.end(), sdl_count, nullptr);
    if(!SDL_Vulkan_GetInstanceExtensions(window, &sdl_count,
                                         info.inst_ext.data() + info.inst_ext.size() - sdl_count)) {
        die("Failed to get required SDL vk instance extensions: %", SDL_GetError());
    }

    create_info.enabledExtensionCount = info.inst_ext.size();
    create_info.ppEnabledExtensionNames = info.inst_ext.data();

    info.layers.push_back("VK_LAYER_KHRONOS_validation");
    create_info.enabledLayerCount = info.layers.size();
    create_info.ppEnabledLayerNames = info.layers.data();

    VkResult inst = vkCreateInstance(&create_info, nullptr, &info.instance);
    if(inst != VK_SUCCESS) {
        if(inst == VK_ERROR_LAYER_NOT_PRESENT) {
            info.layers.clear();
            create_info.enabledLayerCount = 0;
            create_info.ppEnabledLayerNames = nullptr;
            VK_CHECK(vkCreateInstance(&create_info, nullptr, &info.instance));
        } else {
            die("Failed to create VkInstance: %s", vk_err_str(inst).c_str());
        }
    }

    if(!SDL_Vulkan_CreateSurface(window, info.instance, &swapchain.surface)) {
        die("Failed to create SDL VkSurface: %s", SDL_GetError());
    }

    unsigned int total_extensions = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &total_extensions, nullptr));
    info.extensions.clear();
    info.extensions.resize(total_extensions);
    VK_CHECK(
        vkEnumerateInstanceExtensionProperties(nullptr, &total_extensions, info.extensions.data()));
}

void Manager::enumerate_gpus() {

    unsigned int devices = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(info.instance, &devices, nullptr));
    if(!devices) {
        die("Found no GPUs.");
    }

    std::vector<VkPhysicalDevice> phys_list(devices);
    VK_CHECK(vkEnumeratePhysicalDevices(info.instance, &devices, phys_list.data()));

    gpus.clear();
    gpus.resize(devices);
    for(unsigned int i = 0; i < devices; i++) {
        GPU& g = gpus[i];
        g.device = phys_list[i];

        {
            unsigned int num_queues = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(g.device, &num_queues, nullptr);
            if(num_queues <= 0) warn("Found no device queues.");
            g.queue_families.resize(num_queues);
            vkGetPhysicalDeviceQueueFamilyProperties(g.device, &num_queues,
                                                     g.queue_families.data());
        }
        {
            unsigned int num_exts = 0;
            VK_CHECK(vkEnumerateDeviceExtensionProperties(g.device, nullptr, &num_exts, nullptr));
            if(!num_exts) warn("Found no device extensions.");
            g.exts.resize(num_exts);
            VK_CHECK(
                vkEnumerateDeviceExtensionProperties(g.device, nullptr, &num_exts, g.exts.data()));
        }
        {
            VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g.device, swapchain.surface,
                                                               &g.surf_caps));
        }
        {
            unsigned int num_fmts = 0;
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(g.device, swapchain.surface, &num_fmts,
                                                          nullptr));
            if(!num_fmts) warn("Found no device surface formats.");
            g.fmts.resize(num_fmts);
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(g.device, swapchain.surface, &num_fmts,
                                                          g.fmts.data()));
        }
        {
            unsigned int num_modes = 0;
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(g.device, swapchain.surface,
                                                               &num_modes, nullptr));
            if(!num_modes) warn("Found no device present modes.");
            g.modes.resize(num_modes);
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(g.device, swapchain.surface,
                                                               &num_modes, g.modes.data()));
        }
        {
            vkGetPhysicalDeviceMemoryProperties(g.device, &g.mem_prop);
            vkGetPhysicalDeviceFeatures(g.device, &g.features);
            vkGetPhysicalDeviceProperties(g.device, &g.dev_prop);
        }
    }
}

bool Manager::GPU::supports(const std::vector<const char*>& extensions) {

    size_t matched = 0;

    for(auto e : extensions) {
        for(auto& p : exts) {
            if(std::strcmp(e, p.extensionName) == 0) {
                matched++;
                break;
            }
        }
    }

    return matched == extensions.size();
}

void Manager::select_gpu() {

    for(GPU& g : gpus) {

        std::string name = g.dev_prop.deviceName;
        g.graphics_idx = -1;
        g.present_idx = -1;

        if(!g.features.samplerAnisotropy) {
            info("Device % does not support anisotropic sampling.", name);
            continue;
        }

        if(!g.supports(info.dev_ext)) {
            info("Device % does not support device extensions.", name);
            continue;
        }

        if(!g.fmts.size()) {
            warn("Device % has no available surface formats.", name);
            continue;
        }
        if(!g.modes.size()) {
            warn("Device % has no available present modes.", name);
            continue;
        }

        for(unsigned int i = 0; i < g.queue_families.size(); i++) {
            auto& family = g.queue_families[i];
            if(!family.queueCount) continue;
            if(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                g.graphics_idx = i;
                break;
            }
        }

        for(unsigned int i = 0; i < g.queue_families.size(); i++) {
            auto& family = g.queue_families[i];
            if(!family.queueCount) continue;

            VkBool32 supports_present = VK_FALSE;
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(g.device, i, swapchain.surface,
                                                          &supports_present));
            if(supports_present) {
                g.present_idx = i;
                break;
            }
        }

        if(g.graphics_idx >= 0 && g.present_idx >= 0) {
            // TODO(max): choose best GPU out of all compatible GPUs (e.g. discrete GPU)
            info("Selecting GPU: %s", name.c_str());
            gpu.data = &g;
            return;
        }

        info("Device %s does not have a suitable graphics and present queue.", name.c_str());
    }

    die("Failed to find compatible Vulkan device.");
}

void Manager::create_logical_device_and_queues() {

    const float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> q_info;

    {
        VkDeviceQueueCreateInfo qinfo = {};
        qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qinfo.queueFamilyIndex = gpu.data->graphics_idx;
        qinfo.queueCount = 1;
        qinfo.pQueuePriorities = &priority;
        q_info.push_back(qinfo);
    }
    if(gpu.data->present_idx != gpu.data->graphics_idx) {
        VkDeviceQueueCreateInfo qinfo = {};
        qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qinfo.queueFamilyIndex = gpu.data->present_idx;
        qinfo.queueCount = 1;
        qinfo.pQueuePriorities = &priority;
        q_info.push_back(qinfo);
    }

    // TODO(max): figure out what device features we need to enable
    VkPhysicalDeviceFeatures features = {};
    features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo dev_info = {};
    dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dev_info.queueCreateInfoCount = q_info.size();
    dev_info.pQueueCreateInfos = q_info.data();
    dev_info.pEnabledFeatures = &features;
    dev_info.enabledExtensionCount = info.dev_ext.size();
    dev_info.ppEnabledExtensionNames = info.dev_ext.data();
    dev_info.enabledLayerCount = info.layers.size();
    dev_info.ppEnabledLayerNames = info.layers.data();

    VK_CHECK(vkCreateDevice(gpu.data->device, &dev_info, nullptr, &gpu.device));

    vkGetDeviceQueue(gpu.device, gpu.data->graphics_idx, 0, &gpu.graphics_queue);
    vkGetDeviceQueue(gpu.device, gpu.data->present_idx, 0, &gpu.present_queue);
}

void Manager::create_frames() {

    VkSemaphoreCreateInfo sinfo = {};
    sinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo finfo = {};
    finfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    finfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    frames.resize(Frame::MAX_IN_FLIGHT);
    for(Frame& frame : frames) {
        VK_CHECK(vkCreateSemaphore(gpu.device, &sinfo, nullptr, &frame.avail));
        VK_CHECK(vkCreateSemaphore(gpu.device, &sinfo, nullptr, &frame.finish));
        VK_CHECK(vkCreateFence(gpu.device, &finfo, nullptr, &frame.fence));

        { // Create command buffer
            VkCommandBufferAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc_info.commandPool = command_pool;
            alloc_info.commandBufferCount = 1;
            VK_CHECK(vkAllocateCommandBuffers(gpu.device, &alloc_info, &frame.primary));
        }
    }
}

void Manager::create_command_pool() {

    VkCommandPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex = gpu.data->graphics_idx;

    VK_CHECK(vkCreateCommandPool(gpu.device, &create_info, nullptr, &command_pool));
}

static VkSurfaceFormatKHR choose_format(const std::vector<VkSurfaceFormatKHR>& formats) {

    VkSurfaceFormatKHR result;

    if(formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        result.format = VK_FORMAT_B8G8R8A8_UNORM;
        result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        warn("Undefined surface format give, using default.");
        return result;
    }
    for(unsigned int i = 0; i < formats.size(); i++) {
        const VkSurfaceFormatKHR& fmt = formats[i];
        if(fmt.format == VK_FORMAT_B8G8R8A8_UNORM &&
           fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return fmt;
        }
    }

    warn("Suitable surface format not found!");
    return formats[0];
}

static VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes) {

    const VkPresentModeKHR desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;

    for(unsigned int i = 0; i < modes.size(); i++) {
        if(modes[i] == desiredMode) {
            return desiredMode;
        }
    }
    warn("Mailbox present not found, using FIFO.");
    return VK_PRESENT_MODE_FIFO_KHR;
}

void Manager::create_framebuffers() {

    for(Swapchain_Slot& slot : swapchain.slots) {
        slot.framebuffer.recreate(swapchain.extent.width, swapchain.extent.height, output_pass,
                                  {slot.view, pipeline->depth_view});
    }
}

void Manager::create_swapchain() {

    swapchain.format = choose_format(gpu.data->fmts);
    swapchain.present_mode = choose_present_mode(gpu.data->modes);
    swapchain.extent = choose_surface_extent();

    VkSwapchainCreateInfoKHR sw_info = {};
    sw_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sw_info.surface = swapchain.surface;

    sw_info.minImageCount = gpu.data->surf_caps.minImageCount;
    sw_info.imageFormat = swapchain.format.format;
    sw_info.imageColorSpace = swapchain.format.colorSpace;
    sw_info.imageExtent = swapchain.extent;
    sw_info.imageArrayLayers = 1;
    sw_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    unsigned int queue_indices[] = {(unsigned int)gpu.data->graphics_idx,
                                    (unsigned int)gpu.data->present_idx};

    if(gpu.data->graphics_idx != gpu.data->present_idx) {

        sw_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sw_info.queueFamilyIndexCount = 2;
        sw_info.pQueueFamilyIndices = queue_indices;

    } else {

        sw_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    sw_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    sw_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sw_info.presentMode = swapchain.present_mode;
    sw_info.clipped = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(gpu.device, &sw_info, nullptr, &swapchain.swapchain));

    unsigned int images = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(gpu.device, swapchain.swapchain, &images, nullptr));
    if(!images) {
        die("Failed to get any images from vk swapchain!");
    }

    swapchain.slots.resize(images);

    std::vector<VkImage> image_data(images);

    VK_CHECK(vkGetSwapchainImagesKHR(gpu.device, swapchain.swapchain, &images, image_data.data()));
    if(!images) {
        die("Failed to get any images from vk swapchain!");
    }

    for(unsigned int i = 0; i < images; i++) {

        Swapchain_Slot& image = swapchain.slots[i];
        image.image = image_data[i];
        image.view.recreate(image.image, swapchain.format.format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

VkFormat Manager::choose_supported_format(const std::vector<VkFormat>& formats,
                                          VkImageTiling tiling, VkFormatFeatureFlags features) {

    for(VkFormat format : formats) {

        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(gpu.data->device, format, &props);

        if(tiling == VK_IMAGE_TILING_LINEAR &&
           (props.linearTilingFeatures & features) == features) {
            return format;
        } else if(tiling == VK_IMAGE_TILING_OPTIMAL &&
                  (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    die("Failed to find suitable VkFormat.");
}

void Manager::create_output_pass() {

    VkAttachmentDescription color = {};
    color.format = swapchain.format.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth = {};
    depth.format = find_depth_format();
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref = {};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref = {};
    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color, depth};

    VkRenderPassCreateInfo pass_info = {};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass_info.attachmentCount = 2;
    pass_info.pAttachments = attachments;
    pass_info.subpassCount = 1;
    pass_info.pSubpasses = &subpass;
    pass_info.dependencyCount = 1;
    pass_info.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(gpu.device, &pass_info, nullptr, &output_pass));
}

unsigned int Manager::choose_memory_type(unsigned int filter, VkMemoryPropertyFlags properties) {

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(gpu.data->device, &mem_props);

    for(unsigned int i = 0; i < mem_props.memoryTypeCount; i++) {
        bool type_bit = filter & (i << i);
        bool prop_bits = (mem_props.memoryTypes[i].propertyFlags & properties) == properties;
        if(type_bit && prop_bits) {
            return i;
        }
    }

    die("Failed to find suitable memory type!");
}

void Manager::create_descriptor_pool() {

    std::array<VkDescriptorPoolSize, 11> pool_sizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1024}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = pool_sizes.size() * 1024;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(gpu.device, &pool_info, nullptr, &descriptor_pool));
}

VkFormat Manager::find_depth_format() {
    return choose_supported_format(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

} // namespace VK
