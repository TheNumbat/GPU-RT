
#include "rt.h"
#include <util/files.h>
#include <scene/scene.h>

namespace VK {

RTPipe::~RTPipe() {
    destroy();
}

RTPipe::RTPipe(const Scene& scene) {
    recreate(scene);
}

void RTPipe::recreate(const Scene& scene) {
    pipe.drop();
    build_textures(scene);
    create_desc(scene);
    build_desc(scene);
    create_pipe();
    create_sbt();
    reset_frame();
}

void RTPipe::build_desc(const Scene& scene) {
    
    std::unordered_map<unsigned int, unsigned int> obj_to_idx;
    
    std::vector<Scene_Desc> descs;
    scene.for_objs([&](const Object& obj) {
        Scene_Desc desc;
        desc.index = descs.size();
        desc.model = Mat4::scale(Vec3{scene.scale}) * obj.pose.transform();
        desc.modelIT = desc.model.inverse().T();
        desc.albedo_tex = obj.material.albedo_tex;
        desc.metal_rough_tex = obj.material.metal_rough_tex;
        desc.emissive_tex = obj.material.emissive_tex;
        desc.normal_tex = obj.material.normal_tex;
        desc.albedo = Vec4{obj.material.albedo, 0.0f};
        desc.emissive = Vec4{obj.material.emissive, 0.0f};
        desc.metal_rough = Vec4{obj.material.metal_rough.x, obj.material.metal_rough.y, 0.0f, 0.0f};
        obj_to_idx[obj.id()] = descs.size();
        descs.push_back(desc);
    });

    std::vector<Scene_Light> lights;
    scene.for_objs([&](const Object& obj) {
        if(obj.material.emissive != Vec3{} || obj.material.emissive_tex != -1) {
            Scene_Light light;
            light.index = obj_to_idx[obj.id()];
            light.n_triangles = obj.mesh().inds().size() / 3;
            BBox box = obj.mesh().bbox();
            box.transform(descs[light.index].model);
            light.bmin = Vec4{box.min, 0.0f};
            light.bmax = Vec4{box.max, 0.0f};
            lights.push_back(light);
        }
    });

    consts.n_objs = descs.size();
    consts.n_lights = lights.size();

    desc_buf.drop();
    light_buf.drop();
    
    {
        VkDeviceSize size = descs.size() * sizeof(Scene_Desc);
        desc_buf->recreate(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        desc_buf->write_staged(descs.data(), size);
    }
    {
        VkDeviceSize size = lights.size() * sizeof(Scene_Light);
        light_buf->recreate(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        light_buf->write_staged(lights.data(), size);
    }

    VkDescriptorBufferInfo d_buf_info = {};
    d_buf_info.buffer = desc_buf->buf;
    d_buf_info.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo l_buf_info = {};
    l_buf_info.buffer = light_buf->buf;
    l_buf_info.range = VK_WHOLE_SIZE;

    for(unsigned int i = 0; i < Manager::MAX_IN_FLIGHT; i++) {
        
        VkWriteDescriptorSet d_buf_write = {};
        d_buf_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        d_buf_write.dstSet = pipe->descriptor_sets[i];
        d_buf_write.dstBinding = 1;
        d_buf_write.dstArrayElement = 0;
        d_buf_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        d_buf_write.descriptorCount = 1;
        d_buf_write.pBufferInfo = &d_buf_info;

        VkWriteDescriptorSet l_buf_write = {};
        l_buf_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        l_buf_write.dstSet = pipe->descriptor_sets[i];
        l_buf_write.dstBinding = 2;
        l_buf_write.dstArrayElement = 0;
        l_buf_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        l_buf_write.descriptorCount = 1;
        l_buf_write.pBufferInfo = &l_buf_info;

        VkWriteDescriptorSet writes[] = {d_buf_write, l_buf_write};
        vkUpdateDescriptorSets(vk().device(), 2, writes, 0, nullptr);
    }
}

void RTPipe::destroy() {
    ubos.clear();
    pipe.drop();
}

void RTPipe::recreate_swap(const Scene& scene) {
    pipe->destroy_swap();
    create_pipe();
}

void RTPipe::update_uniforms(const Camera& cam) {

    UBO ubo;
    ubo.camera.V = cam.get_view();
    ubo.camera.P = cam.get_proj();
    ubo.camera.iV = ubo.camera.V.inverse();
    ubo.camera.iP = ubo.camera.P.inverse();
    ubo.restir.new_samples = res_samples;
    ubo.restir.prev_PV = old_cam.P * old_cam.V;
    ubo.restir.temporal_multiplier = temporal_scale;

    if(consts.frame >= 0 && std::memcmp(&ubo.camera, &old_cam, sizeof(CameraConstants))) {
        reset_frame();
        old_cam = ubo.camera;
    }

    ubos[vk().frame()]->write(&ubo, sizeof(ubo));
}

void RTPipe::use_image(const ImageView& out) {

    unsigned int i = vk().frame();
    VkDescriptorImageInfo img_info = {};
    img_info.imageLayout = out.img().layout;
    img_info.imageView = out.view;

    VkWriteDescriptorSet img_write = {};
    img_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    img_write.dstSet = pipe->descriptor_sets[i];
    img_write.dstBinding = 7;
    img_write.dstArrayElement = 0;
    img_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    img_write.descriptorCount = 1;
    img_write.pImageInfo = &img_info;

    vkUpdateDescriptorSets(vk().device(), 1, &img_write, 0, nullptr);
}

void RTPipe::use_accel(const Accel& tlas) {

    VkWriteDescriptorSetAccelerationStructureKHR acc_info = {};
    acc_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    acc_info.accelerationStructureCount = 1;
    acc_info.pAccelerationStructures = &tlas.accel;

    VkWriteDescriptorSet acw = {};
    acw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    acw.dstSet = pipe->descriptor_sets[vk().frame()];
    acw.dstBinding = 6;
    acw.dstArrayElement = 0;
    acw.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    acw.descriptorCount = 1;
    acw.pNext = &acc_info;

    vkUpdateDescriptorSets(vk().device(), 1, &acw, 0, nullptr);
}

void RTPipe::resize_temporal_stuff() {

    res0.drop();
    res1.drop();

    res0->recreate(sizeof(Reservoir) * prev_ext.width * prev_ext.height, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    res1->recreate(sizeof(Reservoir) * prev_ext.width * prev_ext.height, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    pos_image[0].drop();
    pos_image[1].drop();
    norm_image[0].drop();
    norm_image[1].drop();
    alb_image[0].drop();
    alb_image[1].drop();

    pos_image[0]->recreate(prev_ext.width, prev_ext.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    pos_image[1]->recreate(prev_ext.width, prev_ext.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    norm_image[0]->recreate(prev_ext.width, prev_ext.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    norm_image[1]->recreate(prev_ext.width, prev_ext.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    alb_image[0]->recreate(prev_ext.width, prev_ext.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    alb_image[1]->recreate(prev_ext.width, prev_ext.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    pos_image_view[0].drop();
    pos_image_view[1].drop();
    norm_image_view[0].drop();
    norm_image_view[1].drop();
    alb_image_view[0].drop();
    alb_image_view[1].drop();

    pos_image_view[0]->recreate(pos_image[0], VK_IMAGE_ASPECT_COLOR_BIT);
    pos_image_view[1]->recreate(pos_image[1], VK_IMAGE_ASPECT_COLOR_BIT);
    norm_image_view[0]->recreate(norm_image[0], VK_IMAGE_ASPECT_COLOR_BIT);
    norm_image_view[1]->recreate(norm_image[1], VK_IMAGE_ASPECT_COLOR_BIT);
    alb_image_view[0]->recreate(alb_image[0], VK_IMAGE_ASPECT_COLOR_BIT);
    alb_image_view[1]->recreate(alb_image[1], VK_IMAGE_ASPECT_COLOR_BIT);

    pos_image[0]->transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    norm_image[0]->transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    alb_image[0]->transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    pos_image[1]->transition(VK_IMAGE_LAYOUT_GENERAL);
    norm_image[1]->transition(VK_IMAGE_LAYOUT_GENERAL);
    alb_image[1]->transition(VK_IMAGE_LAYOUT_GENERAL);
}

void RTPipe::bind_temporal_stuff(VkCommandBuffer cmds) {

    bool even = vk().frame() % 2;

    VkDescriptorBufferInfo res_b = {};
    res_b.buffer = even ? res0->buf : res1->buf;
    res_b.offset = 0;
    res_b.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo prev_res_b = {};
    prev_res_b.buffer = even ? res1->buf : res0->buf;
    prev_res_b.offset = 0;
    prev_res_b.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet res_w = {};
    res_w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    res_w.dstSet = pipe->descriptor_sets[vk().frame()];
    res_w.dstBinding = 8;
    res_w.dstArrayElement = 0;
    res_w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    res_w.descriptorCount = 1;
    res_w.pBufferInfo = &res_b;

    VkWriteDescriptorSet prev_res_w = {};
    prev_res_w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    prev_res_w.dstSet = pipe->descriptor_sets[vk().frame()];
    prev_res_w.dstBinding = 9;
    prev_res_w.dstArrayElement = 0;
    prev_res_w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    prev_res_w.descriptorCount = 1;
    prev_res_w.pBufferInfo = &prev_res_b;

    VkDescriptorImageInfo pos_info = {};
    pos_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    pos_info.imageView = pos_image_view[even]->view;

    VkDescriptorImageInfo norm_info = {};
    norm_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    norm_info.imageView = norm_image_view[even]->view;

    VkDescriptorImageInfo alb_info = {};
    alb_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    alb_info.imageView = alb_image_view[even]->view;

    VkDescriptorImageInfo ppos_info = {};
    ppos_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ppos_info.imageView = pos_image_view[!even]->view;
    ppos_info.sampler = gbuf_sampler->sampler;

    VkDescriptorImageInfo pnorm_info = {};
    pnorm_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    pnorm_info.imageView = norm_image_view[!even]->view;
    pnorm_info.sampler = gbuf_sampler->sampler;

    VkDescriptorImageInfo palb_info = {};
    palb_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    palb_info.imageView = alb_image_view[!even]->view;
    palb_info.sampler = gbuf_sampler->sampler;

    VkWriteDescriptorSet pos_write = {};
    pos_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pos_write.dstSet = pipe->descriptor_sets[vk().frame()];
    pos_write.dstBinding = 10;
    pos_write.dstArrayElement = 0;
    pos_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    pos_write.descriptorCount = 1;
    pos_write.pImageInfo = &pos_info;

    VkWriteDescriptorSet norm_write = {};
    norm_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    norm_write.dstSet = pipe->descriptor_sets[vk().frame()];
    norm_write.dstBinding = 11;
    norm_write.dstArrayElement = 0;
    norm_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    norm_write.descriptorCount = 1;
    norm_write.pImageInfo = &norm_info;

    VkWriteDescriptorSet alb_write = {};
    alb_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    alb_write.dstSet = pipe->descriptor_sets[vk().frame()];
    alb_write.dstBinding = 12;
    alb_write.dstArrayElement = 0;
    alb_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    alb_write.descriptorCount = 1;
    alb_write.pImageInfo = &alb_info;

    VkWriteDescriptorSet ppos_write = {};
    ppos_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ppos_write.dstSet = pipe->descriptor_sets[vk().frame()];
    ppos_write.dstBinding = 13;
    ppos_write.dstArrayElement = 0;
    ppos_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ppos_write.descriptorCount = 1;
    ppos_write.pImageInfo = &ppos_info;

    VkWriteDescriptorSet pnorm_write = {};
    pnorm_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pnorm_write.dstSet = pipe->descriptor_sets[vk().frame()];
    pnorm_write.dstBinding = 14;
    pnorm_write.dstArrayElement = 0;
    pnorm_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pnorm_write.descriptorCount = 1;
    pnorm_write.pImageInfo = &pnorm_info;

    VkWriteDescriptorSet palb_write = {};
    palb_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    palb_write.dstSet = pipe->descriptor_sets[vk().frame()];
    palb_write.dstBinding = 15;
    palb_write.dstArrayElement = 0;
    palb_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    palb_write.descriptorCount = 1;
    palb_write.pImageInfo = &palb_info;

    std::vector<VkWriteDescriptorSet> writes = {res_w, prev_res_w, pos_write, norm_write, alb_write, ppos_write, pnorm_write, palb_write};
    vkUpdateDescriptorSets(vk().device(), writes.size(), writes.data(), 0, nullptr);

    pos_image[even]->transition(cmds, VK_IMAGE_LAYOUT_GENERAL);
    norm_image[even]->transition(cmds, VK_IMAGE_LAYOUT_GENERAL);
    alb_image[even]->transition(cmds, VK_IMAGE_LAYOUT_GENERAL);
    pos_image[!even]->transition(cmds, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    norm_image[!even]->transition(cmds, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    alb_image[!even]->transition(cmds, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

bool RTPipe::trace(const Camera& cam, VkCommandBuffer& cmds, VkExtent2D ext) {

    if(ext.width != prev_ext.width || ext.height != prev_ext.height) {
        prev_ext = ext;
        resize_temporal_stuff();
    }

    if(consts.frame >= max_frames) return false;

    consts.clear_col = Vec4{clear, 1.0f};
    consts.env_light = Vec4{env_scale * env, 1.0f};
    consts.samples = samples_per_frame;
    consts.max_depth = max_depth;
    consts.use_normal_map = use_normal_map;
    consts.use_metalness = use_metalness;
    consts.integrator = integrator;
    consts.brdf = brdf;
    consts.use_rr = use_rr;
    consts.max_frame = max_frames;
    consts.qmc = use_qmc;
    consts.use_temporal = use_temporal;
    consts.debug_view = debug_view;
    consts.frame++;

    bind_temporal_stuff(cmds);

    vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipe->pipe);
    vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipe->p_layout, 0, 1,
                            &pipe->descriptor_sets[vk().frame()], 0, nullptr);
    vkCmdPushConstants(cmds, pipe->p_layout,
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR,
                       0, sizeof(RTPipe_Constants), &consts);

    unsigned int groupSize = align_up(vk().rtx.properties.shaderGroupHandleSize,
                                      vk().rtx.properties.shaderGroupBaseAlignment);
    unsigned int groupStride = groupSize;
    VkDeviceAddress sbt_addr = sbt->address();

    using Stride = VkStridedDeviceAddressRegionKHR;
    std::array<Stride, 4> addrs{Stride{sbt_addr + 0u * groupSize, groupStride, groupSize}, // raygen
                                Stride{sbt_addr + 1u * groupSize, groupStride, groupSize}, // miss
                                Stride{sbt_addr + 2u * groupSize, groupStride, groupSize}, // hit
                                Stride{0u, 0u, 0u}};

    vk().rtx.vkCmdTraceRaysKHR(cmds, &addrs[0], &addrs[1], &addrs[2], &addrs[3], ext.width,
                               ext.height, 1);
    return true;
}

void RTPipe::reset_frame() {
    consts.frame = -1;
}

void RTPipe::create_sbt() {

    unsigned int shader_count = 3;
    unsigned int groupHandleSize = vk().rtx.properties.shaderGroupHandleSize;
    unsigned int groupSizeAligned =
        align_up(groupHandleSize, vk().rtx.properties.shaderGroupBaseAlignment);
    unsigned int sbtSize = shader_count * groupSizeAligned;

    std::vector<unsigned char> shaderHandleStorage(sbtSize);

    VK_CHECK(vk().rtx.vkGetRayTracingShaderGroupHandlesKHR(
        vk().device(), pipe->pipe, 0, shader_count, sbtSize, shaderHandleStorage.data()));

    Buffer staging(sbtSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    sbt->recreate(sbtSize,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                  VMA_MEMORY_USAGE_GPU_ONLY);

    unsigned char* data = (unsigned char*)staging.map();

    for(unsigned int g = 0; g < shader_count; g++) {
        std::memcpy(data, shaderHandleStorage.data() + g * groupHandleSize, groupHandleSize);
        data += groupSizeAligned;
    }

    staging.unmap();
    staging.copy_to(sbt);
}

void RTPipe::build_textures(const Scene& scene) {

    const auto& images = scene.images();
    textures.clear();
    texture_views.clear();

    for(const auto& image : images) {
        auto [w,h] = image.dim();
        
        VK::Image img(w, h, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        img.write(image);

        textures.push_back(std::move(img));
        texture_views.push_back(VK::ImageView(textures.back(), VK_IMAGE_ASPECT_COLOR_BIT));
    }

    texture_sampler.drop();
    texture_sampler->recreate(VK_FILTER_LINEAR, VK_FILTER_LINEAR);

    gbuf_sampler.drop();
    gbuf_sampler->recreate(VK_FILTER_NEAREST, VK_FILTER_NEAREST);
}

void RTPipe::create_pipe() {

    pipe->destroy_swap();

    Shader chit(File::read("shaders/rt/rt.rchit.spv").value());
    Shader miss(File::read("shaders/rt/rt.rmiss.spv").value());
    Shader gen(File::read("shaders/rt/rt.rgen.spv").value());

    VkRayTracingShaderGroupCreateInfoKHR groups[3] = {};

    groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;
    groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].generalShader = 0;

    groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].intersectionShader = VK_SHADER_UNUSED_KHR;
    groups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
    groups[1].generalShader = 1;

    groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;
    groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[2].closestHitShader = 2;
    groups[2].generalShader = VK_SHADER_UNUSED_KHR;

    VkPipelineShaderStageCreateInfo stage_info[3] = {};

    stage_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stage_info[0].module = gen.shader;
    stage_info[0].pName = "main";

    stage_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stage_info[1].module = miss.shader;
    stage_info[1].pName = "main";

    stage_info[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stage_info[2].module = chit.shader;
    stage_info[2].pName = "main";

    VkPushConstantRange pushes = {};
    pushes.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    pushes.size = sizeof(RTPipe_Constants);
    pushes.offset = 0;

    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &pipe->d_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &pushes;

    VK_CHECK(vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &pipe->p_layout));

    VkRayTracingPipelineCreateInfoKHR rayPipelineInfo = {};
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rayPipelineInfo.stageCount = 3;
    rayPipelineInfo.pStages = stage_info;
    rayPipelineInfo.groupCount = 3;
    rayPipelineInfo.pGroups = groups;
    rayPipelineInfo.maxPipelineRayRecursionDepth = 1;
    rayPipelineInfo.layout = pipe->p_layout;

    VK_CHECK(vk().rtx.vkCreateRayTracingPipelinesKHR(vk().device(), nullptr, nullptr, 1,
                                                     &rayPipelineInfo, nullptr, &pipe->pipe));
}

void RTPipe::create_desc(const Scene& scene) {

    const int n_objects = scene.size() ? scene.size() : 1;

    VkDescriptorSetLayoutBinding ubo_bind = {};
    ubo_bind.binding = 0;
    ubo_bind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_bind.descriptorCount = 1;
    ubo_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding d_bind = {};
    d_bind.binding = 1;
    d_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    d_bind.descriptorCount = 1;
    d_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding l_bind = {};
    l_bind.binding = 2;
    l_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    l_bind.descriptorCount = 1;
    l_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding v_bind = {};
    v_bind.binding = 3;
    v_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    v_bind.descriptorCount = n_objects;
    v_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding i_bind = {};
    i_bind.binding = 4;
    i_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    i_bind.descriptorCount = n_objects;
    i_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding t_bind = {};
    t_bind.binding = 5;
    t_bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    t_bind.descriptorCount = textures.size();
    t_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding a_bind = {};
    a_bind.binding = 6;
    a_bind.descriptorCount = 1;
    a_bind.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    a_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding store_bind = {};
    store_bind.binding = 7;
    store_bind.descriptorCount = 1;
    store_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    store_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding res_bind = {};
    res_bind.binding = 8;
    res_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    res_bind.descriptorCount = 1;
    res_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding prev_res_bind = {};
    prev_res_bind.binding = 9;
    prev_res_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    prev_res_bind.descriptorCount = 1;
    prev_res_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding store_pos_bind = {};
    store_pos_bind.binding = 10;
    store_pos_bind.descriptorCount = 1;
    store_pos_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    store_pos_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding store_norm_bind = {};
    store_norm_bind.binding = 11;
    store_norm_bind.descriptorCount = 1;
    store_norm_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    store_norm_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding store_alb_bind = {};
    store_alb_bind.binding = 12;
    store_alb_bind.descriptorCount = 1;
    store_alb_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    store_alb_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding read_pos_bind = {};
    read_pos_bind.binding = 13;
    read_pos_bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    read_pos_bind.descriptorCount = 1;
    read_pos_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding read_norm_bind = {};
    read_norm_bind.binding = 14;
    read_norm_bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    read_norm_bind.descriptorCount = 1;
    read_norm_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding read_alb_bind = {};
    read_alb_bind.binding = 15;
    read_alb_bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    read_alb_bind.descriptorCount = 1;
    read_alb_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {ubo_bind, d_bind, l_bind, v_bind, i_bind, t_bind, a_bind, store_bind, res_bind, prev_res_bind, store_pos_bind, store_norm_bind, store_alb_bind, read_pos_bind, read_norm_bind, read_alb_bind};

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = bindings.size();
    layout_info.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(vk().device(), &layout_info, nullptr, &pipe->d_layout));

    std::vector<VkDescriptorSetLayout> layouts(Manager::MAX_IN_FLIGHT, pipe->d_layout);

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk().pool();
    alloc_info.descriptorSetCount = Manager::MAX_IN_FLIGHT;
    alloc_info.pSetLayouts = layouts.data();

    pipe->descriptor_sets.resize(Manager::MAX_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(vk().device(), &alloc_info, pipe->descriptor_sets.data()));

    ubos.resize(Manager::MAX_IN_FLIGHT);
    for(unsigned int i = 0; i < Manager::MAX_IN_FLIGHT; i++) {

        ubos[i]->recreate(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VMA_MEMORY_USAGE_CPU_TO_GPU);

        std::vector<VkDescriptorBufferInfo> vbufs, ibufs;

        VkDescriptorBufferInfo ub = {};
        ub.buffer = ubos[i]->buf;
        ub.offset = 0;
        ub.range = sizeof(UBO);

        scene.for_objs([&](const Object& obj) {
            VkDescriptorBufferInfo vb = {};
            vb.buffer = obj.mesh().vbuf->buf;
            vb.offset = 0;
            vb.range = VK_WHOLE_SIZE;
            VkDescriptorBufferInfo ib = {};
            ib.buffer = obj.mesh().ibuf->buf;
            ib.offset = 0;
            ib.range = VK_WHOLE_SIZE;
            vbufs.push_back(vb);
            ibufs.push_back(ib);
        });

        VkWriteDescriptorSet ubw = {};
        ubw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ubw.dstSet = pipe->descriptor_sets[i];
        ubw.dstBinding = 0;
        ubw.dstArrayElement = 0;
        ubw.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubw.descriptorCount = 1;
        ubw.pBufferInfo = &ub;

        VkDescriptorBufferInfo empty_info = {};
        empty_info.buffer = VK_NULL_HANDLE;
        empty_info.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet vbw = {};
        vbw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vbw.dstSet = pipe->descriptor_sets[i];
        vbw.dstBinding = 3;
        vbw.dstArrayElement = 0;
        vbw.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        if(vbufs.size()) {
            vbw.descriptorCount = vbufs.size();
            vbw.pBufferInfo = vbufs.data();
        } else {
            vbw.descriptorCount = 1;
            vbw.pBufferInfo = &empty_info;
        }

        VkWriteDescriptorSet ibw = {};
        ibw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ibw.dstSet = pipe->descriptor_sets[i];
        ibw.dstBinding = 4;
        ibw.dstArrayElement = 0;
        ibw.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        if(ibufs.size()) {
            ibw.descriptorCount = ibufs.size();
            ibw.pBufferInfo = ibufs.data();
        } else {
            ibw.descriptorCount = 1;
            ibw.pBufferInfo = &empty_info;
        }

        std::vector<VkDescriptorImageInfo> t_imgs;
        for(auto& view : texture_views) {
            VkDescriptorImageInfo img = {};
            img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            img.imageView = view->view;
            img.sampler = texture_sampler->sampler;
            t_imgs.push_back(img);
        }
        
        VkWriteDescriptorSet tbw = {};
        tbw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        tbw.dstSet = pipe->descriptor_sets[i];
        tbw.dstBinding = 5;
        tbw.dstArrayElement = 0;
        tbw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        tbw.pImageInfo = t_imgs.data();
        tbw.descriptorCount = t_imgs.size();

        std::vector<VkWriteDescriptorSet> writes = {ubw, vbw, ibw};

        if(tbw.descriptorCount > 0)
            writes.push_back(tbw);

        vkUpdateDescriptorSets(vk().device(), writes.size(), writes.data(), 0, nullptr);
    }
}

} // namespace VK
