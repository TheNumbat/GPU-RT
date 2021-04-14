
#include "vulkan.h"
#include "imgui_impl_vulkan.h"

#include <lib/log.h>
#include <SDL2/SDL_vulkan.h>
#include <util/image.h>
#include <util/files.h>

namespace VK {

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<unsigned short> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

VkVertexInputBindingDescription Vertex::bind_desc() {
    VkVertexInputBindingDescription desc = {};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::attr_descs() {

    std::array<VkVertexInputAttributeDescription, 3> ret;

    ret[0].binding = 0;
    ret[0].location = 0;
    ret[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    ret[0].offset = offsetof(Vertex, pos);

    ret[1].binding = 0;
    ret[1].location = 1;
    ret[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    ret[1].offset = offsetof(Vertex, color);

    ret[2].binding = 0;
    ret[2].location = 2;
    ret[2].format = VK_FORMAT_R32G32_SFLOAT;
    ret[2].offset = offsetof(Vertex, tex_coord);

    return ret;
}

#define VK_CHECK(f)                                                                                \
    do {                                                                                           \
        VkResult res = (f);                                                                        \
        if(res != VK_SUCCESS) {                                                                    \
            die("VK_CHECK: %", vk_err_str(res));                                                   \
        }                                                                                          \
    } while(0)

static std::string vk_err_str(VkResult errorCode) {
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

void Manager::begin_frame() {

    if(minimized) return;

    Frame& frame = frames[current_frame];
    Swapchain_Image& image = swapchain.images[current_img];

    vkWaitForFences(gpu.device, 1, &frame.fence, VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(gpu.device, swapchain.swapchain, UINT64_MAX,
                                            frame.avail, VK_NULL_HANDLE, &current_img);

    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    } else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        die("Failed to acquire next image: %", vk_err_str(result));
    }

    if(image.frame_fence != VK_NULL_HANDLE) {
        vkWaitForFences(gpu.device, 1, &image.frame_fence, VK_TRUE, UINT64_MAX);
    }
    image.frame_fence = frame.fence;

    update_uniforms();

    ImGui_ImplVulkan_NewFrame();
}

void Manager::submit_frame(std::vector<VkCommandBuffer>&& buffers) {

    Frame& frame = frames[current_frame];
    Swapchain_Image& image = swapchain.images[current_img];

    if(frame.secondary.size())
        vkFreeCommandBuffers(gpu.device, command_pool, frame.secondary.size(), frame.secondary.data());
        
    for(VkCommandBuffer& buf : buffers) {
        vkEndCommandBuffer(buf);
    }

    VK_CHECK(vkResetCommandBuffer(frame.primary, 0));

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(frame.primary, &begin_info));

    VkRenderPassBeginInfo pass_info = {};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass_info.renderPass = output_pass;
    pass_info.framebuffer = image.framebuffer;
    pass_info.renderArea.offset = {0, 0};
    pass_info.renderArea.extent = swapchain.extent;

    VkClearValue clears[2] = {};
    clears[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clears[1].depthStencil = {0.0f, 0};
    pass_info.clearValueCount = 2;
    pass_info.pClearValues = clears;

    vkCmdBeginRenderPass(frame.primary, &pass_info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    vkCmdExecuteCommands(frame.primary, buffers.size(), buffers.data());
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

    frame.secondary = std::move(buffers);
}

std::pair<unsigned int,unsigned int> Swapchain::dim() {
    return {extent.width, extent.height};
}

float Swapchain::aspect_ratio() {
    return (float)extent.width / extent.height;
}

void Manager::update_uniforms() {

    static unsigned int start = SDL_GetTicks();

    unsigned int current = SDL_GetTicks();
    float time = (current - start) / 1000.0f;

    Uniforms ubo;
    ubo.M = Mat4::rotate(time * 90.0f, {0.0f, 0.0f, 1.0f});
    ubo.V = Mat4::look_at({2.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    ubo.P = Mat4::project(90.0f, swapchain.aspect_ratio(), 0.01f);

    void* data;
    vkMapMemory(gpu.device, uniform_buffers[current_frame].second, 0, sizeof(Uniforms), 0, &data);
    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(gpu.device, uniform_buffers[current_frame].second);
}

void Manager::trigger_resize() {
    needs_resize = true;
}

void Manager::end_frame() {

    if(minimized) {
        recreate_swapchain();
        return;
    }

    std::vector<VkCommandBuffer> buffers;
    buffers.push_back(render_square());
    buffers.push_back(render_imgui());
    submit_frame(std::move(buffers));

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
        die("Failed to present swapchain image: %", vk_err_str(result));
    }

    current_frame = (current_frame + 1) % Frame::MAX_IN_FLIGHT;
}

VkCommandBuffer Manager::render_imgui() {

    VkCommandBuffer cmds = allocate_buf_secondary();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmds);
    return cmds;
}

VkCommandBuffer Manager::allocate_buf_secondary() {

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
    inherit_info.framebuffer = swapchain.images[i].framebuffer;
    inherit_info.renderPass = output_pass;
    inherit_info.subpass = 0;

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    begin_info.pInheritanceInfo = &inherit_info;

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
    return command_buffer;
}

VkCommandBuffer Manager::render_square() {

    VkBuffer vertex_buffers[] = {vertex_buffer.first};
    VkDeviceSize offsets[] = {0};

    VkCommandBuffer cmds = allocate_buf_secondary();

    vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
    vkCmdBindVertexBuffers(cmds, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(cmds, index_buffer.first, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1,
                            &descriptor_sets[current_frame], 0, nullptr);
    vkCmdDrawIndexed(cmds, indices.size(), 1, 0, 0, 0);

    return cmds;
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

    create_depth_buf();
    create_framebuffers();

    // rendering quad pipeline; move?
    create_pipeline();

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

    create_command_pool();
    create_descriptor_pool();
    create_frames();

    create_swapchain();
    create_output_pass();
    
    create_depth_buf();
    create_framebuffers();

    // rendering quad
    create_descriptor_set_layout();
    create_pipeline();
    create_texture();
    create_texture_view_and_sampler();
    create_data_buffers();
    create_uniform_buffers();
    create_descriptor_sets();

    init_imgui();
}

void Manager::init_imgui() {

    ImGui_ImplVulkan_InitInfo init = {};
    init.Instance = info.instance;
    init.PhysicalDevice = gpu.data->device;
    init.Device = gpu.device;
    init.QueueFamily = gpu.data->graphics_idx;
    init.Queue = gpu.graphics_queue;
    init.DescriptorPool = descriptor_pool;
    init.MinImageCount = swapchain.images.size();
    init.ImageCount = swapchain.images.size();
    init.CheckVkResultFn = vk_check_fn;
    ImGui_ImplVulkan_Init(&init, output_pass);

    VkCommandBuffer create_buf = allocate_buf_ots();

    ImGui_ImplVulkan_CreateFontsTexture(create_buf);

    run_wait_free_buf(create_buf);

    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Manager::run_wait_free_buf(VkCommandBuffer cmds) {

    vkEndCommandBuffer(cmds);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmds;

    VK_CHECK(vkQueueSubmit(gpu.graphics_queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(gpu.graphics_queue));

    vkFreeCommandBuffers(gpu.device, command_pool, 1, &cmds);
}

VkCommandBuffer Manager::allocate_buf_ots() {

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

    // Rendering quad
    {
        vkDestroyPipeline(gpu.device, graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(gpu.device, pipeline_layout, nullptr);
        pipeline_layout = {};
        graphics_pipeline = {};
    }

    vkDestroyImageView(gpu.device, depth_view, nullptr);
    vkDestroyImage(gpu.device, depth_image.first, nullptr);
    vkFreeMemory(gpu.device, depth_image.second, nullptr);

    vkDestroyRenderPass(gpu.device, output_pass, nullptr);
    output_pass = {};

    for(Swapchain_Image& image : swapchain.images) {
        vkDestroyImageView(gpu.device, image.view, nullptr);
        vkDestroyFramebuffer(gpu.device, image.framebuffer, nullptr);
    }
    vkDestroySwapchainKHR(gpu.device, swapchain.swapchain, nullptr);
    swapchain.images.clear();
}

void Manager::destroy() {

    VK_CHECK(vkDeviceWaitIdle(gpu.device));

    ImGui_ImplVulkan_Shutdown();
    destroy_swapchain();

    // Rendering quad
    {
        vkDestroySampler(gpu.device, texture_sampler, nullptr);
        vkDestroyImageView(gpu.device, texture_view, nullptr);
        vkDestroyImage(gpu.device, texture.first, nullptr);
        vkFreeMemory(gpu.device, texture.second, nullptr);

        vkFreeDescriptorSets(gpu.device, descriptor_pool, descriptor_sets.size(),
                             descriptor_sets.data());
        descriptor_sets.clear();

        for(auto& buf : uniform_buffers) {
            vkFreeMemory(gpu.device, buf.second, nullptr);
            vkDestroyBuffer(gpu.device, buf.first, nullptr);
        }
        uniform_buffers.clear();

        vkDestroyDescriptorSetLayout(gpu.device, descriptor_layout, nullptr);

        vkDestroyBuffer(gpu.device, index_buffer.first, nullptr);
        vkFreeMemory(gpu.device, index_buffer.second, nullptr);
        vkDestroyBuffer(gpu.device, vertex_buffer.first, nullptr);
        vkFreeMemory(gpu.device, vertex_buffer.second, nullptr);
    }

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

    vkDestroyDevice(gpu.device, nullptr);
    vkDestroySurfaceKHR(info.instance, swapchain.surface, nullptr);
    destroy_debug_callback();

    vkDestroyInstance(info.instance, nullptr);

    gpu = {};
    command_pool = {};
    descriptor_pool = {};

    info = Info();
    swapchain = Swapchain();
}

static VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
                               VkDebugUtilsMessageTypeFlagsEXT type,
                               const VkDebugUtilsMessengerCallbackDataEXT *data,
                               void *user_data)
{
    // Ignore VUID-VkSwapchainCreateInfoKHR-imageExtent-01274
    // cf. https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/1340
    if (data->messageIdNumber == 0x7cd0911d)
        return false;

    std::string message(data->pMessage);

    // Ignore these
    if(message.starts_with("Device Extension") || message.starts_with("Unloading layer library") || message.starts_with("Loading layer library")) {
        return false;
    }

    if(sev == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT || sev == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) 
        info("[VK] %s (%d)", message.c_str(), data->messageIdNumber);
    else
        warn("[VK] %s", message.c_str());

    for (unsigned int i = 0; i < data->queueLabelCount; i++)
        info("\tduring %s", data->pQueueLabels[i].pLabelName);
    for (unsigned int i = 0; i < data->cmdBufLabelCount; i++)
        info("\tinside %s", data->pCmdBufLabels[i].pLabelName);
    
    for (unsigned int i = 0; i < data->objectCount; i++) {
        const VkDebugUtilsObjectNameInfoEXT *obj = &data->pObjects[i];
        info("\tusing %s: %s (%zu)", vk_obj_type(obj->objectType).c_str(), obj->pObjectName ? obj->pObjectName : "?", obj->objectHandle);
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
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(info.instance, "vkCreateDebugUtilsMessengerEXT");
    if(!func) die("Could not find vkCreateDebugUtilsMessengerEXT");
    VK_CHECK(func(info.instance, &callback, nullptr, &info.debug_callback_info));
}

void Manager::destroy_debug_callback() {

    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(info.instance, "vkDestroyDebugUtilsMessengerEXT");
    if(!func) die("Could not find vkDestroyDebugUtilsMessengerEXT");
    func(info.instance, info.debug_callback_info, nullptr);
}

void Manager::create_instance() {

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "GPURT";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 2);
    app_info.pEngineName = "GPURT";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 2);
    app_info.apiVersion = VK_API_VERSION_1_1;

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
    if(!SDL_Vulkan_GetInstanceExtensions(window, &sdl_count, info.inst_ext.data() + info.inst_ext.size() - sdl_count)) {
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
            die("Failed to create VkInstance: %", vk_err_str(inst));
        }
    }

    if(!SDL_Vulkan_CreateSurface(window, info.instance, &swapchain.surface)) {
        die("Failed to create SDL VkSurface: %", SDL_GetError());
    }

    unsigned int total_extensions = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &total_extensions, nullptr));
    info.extensions.clear();
    info.extensions.resize(total_extensions);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &total_extensions, info.extensions.data()));
}

void Manager::enumerate_gpus() {

    unsigned int devices = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(info.instance, &devices, nullptr));
    if(!devices) {
        die("Found no GPUs.");
    }

    std::vector<VkPhysicalDevice> phys_list;
    phys_list.resize(devices);
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
            VK_CHECK(
                vkGetPhysicalDeviceSurfaceFormatsKHR(g.device, swapchain.surface, &num_fmts, nullptr));
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

bool GPU::supports(const std::vector<const char*>& extensions) {

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

    for(Swapchain_Image& image : swapchain.images) {

        VkImageView attachments[] = {image.view, depth_view};

        VkFramebufferCreateInfo fb_info{};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = output_pass;
        fb_info.attachmentCount = 2;
        fb_info.pAttachments = attachments;
        fb_info.width = swapchain.extent.width;
        fb_info.height = swapchain.extent.height;
        fb_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(gpu.device, &fb_info, nullptr, &image.framebuffer));
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

    unsigned int queue_indices[] = {(unsigned int)gpu.data->graphics_idx, (unsigned int)gpu.data->present_idx};

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

    swapchain.images.resize(images);

    std::vector<VkImage> image_data(images, VkImage());

    VK_CHECK(vkGetSwapchainImagesKHR(gpu.device, swapchain.swapchain, &images, image_data.data()));
    if(!images) {
        die("Failed to get any images from vk swapchain!");
    }

    for(unsigned int i = 0; i < images; i++) {

        Swapchain_Image& image = swapchain.images[i];
        image.image = image_data[i];
        image.view = create_image_view(image.image, swapchain.format.format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

VkShaderModule Manager::create_shader(const std::vector<unsigned char>& data) {

    VkShaderModuleCreateInfo mod_info = {};
    mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mod_info.codeSize = data.size();
    mod_info.pCode = (const uint32_t*)data.data();

    VkShaderModule mod = {};
    VK_CHECK(vkCreateShaderModule(gpu.device, &mod_info, nullptr, &mod));
    return mod;
}

void Manager::create_pipeline() {

    std::vector<unsigned char> vert_code = File::read("shaders/1.vert.spv").value();
    std::vector<unsigned char> frag_code = File::read("shaders/1.frag.spv").value();

    VkShaderModule v_mod = create_shader(vert_code);
    VkShaderModule f_mod = create_shader(frag_code);

    VkPipelineShaderStageCreateInfo stage_info[2] = {};

    stage_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_info[0].module = v_mod;
    stage_info[0].pName = "main";

    stage_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_info[1].module = f_mod;
    stage_info[1].pName = "main";

    auto binding_desc = Vertex::bind_desc();
    auto attr_descs = Vertex::attr_descs();

    VkPipelineVertexInputStateCreateInfo v_in_info = {};
    v_in_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    v_in_info.vertexBindingDescriptionCount = 1;
    v_in_info.pVertexBindingDescriptions = &binding_desc;
    v_in_info.vertexAttributeDescriptionCount = attr_descs.size();
    v_in_info.pVertexAttributeDescriptions = attr_descs.data();

    VkPipelineInputAssemblyStateCreateInfo in_asm_info = {};
    in_asm_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    in_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    in_asm_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchain.extent.width;
    viewport.height = (float)swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchain.extent;

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
    raster_info.depthBiasConstantFactor = 0.0f; // Optional
    raster_info.depthBiasClamp = 0.0f;          // Optional
    raster_info.depthBiasSlopeFactor = 0.0f;    // Optional

    VkPipelineMultisampleStateCreateInfo msaa_info = {};
    msaa_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa_info.sampleShadingEnable = VK_FALSE;
    msaa_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msaa_info.minSampleShading = 1.0f;          // Optional
    msaa_info.pSampleMask = nullptr;               // Optional
    msaa_info.alphaToCoverageEnable = VK_FALSE; // Optional
    msaa_info.alphaToOneEnable = VK_FALSE;      // Optional

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
    blend_info.logicOp = VK_LOGIC_OP_COPY; // Optional
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &color_blend;
    blend_info.blendConstants[0] = 0.0f; // Optional
    blend_info.blendConstants[1] = 0.0f; // Optional
    blend_info.blendConstants[2] = 0.0f; // Optional
    blend_info.blendConstants[3] = 0.0f; // Optional

#if 0
	VkDynamicState dynamic[] = {
		VK_DYNAMIC_STATE_VIEWPORT
	};
	VkPipelineDynamicStateCreateInfo dynamic_info{};
	dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_info.dynamicStateCount = 1;
	dynamic_info.pDynamicStates = dynamic;
#endif

    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &descriptor_layout;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(gpu.device, &layout_info, nullptr, &pipeline_layout));

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_info.depthTestEnable = VK_TRUE;
    depth_info.depthWriteEnable = VK_TRUE;
    depth_info.depthCompareOp = VK_COMPARE_OP_GREATER;
    depth_info.depthBoundsTestEnable = VK_FALSE;
    depth_info.minDepthBounds = 0.0f; // Optional
    depth_info.maxDepthBounds = 1.0f; // Optional
    depth_info.stencilTestEnable = VK_FALSE;
    depth_info.front = {}; // Optional
    depth_info.back = {}; // Optional

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stage_info;
    pipeline_info.pVertexInputState = &v_in_info;
    pipeline_info.pInputAssemblyState = &in_asm_info;
    pipeline_info.pViewportState = &view_info;
    pipeline_info.pRasterizationState = &raster_info;
    pipeline_info.pMultisampleState = &msaa_info;
    pipeline_info.pDepthStencilState = &depth_info;
    pipeline_info.pColorBlendState = &blend_info;
    pipeline_info.pDynamicState = nullptr;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = output_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipeline_info.basePipelineIndex = -1;              // Optional

    VK_CHECK(vkCreateGraphicsPipelines(gpu.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                       &graphics_pipeline));

    vkDestroyShaderModule(gpu.device, v_mod, nullptr);
    vkDestroyShaderModule(gpu.device, f_mod, nullptr);
}

VkFormat Manager::choose_supported_format(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags features) {

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
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

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

std::pair<VkBuffer, VkDeviceMemory> Manager::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                                      VkMemoryPropertyFlags properties) {

    VkBuffer out_buf;
    VkDeviceMemory out_mem;

    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = size;
    buf_info.usage = usage;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(gpu.device, &buf_info, nullptr, &out_buf));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(gpu.device, out_buf, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = choose_memory_type(mem_reqs.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(gpu.device, &alloc_info, nullptr, &out_mem));

    vkBindBufferMemory(gpu.device, out_buf, out_mem, 0);

    return {out_buf, out_mem};
}

void Manager::create_data_buffers() {

    {
        VkDeviceSize buf_size = sizeof(vertices[0]) * vertices.size();

        auto staging = create_buffer(buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data;
        vkMapMemory(gpu.device, staging.second, 0, buf_size, 0, &data);
        std::memcpy(data, vertices.data(), (size_t)buf_size);
        vkUnmapMemory(gpu.device, staging.second);

        vertex_buffer = create_buffer(
            buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        copy_buffer(staging.first, vertex_buffer.first, buf_size);

        vkDestroyBuffer(gpu.device, staging.first, nullptr);
        vkFreeMemory(gpu.device, staging.second, nullptr);
    }
    {
        VkDeviceSize buf_size = sizeof(indices[0]) * indices.size();

        auto staging = create_buffer(buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data;
        vkMapMemory(gpu.device, staging.second, 0, buf_size, 0, &data);
        std::memcpy(data, indices.data(), buf_size);
        vkUnmapMemory(gpu.device, staging.second);

        index_buffer = create_buffer(
            buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        copy_buffer(staging.first, index_buffer.first, buf_size);

        vkDestroyBuffer(gpu.device, staging.first, nullptr);
        vkFreeMemory(gpu.device, staging.second, nullptr);
    }
}

void Manager::create_uniform_buffers() {

    VkDeviceSize buf_size = sizeof(Uniforms);

    uniform_buffers.resize(Frame::MAX_IN_FLIGHT);

    for(unsigned int i = 0; i < Frame::MAX_IN_FLIGHT; i++) {
        uniform_buffers[i] = create_buffer(buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void Manager::copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {

    VkCommandBuffer copy_buf = allocate_buf_ots();

    VkBufferCopy region = {};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(copy_buf, src, dst, 1, &region);

    run_wait_free_buf(copy_buf);
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

void Manager::create_descriptor_set_layout() {

    VkDescriptorSetLayoutBinding ubo_bind = {};
    ubo_bind.binding = 0;
    ubo_bind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_bind.descriptorCount = 1;
    ubo_bind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    ubo_bind.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding sampler_bind = {};
    sampler_bind.binding = 1;
    sampler_bind.descriptorCount = 1;
    sampler_bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_bind.pImmutableSamplers = nullptr;
    sampler_bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {ubo_bind, sampler_bind};

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 2;
    layout_info.pBindings = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(gpu.device, &layout_info, nullptr, &descriptor_layout));
}

void Manager::create_descriptor_pool() {

    std::array<VkDescriptorPoolSize, 11> pool_sizes = 
        {VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1024},
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

void Manager::create_descriptor_sets() {

    std::vector<VkDescriptorSetLayout> layouts(Frame::MAX_IN_FLIGHT, descriptor_layout);

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = Frame::MAX_IN_FLIGHT;
    alloc_info.pSetLayouts = layouts.data();

    descriptor_sets.resize(Frame::MAX_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(gpu.device, &alloc_info, descriptor_sets.data()));

    for(unsigned int i = 0; i < Frame::MAX_IN_FLIGHT; i++) {

        VkDescriptorBufferInfo buf_info = {};
        buf_info.buffer = uniform_buffers[i].first;
        buf_info.offset = 0;
        buf_info.range = sizeof(Uniforms);

        VkDescriptorImageInfo img_info = {};
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_info.imageView = texture_view;
        img_info.sampler = texture_sampler;

        VkWriteDescriptorSet desc_writes[2] = {};
        
        desc_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_writes[0].dstSet = descriptor_sets[i];
        desc_writes[0].dstBinding = 0;
        desc_writes[0].dstArrayElement = 0;
        desc_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_writes[0].descriptorCount = 1;
        desc_writes[0].pBufferInfo = &buf_info;

        desc_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_writes[1].dstSet = descriptor_sets[i];
        desc_writes[1].dstBinding = 1;
        desc_writes[1].dstArrayElement = 0;
        desc_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_writes[1].descriptorCount = 1;
        desc_writes[1].pImageInfo = &img_info;

        vkUpdateDescriptorSets(gpu.device, 2, desc_writes, 0, nullptr);
    }
}

std::pair<VkImage, VkDeviceMemory> Manager::create_image(unsigned int width, unsigned int height, VkFormat format,
                                                    VkImageTiling tiling, VkImageUsageFlags usage,
                                                    VkMemoryPropertyFlags properties) {

    std::pair<VkImage, VkDeviceMemory> image;

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
    img_info.usage = usage;
    img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    img_info.flags = 0;

    VK_CHECK(vkCreateImage(gpu.device, &img_info, nullptr, &image.first));

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(gpu.device, image.first, &mem_req);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = choose_memory_type(mem_req.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(gpu.device, &alloc_info, nullptr, &image.second));

    vkBindImageMemory(gpu.device, image.first, image.second, 0);

    return image;
}

void Manager::buffer_to_image(VkBuffer buffer, VkImage image, unsigned int w, unsigned int h) {

    VkCommandBuffer cmds = allocate_buf_ots();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {w, h, 1};

    vkCmdCopyBufferToImage(cmds, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    run_wait_free_buf(cmds);
}

void Manager::transition_image(VkImage image, VkFormat format, VkImageLayout old_l,
                               VkImageLayout new_l) {

    VkCommandBuffer cmds = allocate_buf_ots();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_l;
    barrier.newLayout = new_l;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;

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
    
    } else if(old_l == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_l == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    } else if(old_l == VK_IMAGE_LAYOUT_UNDEFINED && new_l == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {

        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    } else {
        die("Unsupported image layout transition!");
    }

    vkCmdPipelineBarrier(cmds, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    run_wait_free_buf(cmds);
}

void Manager::create_texture() {

    Image img = Image::load("numbat.jpg").value();

    auto [w, h] = img.dim();
    
    VkDeviceSize size = img.bytes();

    auto staging =
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(gpu.device, staging.second, 0, size, 0, &data);
    std::memcpy(data, img.data(), size);
    vkUnmapMemory(gpu.device, staging.second);

    texture = create_image(img.w(), img.h(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    transition_image(texture.first, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    buffer_to_image(staging.first, texture.first, img.w(), img.h());
    transition_image(texture.first, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(gpu.device, staging.first, nullptr);
    vkFreeMemory(gpu.device, staging.second, nullptr);
}

VkImageView Manager::create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect) {

    VkImageView ret;

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
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

    VK_CHECK(vkCreateImageView(gpu.device, &view_info, nullptr, &ret));
    return ret;
}

void Manager::create_texture_view_and_sampler() {

    texture_view = create_image_view(texture.first, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

    VkSamplerCreateInfo sample_info = {};
    sample_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sample_info.magFilter = VK_FILTER_LINEAR;
    sample_info.minFilter = VK_FILTER_LINEAR;
    sample_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sample_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sample_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sample_info.anisotropyEnable = VK_TRUE;
    sample_info.maxAnisotropy = gpu.data->dev_prop.limits.maxSamplerAnisotropy;
    sample_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sample_info.unnormalizedCoordinates = VK_FALSE;
    sample_info.compareEnable = VK_FALSE;
    sample_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sample_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sample_info.mipLodBias = 0.0f;
    sample_info.minLod = 0.0f;
    sample_info.maxLod = 0.0f;

    VK_CHECK(vkCreateSampler(gpu.device, &sample_info, nullptr, &texture_sampler));
}

VkFormat Manager::find_depth_format() {
    return choose_supported_format(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, 
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool Manager::format_has_stencil(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void Manager::create_depth_buf() {

    VkFormat format = find_depth_format();
    depth_image = create_image(swapchain.dim().first, swapchain.dim().second, format, 
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depth_view = create_image_view(depth_image.first, format, VK_IMAGE_ASPECT_DEPTH_BIT);

    transition_image(depth_image.first, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}  

} // namespace VK
