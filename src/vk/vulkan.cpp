
#include "vulkan.h"
#include "imgui_impl_vulkan.h"

#include <SDL2/SDL_vulkan.h>
#include <lib/log.h>
#include <util/files.h>
#include <util/image.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "render.h"

namespace VK {

Manager& vk() {
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
    if(buf && mem) vmaDestroyBuffer(vk().gpu_alloc, buf, mem);
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

    if(sz) VK_CHECK(vmaCreateBuffer(vk().gpu_alloc, &buf_info, &alloc_info, &buf, &mem, nullptr));
}

void* Buffer::map() const {
    void* map;
    VK_CHECK(vmaMapMemory(vk().gpu_alloc, mem, &map));
    return map;
}

void Buffer::unmap() const {
    vmaUnmapMemory(vk().gpu_alloc, mem);
}

VkDeviceAddress Buffer::address() const {
    if(!buf) return {};
    VkBufferDeviceAddressInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = buf;
    return vkGetBufferDeviceAddress(vk().device(), &info);
}

void Buffer::copy_to(const Buffer& dst) {

    VkCommandBuffer cmds = vk().begin_one_time();

    VkBufferCopy region = {};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(cmds, buf, dst.buf, 1, &region);

    vk().end_one_time(cmds);
}

void Buffer::to_image(VkCommandBuffer& cmds, const Image& image) {

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
}

void Buffer::write(const void* data, size_t dsize) {

    if(!dsize) return;
    assert(dsize <= size);

    void* map;
    VK_CHECK(vmaMapMemory(vk().gpu_alloc, mem, &map));
    std::memcpy(map, data, dsize);
    vmaUnmapMemory(vk().gpu_alloc, mem);
}

void Buffer::read(void* data, size_t dsize) {

    if(!dsize) return;
    assert(dsize <= size);

    void* map;
    VK_CHECK(vmaMapMemory(vk().gpu_alloc, mem, &map));
    std::memcpy(data, map, dsize);
    vmaUnmapMemory(vk().gpu_alloc, mem);
}

void Buffer::write_staged(const void* data, size_t dsize) {

    if(!dsize) return;
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
    if(img && mem) vmaDestroyImage(vk().gpu_alloc, img, mem);
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

    VK_CHECK(vmaCreateImage(vk().gpu_alloc, &img_info, &alloc_info, &img, &mem, nullptr));
}

void Image::write(Util::Image& data) {

    VkCommandBuffer cmds = vk().begin_one_time();

    transition(cmds, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    Buffer staging(data.bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    staging.write(data.data(), data.bytes());
    staging.to_image(cmds, *this);

    transition(cmds, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vk().end_one_time(cmds);
}

void Image::transition(VkImageLayout new_l) {
    VkCommandBuffer cmds = vk().begin_one_time();
    transition(cmds, new_l);
    vk().end_one_time(cmds);
}

void Image::transition(VkCommandBuffer& cmds, VkImageLayout new_l) {
    transition(cmds, layout, new_l);
}

void Image::transition(VkCommandBuffer& cmds, VkImageLayout old_l, VkImageLayout new_l) {

    if(old_l == new_l) return;
    layout = new_l;

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

    switch(old_l) {
    case VK_IMAGE_LAYOUT_UNDEFINED: {
        barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    } break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
        barrier.srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } break;

    case VK_IMAGE_LAYOUT_GENERAL: {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    } break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } break;

    default: die("Unsupported image transition src.");
    }

    switch(new_l) {
    case VK_IMAGE_LAYOUT_GENERAL: {
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    } break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
        barrier.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } break;

    default: die("Unsupported image transition dst.");
    }

    vkCmdPipelineBarrier(cmds, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

ImageView::ImageView(Image& image, VkImageAspectFlags aspect) {
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

void ImageView::recreate(Image& img, VkImageAspectFlags asp) {
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

    VK_CHECK(vkCreateImageView(vk().device(), &view_info, nullptr, &view));
}

void ImageView::destroy() {
    if(view) vkDestroyImageView(vk().device(), view, nullptr);
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
    sample_info.maxAnisotropy = vk().gpu.data->dev_prop.limits.maxSamplerAnisotropy;
    sample_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sample_info.unnormalizedCoordinates = VK_FALSE;
    sample_info.compareEnable = VK_FALSE;
    sample_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sample_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sample_info.mipLodBias = 0.0f;
    sample_info.minLod = 0.0f;
    sample_info.maxLod = 0.0f;

    VK_CHECK(vkCreateSampler(vk().device(), &sample_info, nullptr, &sampler));
}

void Sampler::destroy() {
    if(sampler) vkDestroySampler(vk().device(), sampler, nullptr);
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

    VK_CHECK(vkCreateShaderModule(vk().device(), &mod_info, nullptr, &shader));
}

void Shader::destroy() {
    if(shader) vkDestroyShaderModule(vk().device(), shader, nullptr);
    std::memset(this, 0, sizeof(Shader));
}

Framebuffer::Framebuffer(unsigned int width, unsigned int height, const Pass& pass,
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

void Framebuffer::recreate(unsigned int width, unsigned int height, const Pass& pass,
                           const std::vector<std::reference_wrapper<ImageView>>& ivs) {
    recreate(width, height, pass.pass, ivs);
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

    VK_CHECK(vkCreateFramebuffer(vk().device(), &fb_info, nullptr, &buf));
}

void Framebuffer::destroy() {
    if(buf) vkDestroyFramebuffer(vk().device(), buf, nullptr);
    std::memset(this, 0, sizeof(Framebuffer));
}

Pass::Pass(const Pass::Info& info) {
    recreate(info);
}

Pass::~Pass() {
    destroy();
}

Pass::Pass(Pass&& src) {
    *this = std::move(src);
}

Pass& Pass::operator=(Pass&& src) {
    destroy();
    std::memcpy(this, &src, sizeof(Pass));
    std::memset(&src, 0, sizeof(Pass));
    return *this;
}

void Pass::begin(VkCommandBuffer& cmds, Framebuffer& fb, const std::vector<VkClearValue>& clears) {

    VkRenderPassBeginInfo pinfo = {};
    pinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pinfo.renderPass = pass;
    pinfo.framebuffer = fb.buf;
    pinfo.renderArea.offset = {0, 0};
    pinfo.renderArea.extent.width = fb.w;
    pinfo.renderArea.extent.height = fb.h;
    pinfo.clearValueCount = clears.size();
    pinfo.pClearValues = clears.data();

    vkCmdBeginRenderPass(cmds, &pinfo, VK_SUBPASS_CONTENTS_INLINE);
}

void Pass::end(VkCommandBuffer& cmds) {
    vkCmdEndRenderPass(cmds);
}

void Pass::recreate(const Pass::Info& info) {

    destroy();

    std::vector<VkSubpassDescription> subpasses;
    for(auto& sp : info.subpasses) {
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = sp.bind;
        subpass.colorAttachmentCount = sp.color.size();
        subpass.pColorAttachments = sp.color.data();
        if(sp.depth.attachment) subpass.pDepthStencilAttachment = &sp.depth;
        subpasses.push_back(subpass);
    }

    VkRenderPassCreateInfo pinfo = {};
    pinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pinfo.attachmentCount = info.attachments.size();
    pinfo.pAttachments = info.attachments.data();
    pinfo.subpassCount = subpasses.size();
    pinfo.pSubpasses = subpasses.data();
    pinfo.dependencyCount = info.dependencies.size();
    pinfo.pDependencies = info.dependencies.data();

    VK_CHECK(vkCreateRenderPass(vk().device(), &pinfo, nullptr, &pass));
}

void Pass::destroy() {
    if(pass) vkDestroyRenderPass(vk().device(), pass, nullptr);
    std::memset(this, 0, sizeof(Pass));
}

PipeData::~PipeData() {
    destroy();
}

PipeData::PipeData(PipeData&& src) {
    *this = std::move(src);
}

PipeData& PipeData::operator=(PipeData&& src) {
    destroy();
    std::memcpy(this, &src, sizeof(PipeData));
    std::memset(&src, 0, sizeof(PipeData));
    return *this;
}

void PipeData::destroy() {
    destroy_swap();
    if(descriptor_sets.size()) {
        vkFreeDescriptorSets(vk().device(), vk().pool(), descriptor_sets.size(),
                             descriptor_sets.data());
        descriptor_sets.clear();
    }
    if(d_layout) {
        vkDestroyDescriptorSetLayout(vk().device(), d_layout, nullptr);
        d_layout = {};
    }
}

void PipeData::destroy_swap() {
    if(pipe) {
        vkDestroyPipeline(vk().device(), pipe, nullptr);
        pipe = {};
    }
    if(p_layout) {
        vkDestroyPipelineLayout(vk().device(), p_layout, nullptr);
        p_layout = {};
    }
}

Accel::Accel(const Mesh& mesh) {
    recreate(mesh);
}

Accel::Accel(const std::vector<Drop<Accel>>& blas, const std::vector<Mat4>& T) {
    recreate(blas, T);
}

Accel::~Accel() {
    destroy();
}

Accel::Accel(Accel&& src) {
    *this = std::move(src);
}

Accel& Accel::operator=(Accel&& src) {
    destroy();
    abuf = std::move(src.abuf);
    ibuf = std::move(src.ibuf);
    flags = src.flags;
    accel = src.accel;
    size = src.size;
    std::memset(&src, 0, sizeof(Accel));
    return *this;
}

void Accel::destroy() {
    if(accel) vk().rtx.vkDestroyAccelerationStructureKHR(vk().device(), accel, nullptr);
    abuf.destroy();
    ibuf.destroy();
    flags = {};
    size = {};
    accel = {};
}

void Accel::recreate(const std::vector<Drop<Accel>>& blas, const std::vector<Mat4>& T) {

    destroy();
    flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(blas.size());

    for(size_t i = 0; i < blas.size(); i++) {

        VkAccelerationStructureDeviceAddressInfoKHR addr = {};
        addr.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addr.accelerationStructure = blas[i]->accel;

        VkDeviceAddress blasAddress =
            vk().rtx.vkGetAccelerationStructureDeviceAddressKHR(vk().device(), &addr);

        VkAccelerationStructureInstanceKHR as_inst = {};
        Mat4 inst = T[i].T();
        std::memcpy(&as_inst.transform, &inst, sizeof(as_inst.transform));

        as_inst.instanceCustomIndex = i;
        as_inst.accelerationStructureReference = blasAddress;
        as_inst.instanceShaderBindingTableRecordOffset = 0;
        as_inst.mask = 0xFF;
        as_inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        instances.push_back(as_inst);
    }

    VkDeviceSize instances_size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    ibuf.recreate(instances_size,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VMA_MEMORY_USAGE_GPU_ONLY);
    ibuf.write_staged(instances.data(), instances_size);

    VkDeviceAddress instanceAddress = ibuf.address();

    VkAccelerationStructureGeometryInstancesDataKHR instancesVk = {};
    instancesVk.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesVk.arrayOfPointers = VK_FALSE;
    instancesVk.data.deviceAddress = instanceAddress;

    VkAccelerationStructureGeometryKHR topASGeometry = {};
    topASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topASGeometry.geometry.instances = instancesVk;

    VkAccelerationStructureBuildGeometryInfoKHR build_info = {};
    build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_info.flags = flags;
    build_info.geometryCount = 1;
    build_info.pGeometries = &topASGeometry;
    build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build_info.srcAccelerationStructure = VK_NULL_HANDLE;

    unsigned int count = (unsigned int)instances.size();
    size.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vk().rtx.vkGetAccelerationStructureBuildSizesKHR(
        vk().device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &count, &size);

    abuf.recreate(size.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                  VMA_MEMORY_USAGE_GPU_ONLY);

    VkAccelerationStructureCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    create_info.size = size.accelerationStructureSize;

    VkAccelerationStructureBuildRangeInfoKHR offset = {};
    offset.primitiveCount = (unsigned int)instances.size();

    create_and_build(create_info, build_info, offset);
}

void Accel::create_and_build(VkAccelerationStructureCreateInfoKHR create_info,
                             VkAccelerationStructureBuildGeometryInfoKHR build_info,
                             VkAccelerationStructureBuildRangeInfoKHR offset) {

    create_info.buffer = abuf.buf;

    VK_CHECK(
        vk().rtx.vkCreateAccelerationStructureKHR(vk().device(), &create_info, nullptr, &accel));

    Buffer scratch(size.buildScratchSize,
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   VMA_MEMORY_USAGE_GPU_ONLY);

    build_info.scratchData.deviceAddress = scratch.address();
    build_info.dstAccelerationStructure = accel;

    VkAccelerationStructureBuildRangeInfoKHR* offsets[] = {&offset};

    VkCommandBuffer cmds = vk().begin_one_time();
    vk().rtx.vkCmdBuildAccelerationStructuresKHR(cmds, 1, &build_info, offsets);
    vk().end_one_time(cmds);
}

void Accel::recreate(const Mesh& mesh) {

    destroy();
    flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

    mesh.sync();
    VkDeviceAddress v_addr = mesh.vbuf->address();
    VkDeviceAddress i_addr = mesh.ibuf->address();

    uint32_t maxPrimitiveCount = mesh._idxs.size() / 3;

    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = v_addr;
    triangles.vertexStride = sizeof(Mesh::Vertex);
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = i_addr;
    triangles.maxVertex = mesh._verts.size();

    VkAccelerationStructureGeometryKHR geom = {};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.triangles = triangles;

    VkAccelerationStructureBuildRangeInfoKHR offset = {};
    offset.primitiveCount = maxPrimitiveCount;

    VkAccelerationStructureBuildGeometryInfoKHR build_info = {};
    build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_info.flags = flags;
    build_info.geometryCount = 1;
    build_info.pGeometries = &geom;
    build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_info.srcAccelerationStructure = VK_NULL_HANDLE;

    size.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vk().rtx.vkGetAccelerationStructureBuildSizesKHR(
        vk().device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info,
        &maxPrimitiveCount, &size);

    abuf.recreate(size.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                  VMA_MEMORY_USAGE_GPU_ONLY);

    VkAccelerationStructureCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    create_info.size = size.accelerationStructureSize;

    create_and_build(create_info, build_info, offset);
}

bool Manager::begin_frame() {

    Frame& frame = frames[current_frame];
    Swapchain_Slot& image = swapchain.slots[current_img];

    vkWaitForFences(gpu.device, 1, &frame.fence, VK_TRUE, UINT64_MAX);

    do_erase();
    if(frame.buffers.size()) {
        vkFreeCommandBuffers(gpu.device, command_pool, frame.buffers.size(), frame.buffers.data());
        frame.buffers.clear();
    }

    if(minimized) return true;

    VkResult result = vkAcquireNextImageKHR(gpu.device, swapchain.swapchain, UINT64_MAX,
                                            frame.avail, nullptr, &current_img);

    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return true;
    } else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        die("Failed to acquire next image: %s", vk_err_str(result).c_str());
    }

    if(image.frame_fence != VK_NULL_HANDLE) {
        vkWaitForFences(gpu.device, 1, &image.frame_fence, VK_TRUE, UINT64_MAX);
    }
    image.frame_fence = frame.fence;

    ImGui_ImplVulkan_NewFrame();

    return false;
}

void Manager::submit_frame(ImageView& out_image) {

    Frame& frame = frames[current_frame];
    for(VkCommandBuffer& buf : frame.buffers) {
        VK_CHECK(vkEndCommandBuffer(buf));
    }

    VK_CHECK(vkResetCommandBuffer(frame.composite, 0));

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(frame.composite, &begin_info));

    compositor.composite(frame.composite, out_image);

    VK_CHECK(vkEndCommandBuffer(frame.composite));

    VkSemaphore wait_sems[] = {frame.avail};
    VkSemaphore sig_sems[] = {frame.finish};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    std::vector<VkCommandBuffer> buffers = frame.buffers;
    buffers.push_back(frame.composite);

    VkSubmitInfo sub_info = {};
    sub_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sub_info.waitSemaphoreCount = 1;
    sub_info.pWaitSemaphores = wait_sems;
    sub_info.pWaitDstStageMask = wait_stages;
    sub_info.commandBufferCount = buffers.size();
    sub_info.pCommandBuffers = buffers.data();
    sub_info.signalSemaphoreCount = 1;
    sub_info.pSignalSemaphores = sig_sems;

    VK_CHECK(vkResetFences(gpu.device, 1, &frame.fence));
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

void Manager::end_frame(ImageView& img) {

    ImGui::Render();

    if(minimized) {
        recreate_swapchain();
        return;
    }

    submit_frame(img);

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
        current_frame = 0;
        return;
    } else if(result != VK_SUCCESS) {
        die("Failed to present swapchain image: %s", vk_err_str(result).c_str());
    }

    current_frame = (current_frame + 1) % MAX_IN_FLIGHT;
}

unsigned int Manager::frame() const {
    return current_frame;
}

VkCommandBuffer Manager::begin() {

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(gpu.device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

    frames[current_frame].buffers.push_back(command_buffer);
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

    compositor.create_swap();

    ImGui_ImplVulkan_Shutdown();
    init_imgui();

    for(auto& f : resize_callbacks) f();

    needs_resize = false;
}

void Manager::init(SDL_Window* sdl_window) {

    assert(sdl_window);
    window = sdl_window;

    create_instance();
    init_debug_callback();
    init_rt();

    enumerate_gpus();
    select_gpu();

    create_logical_device_and_queues();
    create_gpu_alloc();

    create_command_pool();
    create_descriptor_pool();
    create_frames();

    create_swapchain();

    compositor.init();

    init_imgui();
}

void Manager::create_gpu_alloc() {

    VmaAllocatorCreateInfo alloc_info = {};
    alloc_info.vulkanApiVersion = VK_API_VERSION_1_2;
    alloc_info.physicalDevice = gpu.data->device;
    alloc_info.device = gpu.device;
    alloc_info.instance = info.instance;
    alloc_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

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
    ImGui_ImplVulkan_Init(&init, compositor.get_pass());

    VkCommandBuffer create_buf = begin_one_time();
    ImGui_ImplVulkan_CreateFontsTexture(create_buf);
    end_one_time(create_buf);

    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Manager::end_one_time(VkCommandBuffer cmds) {

    VK_CHECK(vkEndCommandBuffer(cmds));

    VkFence fence = {};
    VkFenceCreateInfo finfo = {};
    finfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(gpu.device, &finfo, nullptr, &fence));

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmds;

    VK_CHECK(vkQueueSubmit(gpu.graphics_queue, 1, &submit_info, fence));
    VK_CHECK(vkWaitForFences(gpu.device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(gpu.device, fence, nullptr);
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

    compositor.destroy_swap();
    vkDestroySwapchainKHR(gpu.device, swapchain.swapchain, nullptr);
    swapchain.slots.clear();
}

void Manager::destroy() {

    VK_CHECK(vkDeviceWaitIdle(gpu.device));

    for(unsigned int i = 0; i < MAX_IN_FLIGHT; i++) {
        erase_queue[i].clear();
    }

    ImGui_ImplVulkan_Shutdown();
    destroy_swapchain();

    compositor.destroy();

    for(Frame& frame : frames) {
        vkDestroyFence(gpu.device, frame.fence, nullptr);
        vkDestroySemaphore(gpu.device, frame.avail, nullptr);
        vkDestroySemaphore(gpu.device, frame.finish, nullptr);

        if(frame.buffers.size())
            vkFreeCommandBuffers(gpu.device, command_pool, frame.buffers.size(),
                                 frame.buffers.data());

        vkFreeCommandBuffers(gpu.device, command_pool, 1, &frame.composite);
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

void Manager::init_rt() {

    rtx.vkGetPhysicalDeviceProperties2 =
        (PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(
            info.instance, "vkGetPhysicalDeviceProperties2");

    if(!rtx.vkGetPhysicalDeviceProperties2) die("Failed to load vkGetPhysicalDeviceProperties2");

    rtx.vkCreateAccelerationStructureKHR =
        (PFN_vkCreateAccelerationStructureKHR)vkGetInstanceProcAddr(
            info.instance, "vkCreateAccelerationStructureKHR");

    if(!rtx.vkCreateAccelerationStructureKHR)
        die("Failed to load vkCreateAccelerationStructureKHR");

    rtx.vkDestroyAccelerationStructureKHR =
        (PFN_vkDestroyAccelerationStructureKHR)vkGetInstanceProcAddr(
            info.instance, "vkDestroyAccelerationStructureKHR");

    if(!rtx.vkDestroyAccelerationStructureKHR)
        die("Failed to load vkDestroyAccelerationStructureKHR");

    rtx.vkGetAccelerationStructureBuildSizesKHR =
        (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetInstanceProcAddr(
            info.instance, "vkGetAccelerationStructureBuildSizesKHR");

    if(!rtx.vkGetAccelerationStructureBuildSizesKHR)
        die("Failed to load vkGetAccelerationStructureBuildSizesKHR");

    rtx.vkCmdBuildAccelerationStructuresKHR =
        (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetInstanceProcAddr(
            info.instance, "vkCmdBuildAccelerationStructuresKHR");

    if(!rtx.vkCmdBuildAccelerationStructuresKHR)
        die("Failed to load vkCmdBuildAccelerationStructuresKHR");

    rtx.vkGetAccelerationStructureDeviceAddressKHR =
        (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetInstanceProcAddr(
            info.instance, "vkGetAccelerationStructureDeviceAddressKHR");

    if(!rtx.vkGetAccelerationStructureDeviceAddressKHR)
        die("Failed to load vkGetAccelerationStructureDeviceAddressKHR");

    rtx.vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetInstanceProcAddr(
        info.instance, "vkCreateRayTracingPipelinesKHR");

    if(!rtx.vkCreateRayTracingPipelinesKHR) die("Failed to load vkCreateRayTracingPipelinesKHR");

    rtx.vkGetRayTracingShaderGroupHandlesKHR =
        (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetInstanceProcAddr(
            info.instance, "vkGetRayTracingShaderGroupHandlesKHR");

    if(!rtx.vkGetRayTracingShaderGroupHandlesKHR)
        die("Failed to load vkGetRayTracingShaderGroupHandlesKHR");

    rtx.vkCmdTraceRaysKHR =
        (PFN_vkCmdTraceRaysKHR)vkGetInstanceProcAddr(info.instance, "vkCmdTraceRaysKHR");

    if(!rtx.vkCmdTraceRaysKHR) die("Failed to load vkCmdTraceRaysKHR");
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

VkRenderPassBeginInfo Manager::Compositor::pass_info() {
    VkRenderPassBeginInfo pinfo = {};
    pinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pinfo.renderPass = pass;
    pinfo.framebuffer = framebuffers[vk().current_frame].buf;
    pinfo.renderArea.offset = {0, 0};
    pinfo.renderArea.extent = vk().swapchain.extent;
    return pinfo;
}

void Manager::Compositor::composite(VkCommandBuffer& cmds, ImageView& view) {

    update_img(view);

    VkRenderPassBeginInfo begin = pass_info();

    vkCmdBeginRenderPass(vk().frames[vk().current_frame].composite, &begin,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, p_layout, 0, 1,
                            &descriptor_sets[vk().current_frame], 0, nullptr);
    vkCmdDraw(cmds, 4, 1, 0, 0);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmds);

    vkCmdEndRenderPass(vk().frames[vk().current_frame].composite);
}

void Manager::Compositor::update_img(const ImageView& view) {

    VkDescriptorImageInfo img_info = {};
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView = view.view;
    img_info.sampler = sampler.sampler;

    VkWriteDescriptorSet img_write = {};
    img_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    img_write.dstSet = descriptor_sets[vk().current_frame];
    img_write.dstBinding = 0;
    img_write.dstArrayElement = 0;
    img_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    img_write.descriptorCount = 1;
    img_write.pImageInfo = &img_info;

    vkUpdateDescriptorSets(vk().device(), 1, &img_write, 0, nullptr);
}

void Manager::Compositor::create_desc() {

    VkDescriptorSetLayoutBinding sampler_bind = {};
    sampler_bind.binding = 0;
    sampler_bind.descriptorCount = 1;
    sampler_bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_bind.pImmutableSamplers = nullptr;
    sampler_bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {sampler_bind};

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(vk().device(), &layout_info, nullptr, &d_layout));

    std::vector<VkDescriptorSetLayout> layouts(MAX_IN_FLIGHT, d_layout);

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk().pool();
    alloc_info.descriptorSetCount = MAX_IN_FLIGHT;
    alloc_info.pSetLayouts = layouts.data();

    descriptor_sets.resize(MAX_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(vk().device(), &alloc_info, descriptor_sets.data()));
}

void Manager::Compositor::destroy_swap() {
    framebuffers.clear();
    vkDestroyRenderPass(vk().device(), pass, nullptr);
    vkDestroyPipeline(vk().device(), pipeline, nullptr);
    vkDestroyPipelineLayout(vk().device(), p_layout, nullptr);
    pass = {};
    pipeline = {};
    p_layout = {};
}

void Manager::Compositor::destroy() {
    destroy_swap();
    vkFreeDescriptorSets(vk().device(), vk().pool(), descriptor_sets.size(),
                         descriptor_sets.data());
    descriptor_sets.clear();
    vkDestroyDescriptorSetLayout(vk().device(), d_layout, nullptr);
    sampler.destroy();
}

void Manager::Compositor::create_fbs() {

    Swapchain& swap = vk().swapchain;
    for(size_t i = 0; i < swap.slots.size(); i++) {
        std::vector<std::reference_wrapper<ImageView>> views = {swap.slots[i].view};

        Framebuffer fb;
        fb.recreate(swap.extent.width, swap.extent.height, pass, views);
        framebuffers.push_back(std::move(fb));
    }
}

void Manager::Compositor::create_swap() {
    create_pass();
    create_fbs();
    create_pipe();
}

void Manager::Compositor::init() {
    create_desc();
    create_swap();
    sampler.recreate(VK_FILTER_NEAREST, VK_FILTER_NEAREST);
}

void Manager::Compositor::create_pipe() {

    Shader v_mod(File::read("shaders/out.vert.spv").value());
    Shader f_mod(File::read("shaders/out.frag.spv").value());

    VkPipelineShaderStageCreateInfo stage_info[2] = {};

    stage_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_info[0].module = v_mod.shader;
    stage_info[0].pName = "main";

    stage_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_info[1].module = f_mod.shader;
    stage_info[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo v_in_info = {};
    v_in_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo in_asm_info = {};
    in_asm_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    in_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    in_asm_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)vk().swapchain.extent.width;
    viewport.height = (float)vk().swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = vk().swapchain.extent;

    VkPipelineViewportStateCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    view_info.viewportCount = 1;
    view_info.pViewports = &viewport;
    view_info.scissorCount = 1;
    view_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.depthClampEnable = VK_FALSE;
    raster_info.rasterizerDiscardEnable = VK_FALSE;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.lineWidth = 1.0f;
    raster_info.cullMode = VK_CULL_MODE_BACK_BIT;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo msaa_info = {};
    msaa_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa_info.sampleShadingEnable = VK_FALSE;
    msaa_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend = {};
    color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend.blendEnable = VK_TRUE;
    color_blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.logicOpEnable = VK_FALSE;
    blend_info.logicOp = VK_LOGIC_OP_COPY;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &color_blend;

    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &d_layout;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &p_layout));

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stage_info;
    pipeline_info.pVertexInputState = &v_in_info;
    pipeline_info.pInputAssemblyState = &in_asm_info;
    pipeline_info.pViewportState = &view_info;
    pipeline_info.pRasterizationState = &raster_info;
    pipeline_info.pMultisampleState = &msaa_info;
    pipeline_info.pColorBlendState = &blend_info;
    pipeline_info.pDynamicState = nullptr;
    pipeline_info.layout = p_layout;
    pipeline_info.renderPass = pass;
    pipeline_info.subpass = 0;

    VK_CHECK(
        vkCreateGraphicsPipelines(vk().device(), nullptr, 1, &pipeline_info, nullptr, &pipeline));
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

    info.dev_ext.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    info.dev_ext.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    info.dev_ext.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    info.dev_ext.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
    info.dev_ext.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    info.dev_ext.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    info.dev_ext.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

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
            g.prop_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            g.prop_2.pNext = &rtx.properties;
            rtx.properties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            rtx.vkGetPhysicalDeviceProperties2(g.device, &g.prop_2);
        }
        {
            g.features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            g.features.pNext = &g.addr_features;
            g.addr_features.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            vkGetPhysicalDeviceFeatures2(g.device, &g.features);
        }
        {
            vkGetPhysicalDeviceMemoryProperties(g.device, &g.mem_prop);
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

        if(!g.features.features.samplerAnisotropy) {
            info("Device % does not support anisotropic sampling.", name);
            continue;
        }

        if(!g.addr_features.bufferDeviceAddress) {
            info("Device % does not support device buffer addresses.", name);
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

    VkPhysicalDeviceBufferDeviceAddressFeatures buf_features = {};
    buf_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    buf_features.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceRayQueryFeaturesKHR ray_features = {};
    ray_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    ray_features.rayQuery = VK_TRUE;
    ray_features.pNext = &buf_features;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rp_features = {};
    rp_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rp_features.rayTracingPipeline = VK_TRUE;
    rp_features.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
    rp_features.rayTraversalPrimitiveCulling = VK_TRUE;
    rp_features.pNext = &ray_features;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR ac_features = {};
    ac_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    ac_features.accelerationStructure = VK_TRUE;
    ac_features.pNext = &rp_features;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features.samplerAnisotropy = VK_TRUE;
    features2.pNext = &ac_features;

    VkDeviceCreateInfo dev_info = {};
    dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dev_info.pNext = &features2;
    dev_info.queueCreateInfoCount = q_info.size();
    dev_info.pQueueCreateInfos = q_info.data();
    dev_info.pEnabledFeatures = nullptr;
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

    frames.resize(MAX_IN_FLIGHT);
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
            VK_CHECK(vkAllocateCommandBuffers(gpu.device, &alloc_info, &frame.composite));
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

void Manager::Compositor::create_pass() {

    VkAttachmentDescription color = {};
    color.format = vk().swapchain.format.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref = {};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

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

    VkAttachmentDescription attachments[] = {color};

    VkRenderPassCreateInfo pinfo = {};
    pinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pinfo.attachmentCount = 1;
    pinfo.pAttachments = attachments;
    pinfo.subpassCount = 1;
    pinfo.pSubpasses = &subpass;
    pinfo.dependencyCount = 1;
    pinfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(vk().device(), &pinfo, nullptr, &pass));
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

    std::array<VkDescriptorPoolSize, 12> pool_sizes = {
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
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1024},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1024}};

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
