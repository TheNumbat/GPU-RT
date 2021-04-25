
#include "compute.h"

namespace VK {

CPQPipe::~CPQPipe() {
    destroy();
}

void CPQPipe::recreate() {
    pipe.drop();
    create_desc();
    create_pipe();
}

void CPQPipe::destroy() {
    pipe.drop();
}

void CPQPipe::create_pipe() {

    pipe->destroy_swap();

    Shader cpq(File::read("shaders/cpq.comp.spv").value());

    VkPipelineShaderStageCreateInfo stage_info = {};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = cpq.shader;
    stage_info.pName = "main";

    VkPushConstantRange pushes = {};
    pushes.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushes.size = sizeof(unsigned int);
    pushes.offset = 0;

    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &pipe->d_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &pushes;

    VK_CHECK(vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &pipe->p_layout));

    VkComputePipelineCreateInfo p_info = {};
    p_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    p_info.stage = stage_info;
    p_info.layout = pipe->p_layout;

    VK_CHECK(
        vkCreateComputePipelines(vk().device(), nullptr, 1, &p_info, nullptr, &pipe->pipe));
}

VkWriteDescriptorSet CPQPipe::write_buf(const Buffer& buf, int bind) {
    
    VkDescriptorBufferInfo inf = {};
    inf.buffer = buf.buf;
    inf.offset = 0;
    inf.range = VK_WHOLE_SIZE;
    buf_infos[bind] = inf;

    VkWriteDescriptorSet w = {};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = pipe->descriptor_sets[vk().frame()];
    w.dstBinding = bind;
    w.dstArrayElement = 0;
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &buf_infos[bind];
    return w;
}

std::vector<Vec4> CPQPipe::run(const Mesh& mesh, const std::vector<Vec4>& queries) {

    const auto& verts = mesh.verts();
    const auto& inds = mesh.inds();

    std::vector<Vec4> vertices;
    std::vector<unsigned int> indexes;
    for(Mesh::Vertex v : verts) vertices.push_back(Vec4(v.pos, 0.0f));
    for(int i = 0; i < inds.size(); i += 3) {
        indexes.push_back(inds[i]);
        indexes.push_back(inds[i+1]);
        indexes.push_back(inds[i+2]);
        indexes.push_back(0);
    }

    size_t q_size = queries.size() * sizeof(Vec4);
    size_t v_size = vertices.size() * sizeof(Vec4);
    size_t i_size = indexes.size() * sizeof(unsigned int);
    unsigned int n_tris = indexes.size() / 3;

    Buffer g_queries(q_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    g_queries.write_staged(queries.data(), q_size);

    Buffer g_verts(v_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    g_verts.write_staged(vertices.data(), v_size);

    Buffer g_inds(i_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    g_inds.write_staged(indexes.data(), i_size);

    Buffer g_output(q_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
  
    {
        VkWriteDescriptorSet writes[] = {write_buf(g_queries, 0), write_buf(g_output, 1), write_buf(g_verts, 2), write_buf(g_inds, 3)};
        vkUpdateDescriptorSets(vk().device(), 4, writes, 0, nullptr);

        VkCommandBuffer cmds = vk().begin_one_time();

        vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->p_layout, 0, 1, &pipe->descriptor_sets[vk().frame()], 0, nullptr);
        vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->pipe);
        vkCmdPushConstants(cmds, pipe->p_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(n_tris), &n_tris);
        vkCmdDispatch(cmds, queries.size(), 1, 1);

        vk().end_one_time(cmds);
    }

    std::vector<Vec4> output(queries.size());

    Buffer c_output(q_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    g_output.copy_to(c_output);
    c_output.read(output.data(), q_size);

    return output;
}

void CPQPipe::create_desc() {

    VkDescriptorSetLayoutBinding i_bind = {};
    i_bind.binding = 0;
    i_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    i_bind.descriptorCount = 1;
    i_bind.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding o_bind = {};
    o_bind.binding = 1;
    o_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    o_bind.descriptorCount = 1;
    o_bind.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding v_bind = {};
    v_bind.binding = 2;
    v_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    v_bind.descriptorCount = 1;
    v_bind.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding id_bind = {};
    id_bind.binding = 3;
    id_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    id_bind.descriptorCount = 1;
    id_bind.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {i_bind, o_bind, v_bind, id_bind};

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 4;
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
}

}
