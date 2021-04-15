
#include "render.h"
#include <util/files.h>

namespace VK {

VkVertexInputBindingDescription Mesh::Vertex::bind_desc() {
    VkVertexInputBindingDescription desc = {};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::array<VkVertexInputAttributeDescription, 3> Mesh::Vertex::attr_descs() {

    std::array<VkVertexInputAttributeDescription, 3> ret;

    ret[0].binding = 0;
    ret[0].location = 0;
    ret[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    ret[0].offset = offsetof(Vertex, pos);

    ret[1].binding = 0;
    ret[1].location = 1;
    ret[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    ret[1].offset = offsetof(Vertex, norm);

    ret[2].binding = 0;
    ret[2].location = 2;
    ret[2].format = VK_FORMAT_R32G32_SFLOAT;
    ret[2].offset = offsetof(Vertex, tex_coord);

    return ret;
}

Mesh::Mesh(std::vector<Mesh::Vertex>&& vertices, std::vector<Mesh::Index>&& indices) {
    vbuf = VK::make<Buffer>();
    ibuf = VK::make<Buffer>();
    recreate(std::move(vertices), std::move(indices));
}

Mesh::Mesh(Mesh&& src) {
    *this = std::move(src);
}

Mesh& Mesh::operator=(Mesh&& src) {
    _verts = std::move(src._verts);
    _idxs = std::move(src._idxs);
    vbuf = std::move(src.vbuf);
    ibuf = std::move(src.ibuf);
    dirty = true;
    src.dirty = true;
    return *this;
}

void Mesh::recreate(std::vector<Mesh::Vertex>&& vertices, std::vector<Mesh::Index>&& indices) {
    _verts = std::move(vertices);
    _idxs = std::move(indices);
    dirty = true;
    vbuf->recreate(_verts.size() * sizeof(Vertex), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    ibuf->recreate(_idxs.size() * sizeof(Index), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT , VMA_MEMORY_USAGE_GPU_ONLY);
}

void Mesh::sync() const {
    if(!dirty) return;

    vbuf->write_staged(_verts.data(), _verts.size() * sizeof(Vertex));
    ibuf->write_staged(_idxs.data(), _idxs.size() * sizeof(Index));

    dirty = false;
}

void Mesh::render(const Pipeline& pipe, VkCommandBuffer cmds) const {
    sync();

    VkBuffer vertex_buffers[] = {vbuf->buf};
    VkDeviceSize offsets[] = {0};

    vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.graphics_pipeline);
    vkCmdBindVertexBuffers(cmds, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(cmds, ibuf->buf, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_layout, 0, 1,
                            &pipe.descriptor_sets[get().current_frame], 0, nullptr);
    vkCmdDrawIndexed(cmds, _idxs.size(), 1, 0, 0, 0);
}

void Pipeline::init() {
    create_descriptor_set_layout();
    create_pipeline();
    create_depth_buf();
    create_uniform_buffers();
    create_descriptor_sets();
}

void Pipeline::destroy_swap() {
    vkDestroyPipeline(get().gpu.device, graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(get().gpu.device, pipeline_layout, nullptr);
    pipeline_layout = {};
    graphics_pipeline = {};
    depth_view.destroy();
    depth_image.destroy();
}

void Pipeline::destroy() {

    vkFreeDescriptorSets(get().gpu.device, get().descriptor_pool, descriptor_sets.size(),
                         descriptor_sets.data());
    descriptor_sets.clear();

    for(auto& buf : uniform_buffers) {
        buf.destroy();
    }
    uniform_buffers.clear();

    vkDestroyDescriptorSetLayout(get().gpu.device, descriptor_layout, nullptr);
}

void Pipeline::create_depth_buf() {

    VkFormat format = get().find_depth_format();
    auto [w, h] = get().swapchain.dim();

    depth_image.recreate(w, h, format, VK_IMAGE_TILING_OPTIMAL,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    depth_view.recreate(depth_image, VK_IMAGE_ASPECT_DEPTH_BIT);
    depth_image.transition(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void Pipeline::update_uniforms(const Camera& cam) {

    static unsigned int start = SDL_GetTicks();

    unsigned int current = SDL_GetTicks();
    float time = (current - start) / 1000.0f;

    Uniforms ubo;
    ubo.M = Mat4::rotate(time * 90.0f, {0.0f, 0.0f, 1.0f}) * Mat4::rotate(90.0f, {1.0f, 0.0f, 0.0f});
    ubo.V = cam.get_view();
    ubo.P = cam.get_proj();

    uniform_buffers[get().current_frame].write(&ubo, sizeof(ubo));
}

void Pipeline::create_pipeline() {

    Shader v_mod(File::read("shaders/1.vert.spv").value());
    Shader f_mod(File::read("shaders/1.frag.spv").value());

    VkPipelineShaderStageCreateInfo stage_info[2] = {};

    stage_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_info[0].module = v_mod.shader;
    stage_info[0].pName = "main";

    stage_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_info[1].module = f_mod.shader;
    stage_info[1].pName = "main";

    auto binding_desc = Mesh::Vertex::bind_desc();
    auto attr_descs = Mesh::Vertex::attr_descs();

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
    viewport.width = (float)get().swapchain.extent.width;
    viewport.height = (float)get().swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = get().swapchain.extent;

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
    msaa_info.pSampleMask = nullptr;            // Optional
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

    VK_CHECK(vkCreatePipelineLayout(get().gpu.device, &layout_info, nullptr, &pipeline_layout));

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
    depth_info.back = {};  // Optional

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
    pipeline_info.renderPass = get().output_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = nullptr; // Optional
    pipeline_info.basePipelineIndex = -1;       // Optional

    VK_CHECK(vkCreateGraphicsPipelines(get().gpu.device, nullptr, 1, &pipeline_info, nullptr,
                                       &graphics_pipeline));
}

void Pipeline::create_uniform_buffers() {

    VkDeviceSize buf_size = sizeof(Uniforms);

    uniform_buffers.clear();
    for(unsigned int i = 0; i < Manager::Frame::MAX_IN_FLIGHT; i++) {
        uniform_buffers.emplace_back(buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
}

void Pipeline::create_descriptor_set_layout() {

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

    VK_CHECK(
        vkCreateDescriptorSetLayout(get().gpu.device, &layout_info, nullptr, &descriptor_layout));
}

void Pipeline::create_descriptor_sets() {

    std::vector<VkDescriptorSetLayout> layouts(Manager::Frame::MAX_IN_FLIGHT, descriptor_layout);

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = get().descriptor_pool;
    alloc_info.descriptorSetCount = Manager::Frame::MAX_IN_FLIGHT;
    alloc_info.pSetLayouts = layouts.data();

    descriptor_sets.resize(Manager::Frame::MAX_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(get().gpu.device, &alloc_info, descriptor_sets.data()));

    for(unsigned int i = 0; i < Manager::Frame::MAX_IN_FLIGHT; i++) {

        VkDescriptorBufferInfo buf_info = {};
        buf_info.buffer = uniform_buffers[i].buf;
        buf_info.offset = 0;
        buf_info.range = sizeof(Uniforms);

        VkWriteDescriptorSet desc_writes[1] = {};

        desc_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_writes[0].dstSet = descriptor_sets[i];
        desc_writes[0].dstBinding = 0;
        desc_writes[0].dstArrayElement = 0;
        desc_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_writes[0].descriptorCount = 1;
        desc_writes[0].pBufferInfo = &buf_info;

        vkUpdateDescriptorSets(get().gpu.device, 1, desc_writes, 0, nullptr);
    }
}

} // namespace VK
