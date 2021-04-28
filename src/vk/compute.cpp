
#include "compute.h"
#include <scene/bvh.h>

namespace VK {

BVHPipe::~BVHPipe() {
    destroy();
}

void BVHPipe::recreate() {
    threaded_pipe.drop();
    brute_pipe.drop();
    stack_pipe.drop();
    create_desc();
    create_pipe();
}

void BVHPipe::destroy() {
    threaded_pipe.drop();
    brute_pipe.drop();
    stack_pipe.drop();
}

void BVHPipe::create_pipe() {

    {
        threaded_pipe->destroy_swap();

        Shader threaded(File::read("shaders/bvh_threaded.comp.spv").value());
        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = threaded.shader;
        stage_info.pName = "main";

        VkPushConstantRange pushes = {};
        pushes.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushes.size = sizeof(Constants);
        pushes.offset = 0;

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &threaded_pipe->d_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushes;

        VK_CHECK(
            vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &threaded_pipe->p_layout));

        VkComputePipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = stage_info;
        info.layout = threaded_pipe->p_layout;

        VK_CHECK(vkCreateComputePipelines(vk().device(), nullptr, 1, &info, nullptr,
                                          &threaded_pipe->pipe));
    }

    {
        stack_pipe->destroy_swap();

        Shader stack(File::read("shaders/bvh_stack.comp.spv").value());
        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = stack.shader;
        stage_info.pName = "main";

        VkPushConstantRange pushes = {};
        pushes.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushes.size = sizeof(Constants);
        pushes.offset = 0;

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &stack_pipe->d_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushes;

        VK_CHECK(
            vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &stack_pipe->p_layout));

        VkComputePipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = stage_info;
        info.layout = stack_pipe->p_layout;

        VK_CHECK(vkCreateComputePipelines(vk().device(), nullptr, 1, &info, nullptr,
                                          &stack_pipe->pipe));
    }

    {
        brute_pipe->destroy_swap();

        Shader brute(File::read("shaders/bvh_brute.comp.spv").value());
        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = brute.shader;
        stage_info.pName = "main";

        VkPushConstantRange pushes = {};
        pushes.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushes.size = sizeof(Constants);
        pushes.offset = 0;

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &brute_pipe->d_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushes;

        VK_CHECK(
            vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &brute_pipe->p_layout));

        VkComputePipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = stage_info;
        info.layout = brute_pipe->p_layout;

        VK_CHECK(
            vkCreateComputePipelines(vk().device(), nullptr, 1, &info, nullptr, &brute_pipe->pipe));
    }
}

VkWriteDescriptorSet BVHPipe::write_buf(const Buffer& buf, const PipeData& pipe, int bind) {

    VkDescriptorBufferInfo inf = {};
    inf.buffer = buf.buf;
    inf.offset = 0;
    inf.range = VK_WHOLE_SIZE;
    buf_infos[bind] = inf;

    VkWriteDescriptorSet w = {};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = pipe.descriptor_sets[vk().frame()];
    w.dstBinding = bind;
    w.dstArrayElement = 0;
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &buf_infos[bind];
    return w;
}

std::vector<Vec4> BVHPipe::cpqs(BVH_Type type, const BVH& bvh, const std::vector<Vec4>& queries) {
    switch(type) {
    case BVH_Type::none: return run_brute(bvh, queries, false);
    case BVH_Type::threaded: return run_threaded(bvh, queries, false);
    case BVH_Type::stack: return run_stack(bvh, queries, false, false);
    case BVH_Type::stackless: return run_stack(bvh, queries, false, true);
    default: assert(false);
    }
}

std::vector<Vec4> BVHPipe::rays(BVH_Type type, const BVH& bvh,
                                const std::vector<std::pair<Vec4, Vec4>>& queries) {
    std::vector<Vec4> q(queries.size() * 2);
    for(int i = 0; i < queries.size(); i++) {
        q[2 * i] = queries[i].first;
        q[2 * i + 1] = queries[i].second;
    }
    switch(type) {
    case BVH_Type::none: return run_brute(bvh, q, true);
    case BVH_Type::threaded: return run_threaded(bvh, q, true);
    case BVH_Type::stack: return run_stack(bvh, q, true, false);
    case BVH_Type::stackless: return run_stack(bvh, q, true, true);
    default: assert(false);
    }
}

std::vector<Vec4> BVHPipe::run_brute(const BVH& bvh, const std::vector<Vec4>& queries, bool rays) {

    const auto& bvh_tris = bvh.get_triangles();
    std::vector<GPU_Tri> triangles;
    triangles.reserve(bvh_tris.size());

    for(const auto& t : bvh_tris) {
        triangles.push_back({Vec4{t.v0, 0.0f}, Vec4{t.v1, 0.0f}, Vec4{t.v2, 0.0f}});
    }

    size_t q_size = queries.size() * sizeof(Vec4);
    size_t t_size = triangles.size() * sizeof(GPU_Tri);
    size_t o_size = rays ? q_size / 2 : q_size;

    Buffer g_queries(q_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY);
    g_queries.write_staged(queries.data(), q_size);

    Buffer g_tris(t_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VMA_MEMORY_USAGE_GPU_ONLY);
    g_tris.write_staged(triangles.data(), t_size);

    Buffer g_output(o_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);

    {
        VkWriteDescriptorSet writes[] = {write_buf(g_queries, brute_pipe, 0),
                                         write_buf(g_output, brute_pipe, 1),
                                         write_buf(g_tris, brute_pipe, 2)};
        vkUpdateDescriptorSets(vk().device(), 3, writes, 0, nullptr);

        VkCommandBuffer cmds = vk().begin_one_time();

        Constants consts;
        consts.n_nodes = 0;
        consts.n_tris = triangles.size();
        consts.trace_rays = rays;

        vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, brute_pipe->p_layout, 0, 1,
                                &brute_pipe->descriptor_sets[vk().frame()], 0, nullptr);
        vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, brute_pipe->pipe);
        vkCmdPushConstants(cmds, brute_pipe->p_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(consts), &consts);
        vkCmdDispatch(cmds, queries.size(), 1, 1);

        vk().end_one_time(cmds);
    }

    std::vector<Vec4> output(rays ? queries.size() / 2 : queries.size());

    Buffer c_output(o_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    g_output.copy_to(c_output);
    c_output.read(output.data(), o_size);

    return output;
}

std::vector<Vec4> BVHPipe::run_threaded(const BVH& bvh, const std::vector<Vec4>& queries,
                                        bool rays) {

    const auto& bvh_nodes = bvh.get_nodes();
    const auto& bvh_tris = bvh.get_triangles();

    std::vector<GPU_Tri> triangles;
    std::vector<Threaded_Node> nodes;
    triangles.reserve(bvh_tris.size());
    nodes.reserve(bvh_nodes.size());

    for(const auto& n : bvh_nodes) {
        nodes.push_back({Vec4{n.bbox.min, 0.0f}, Vec4{n.bbox.max, 0.0f}, n.is_leaf() ? n.start : 0,
                         n.is_leaf() ? n.size : 0, n.hit, n.miss});
    }
    for(const auto& t : bvh_tris) {
        triangles.push_back({Vec4{t.v0, 0.0f}, Vec4{t.v1, 0.0f}, Vec4{t.v2, 0.0f}});
    }

    size_t q_size = queries.size() * sizeof(Vec4);
    size_t t_size = triangles.size() * sizeof(GPU_Tri);
    size_t n_size = nodes.size() * sizeof(Threaded_Node);
    size_t o_size = rays ? q_size / 2 : q_size;

    Buffer g_queries(q_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY);
    g_queries.write_staged(queries.data(), q_size);

    Buffer g_tris(t_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VMA_MEMORY_USAGE_GPU_ONLY);
    g_tris.write_staged(triangles.data(), t_size);

    Buffer g_nodes(n_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                   VMA_MEMORY_USAGE_GPU_ONLY);
    g_nodes.write_staged(nodes.data(), n_size);

    Buffer g_output(o_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);

    {
        VkWriteDescriptorSet writes[] = {
            write_buf(g_queries, threaded_pipe, 0), write_buf(g_output, threaded_pipe, 1),
            write_buf(g_tris, threaded_pipe, 2), write_buf(g_nodes, threaded_pipe, 3)};
        vkUpdateDescriptorSets(vk().device(), 4, writes, 0, nullptr);

        VkCommandBuffer cmds = vk().begin_one_time();

        Constants consts;
        consts.n_nodes = nodes.size();
        consts.n_tris = triangles.size();
        consts.trace_rays = rays;

        vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, threaded_pipe->p_layout, 0, 1,
                                &threaded_pipe->descriptor_sets[vk().frame()], 0, nullptr);
        vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, threaded_pipe->pipe);
        vkCmdPushConstants(cmds, threaded_pipe->p_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(consts), &consts);
        vkCmdDispatch(cmds, queries.size(), 1, 1);

        vk().end_one_time(cmds);
    }

    std::vector<Vec4> output(rays ? queries.size() / 2 : queries.size());

    Buffer c_output(o_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    g_output.copy_to(c_output);
    c_output.read(output.data(), o_size);

    return output;
}

std::vector<Vec4> BVHPipe::run_stack(const BVH& bvh, const std::vector<Vec4>& queries,
                                        bool rays, bool stackless) {

    const auto& bvh_nodes = bvh.get_nodes();
    const auto& bvh_tris = bvh.get_triangles();

    std::vector<GPU_Tri> triangles;
    std::vector<Stack_Node> nodes;
    triangles.reserve(bvh_tris.size());
    nodes.reserve(bvh_nodes.size());

    for(const auto& n : bvh_nodes) {
        Vec4 bmin{n.bbox.min, 0.0f};
        Vec4 bmax{n.bbox.max, 0.0f};
        if(stackless) {
            if(n.is_leaf()) {
                nodes.push_back({bmin, bmax, n.parent, n.parent, n.start, n.size});
            } else {
                nodes.push_back({bmin, bmax, n.l, n.r, n.parent, 0});
            }
        } else {
            nodes.push_back({bmin, bmax, n.l, n.r, n.start, n.size});
        }
    }
    for(const auto& t : bvh_tris) {
        triangles.push_back({Vec4{t.v0, 0.0f}, Vec4{t.v1, 0.0f}, Vec4{t.v2, 0.0f}});
    }

    size_t q_size = queries.size() * sizeof(Vec4);
    size_t t_size = triangles.size() * sizeof(GPU_Tri);
    size_t n_size = nodes.size() * sizeof(Stack_Node);
    size_t o_size = rays ? q_size / 2 : q_size;

    Buffer g_queries(q_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY);
    g_queries.write_staged(queries.data(), q_size);

    Buffer g_tris(t_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VMA_MEMORY_USAGE_GPU_ONLY);
    g_tris.write_staged(triangles.data(), t_size);

    Buffer g_nodes(n_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                   VMA_MEMORY_USAGE_GPU_ONLY);
    g_nodes.write_staged(nodes.data(), n_size);

    Buffer g_output(o_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);

    {
        VkWriteDescriptorSet writes[] = {
            write_buf(g_queries, stack_pipe, 0), write_buf(g_output, stack_pipe, 1),
            write_buf(g_tris, stack_pipe, 2), write_buf(g_nodes, stack_pipe, 3)};
        vkUpdateDescriptorSets(vk().device(), 4, writes, 0, nullptr);

        VkCommandBuffer cmds = vk().begin_one_time();

        Constants consts;
        consts.n_nodes = nodes.size();
        consts.n_tris = triangles.size();
        consts.trace_rays = rays;
        consts.stackless = stackless;

        vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, stack_pipe->p_layout, 0, 1,
                                &stack_pipe->descriptor_sets[vk().frame()], 0, nullptr);
        vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, stack_pipe->pipe);
        vkCmdPushConstants(cmds, stack_pipe->p_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(consts), &consts);
        vkCmdDispatch(cmds, queries.size(), 1, 1);

        vk().end_one_time(cmds);
    }

    std::vector<Vec4> output(rays ? queries.size() / 2 : queries.size());

    Buffer c_output(o_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    g_output.copy_to(c_output);
    c_output.read(output.data(), o_size);

    return output;
}

void BVHPipe::create_desc() {

    { // THREADED
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

        VK_CHECK(vkCreateDescriptorSetLayout(vk().device(), &layout_info, nullptr,
                                             &threaded_pipe->d_layout));

        std::vector<VkDescriptorSetLayout> layouts(Manager::MAX_IN_FLIGHT, threaded_pipe->d_layout);

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = vk().pool();
        alloc_info.descriptorSetCount = Manager::MAX_IN_FLIGHT;
        alloc_info.pSetLayouts = layouts.data();

        threaded_pipe->descriptor_sets.resize(Manager::MAX_IN_FLIGHT);
        VK_CHECK(vkAllocateDescriptorSets(vk().device(), &alloc_info,
                                          threaded_pipe->descriptor_sets.data()));
    }

    { // STACK
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

        VK_CHECK(vkCreateDescriptorSetLayout(vk().device(), &layout_info, nullptr,
                                             &stack_pipe->d_layout));

        std::vector<VkDescriptorSetLayout> layouts(Manager::MAX_IN_FLIGHT, stack_pipe->d_layout);

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = vk().pool();
        alloc_info.descriptorSetCount = Manager::MAX_IN_FLIGHT;
        alloc_info.pSetLayouts = layouts.data();

        stack_pipe->descriptor_sets.resize(Manager::MAX_IN_FLIGHT);
        VK_CHECK(vkAllocateDescriptorSets(vk().device(), &alloc_info,
                                          stack_pipe->descriptor_sets.data()));
    }

    { // BRUTE
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

        VkDescriptorSetLayoutBinding bindings[] = {i_bind, o_bind, v_bind};

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 3;
        layout_info.pBindings = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(vk().device(), &layout_info, nullptr,
                                             &brute_pipe->d_layout));

        std::vector<VkDescriptorSetLayout> layouts(Manager::MAX_IN_FLIGHT, brute_pipe->d_layout);

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = vk().pool();
        alloc_info.descriptorSetCount = Manager::MAX_IN_FLIGHT;
        alloc_info.pSetLayouts = layouts.data();

        brute_pipe->descriptor_sets.resize(Manager::MAX_IN_FLIGHT);
        VK_CHECK(vkAllocateDescriptorSets(vk().device(), &alloc_info,
                                          brute_pipe->descriptor_sets.data()));
    }
}

} // namespace VK
