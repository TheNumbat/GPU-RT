
#include "mesh.h"
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
    ret[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ret[0].offset = offsetof(Vertex, pos);

    ret[1].binding = 0;
    ret[1].location = 1;
    ret[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ret[1].offset = offsetof(Vertex, norm);

    ret[2].binding = 0;
    ret[2].location = 2;
    ret[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ret[2].offset = offsetof(Vertex, tang);

    return ret;
}

Mesh::Mesh(std::vector<Mesh::Vertex>&& vertices, std::vector<Mesh::Index>&& indices) {
    recreate(std::move(vertices), std::move(indices));
}

Mesh::Mesh(Mesh&& src) {
    *this = std::move(src);
}

Mesh& Mesh::operator=(Mesh&& src) {
    _verts = std::move(src._verts);
    _idxs = std::move(src._idxs);
    _bbox = std::move(src._bbox);
    vbuf = std::move(src.vbuf);
    ibuf = std::move(src.ibuf);
    dirty = src.dirty;
    src.dirty = true;
    return *this;
}

void Mesh::recreate(std::vector<Mesh::Vertex>&& vertices, std::vector<Mesh::Index>&& indices) {
    _verts = std::move(vertices);
    _idxs = std::move(indices);
    dirty = true;
    vbuf->recreate(_verts.size() * sizeof(Vertex),
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                   VMA_MEMORY_USAGE_GPU_ONLY);
    ibuf->recreate(_idxs.size() * sizeof(Index),
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                   VMA_MEMORY_USAGE_GPU_ONLY);

    BBox box;
    for(auto& v : _verts) box.enclose(v.pos.xyz());
    _bbox = box;
}

void Mesh::sync() const {
    if(!dirty) return;

    vbuf->write_staged(_verts.data(), _verts.size() * sizeof(Vertex));
    ibuf->write_staged(_idxs.data(), _idxs.size() * sizeof(Index));

    dirty = false;
}

void Mesh::render(VkCommandBuffer& cmds, const PipeData& pipe, const Mat4& T) const {
    sync();

    VkBuffer vertex_buffers[] = {vbuf->buf};
    VkDeviceSize offsets[] = {0};

    vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipe);
    vkCmdBindVertexBuffers(cmds, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(cmds, ibuf->buf, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmds, pipe.p_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), T.data);
    vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.p_layout, 0, 1,
                            &pipe.descriptor_sets[vk().frame()], 0, nullptr);
    vkCmdDrawIndexed(cmds, _idxs.size(), 1, 0, 0, 0);
}

MeshPipe::~MeshPipe() {
    destroy();
}

MeshPipe::MeshPipe(const Pass& pass, VkExtent2D ext) {
    recreate(pass, ext);
}

void MeshPipe::recreate(const Pass& pass, VkExtent2D ext) {
    pipe.drop();
    create_desc();
    create_pipe(pass, ext);
}

void MeshPipe::destroy() {
    pipe.drop();
    camera_uniforms.clear();
}

void MeshPipe::recreate_swap(const Pass& pass, VkExtent2D ext) {
    pipe->destroy_swap();
    create_pipe(pass, ext);
}

void MeshPipe::update_uniforms(const Camera& cam) {

    Cam_Uniforms ubo;
    ubo.V = cam.get_view();
    ubo.P = cam.get_proj();
    ubo.iV = ubo.V.inverse();
    ubo.iP = ubo.P.inverse();

    camera_uniforms[vk().frame()]->write(&ubo, sizeof(ubo));
}

void MeshPipe::create_pipe(const Pass& pass, VkExtent2D ext) {

    pipe->destroy_swap();

    Shader v_mod(File::read("shaders/mesh.vert.spv").value());
    Shader f_mod(File::read("shaders/mesh.frag.spv").value());

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
    viewport.width = (float)ext.width;
    viewport.height = (float)ext.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = ext;

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
    raster_info.cullMode = VK_CULL_MODE_NONE;
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

    VkPushConstantRange pushes = {};
    pushes.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushes.size = sizeof(Mat4);
    pushes.offset = 0;

    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &pipe->d_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &pushes;

    VK_CHECK(vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &pipe->p_layout));

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
    pipeline_info.layout = pipe->p_layout;
    pipeline_info.renderPass = pass.pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = nullptr; // Optional
    pipeline_info.basePipelineIndex = -1;       // Optional

    VK_CHECK(
        vkCreateGraphicsPipelines(vk().device(), nullptr, 1, &pipeline_info, nullptr, &pipe->pipe));
}

void MeshPipe::create_desc() {

    VkDescriptorSetLayoutBinding ubo_bind = {};
    ubo_bind.binding = 0;
    ubo_bind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_bind.descriptorCount = 1;
    ubo_bind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    ubo_bind.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding bindings[] = {ubo_bind};

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(vk().device(), &layout_info, nullptr, &pipe->d_layout));

    std::vector<VkDescriptorSetLayout> layouts(Manager::MAX_IN_FLIGHT, pipe->d_layout);

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk().pool();
    alloc_info.descriptorSetCount = Manager::MAX_IN_FLIGHT;
    alloc_info.pSetLayouts = layouts.data();

    pipe->descriptor_sets.resize(Manager::MAX_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(vk().device(), &alloc_info, pipe->descriptor_sets.data()));

    VkDeviceSize buf_size = sizeof(Cam_Uniforms);
    camera_uniforms.resize(Manager::MAX_IN_FLIGHT);

    for(unsigned int i = 0; i < Manager::MAX_IN_FLIGHT; i++) {

        camera_uniforms[i]->recreate(buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VMA_MEMORY_USAGE_CPU_TO_GPU);

        VkDescriptorBufferInfo buf_info = {};
        buf_info.buffer = camera_uniforms[i]->buf;
        buf_info.offset = 0;
        buf_info.range = sizeof(Cam_Uniforms);

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = pipe->descriptor_sets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &buf_info;

        vkUpdateDescriptorSets(vk().device(), 1, &write, 0, nullptr);
    }
}

}
