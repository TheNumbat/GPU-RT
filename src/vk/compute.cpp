
#include "compute.h"
#include <algorithm>
#include <scene/bvh.h>

namespace VK {

BVHPipe::~BVHPipe() {
    destroy();
}

void BVHPipe::recreate() {
    
    VkQueryPoolCreateInfo query = {};
    query.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query.queryType = VK_QUERY_TYPE_TIMESTAMP;
    query.queryCount = 2;
    vkCreateQueryPool(vk().device(), &query, nullptr, &querypool);
    
    threaded_pipe.drop();
    brute_pipe.drop();
    stack_pipe.drop();
    rt_pipe.drop();
    obb_pipe.drop();
    for(auto& w : wide_pipe)
        w.drop();
    sbt.drop();
    create_desc();
    create_pipe();
}

void BVHPipe::destroy() {

    vkDestroyQueryPool(vk().device(), querypool, nullptr);
    threaded_pipe.drop();
    brute_pipe.drop();
    stack_pipe.drop();
    rt_pipe.drop();
    obb_pipe.drop();
    for(auto& w : wide_pipe)
        w.drop();
    sbt.drop();
}

BBox BVHPipe::box() {
    return bvh.box();
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

    for(int w = 0; w < WIDE_PIPES; w++) {

        int width = 1 << (w + 1);
        wide_pipe[w]->destroy_swap();

        VkSpecializationMapEntry spec_map[1];
        spec_map[0].constantID = 0;
        spec_map[0].offset = 0;
        spec_map[0].size = sizeof(width);

        VkSpecializationInfo spec = {};
        spec.dataSize = sizeof(width);
        spec.pData = &width;
        spec.mapEntryCount = 1;
        spec.pMapEntries = spec_map;

        Shader wide(File::read("shaders/bvh_wide" + std::to_string(w+1) + ".comp.spv").value());
        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = wide.shader;
        stage_info.pName = "main";
        stage_info.pSpecializationInfo = &spec;

        VkPushConstantRange pushes = {};
        pushes.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushes.size = sizeof(Constants);
        pushes.offset = 0;

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &wide_pipe[w]->d_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushes;

        VK_CHECK(
            vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &wide_pipe[w]->p_layout));

        VkComputePipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = stage_info;
        info.layout = wide_pipe[w]->p_layout;

        VK_CHECK(vkCreateComputePipelines(vk().device(), nullptr, 1, &info, nullptr,
                                          &wide_pipe[w]->pipe));
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

        VK_CHECK(
            vkCreateComputePipelines(vk().device(), nullptr, 1, &info, nullptr, &stack_pipe->pipe));
    }

    {
        obb_pipe->destroy_swap();

        Shader obb(File::read("shaders/bvh_obb.comp.spv").value());
        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = obb.shader;
        stage_info.pName = "main";

        VkPushConstantRange pushes = {};
        pushes.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushes.size = sizeof(Constants);
        pushes.offset = 0;

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &obb_pipe->d_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushes;

        VK_CHECK(
            vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &obb_pipe->p_layout));

        VkComputePipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = stage_info;
        info.layout = obb_pipe->p_layout;

        VK_CHECK(
            vkCreateComputePipelines(vk().device(), nullptr, 1, &info, nullptr, &obb_pipe->pipe));
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

    { // RTX
        rt_pipe->destroy_swap();

        Shader chit(File::read("shaders/rquery.rchit.spv").value());
        Shader miss(File::read("shaders/rquery.rmiss.spv").value());
        Shader gen(File::read("shaders/rquery.rgen.spv").value());

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
        pushes.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        pushes.size = sizeof(Constants);
        pushes.offset = 0;

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &rt_pipe->d_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &pushes;

        VK_CHECK(vkCreatePipelineLayout(vk().device(), &layout_info, nullptr, &rt_pipe->p_layout));

        VkRayTracingPipelineCreateInfoKHR rayPipelineInfo = {};
        rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        rayPipelineInfo.stageCount = 3;
        rayPipelineInfo.pStages = stage_info;
        rayPipelineInfo.groupCount = 3;
        rayPipelineInfo.pGroups = groups;
        rayPipelineInfo.maxPipelineRayRecursionDepth = 1;
        rayPipelineInfo.layout = rt_pipe->p_layout;

        VK_CHECK(vk().rtx.vkCreateRayTracingPipelinesKHR(vk().device(), nullptr, nullptr, 1,
                                                        &rayPipelineInfo, nullptr, &rt_pipe->pipe));
                                                        
        unsigned int shader_count = 3;
        unsigned int groupHandleSize = vk().rtx.properties.shaderGroupHandleSize;
        unsigned int groupSizeAligned =
            align_up(groupHandleSize, vk().rtx.properties.shaderGroupBaseAlignment);
        unsigned int sbtSize = shader_count * groupSizeAligned;

        std::vector<unsigned char> shaderHandleStorage(sbtSize);

        VK_CHECK(vk().rtx.vkGetRayTracingShaderGroupHandlesKHR(
            vk().device(), rt_pipe->pipe, 0, shader_count, sbtSize, shaderHandleStorage.data()));

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

void BVHPipe::build(const Mesh& mesh, int leaf_size) {
    bvh.build(mesh, leaf_size);
    obbbvh.build(mesh, leaf_size);
    wbvh1 = bvh.make_wide<1>();
    wbvh2 = bvh.make_wide<2>();
    wbvh3 = bvh.make_wide<3>();
    wbvh4 = bvh.make_wide<4>();
    BLAS.clear();
    BLAS.push_back(Accel(mesh));
    TLAS->recreate(BLAS, {Mat4::I});
}

std::vector<Vec4> BVHPipe::cpqs(BVH_Type type, const std::vector<Vec4>& queries, int& time, int w) {
    switch(type) {
    case BVH_Type::none: return run_brute(queries, false, time);
    case BVH_Type::threaded: return run_threaded(queries, false, time);
    case BVH_Type::stack: return run_stack(queries, false, false, time);
    case BVH_Type::stackless: return run_stack(queries, false, true, time);
    case BVH_Type::obb: return run_obb(queries, false, false, time);
    case BVH_Type::obb_stackless: return run_obb(queries, false, true, time);
    case BVH_Type::wide: return run_wide(queries, false, 0, time, w);
    case BVH_Type::wide_max: return run_wide(queries, false, 1, time, w);
    case BVH_Type::wide_sort: return run_wide(queries, false, 2, time, w);
    case BVH_Type::RTX: return {};
    default: assert(false);
    }
}

std::vector<Vec4> BVHPipe::rays(BVH_Type type, const std::vector<std::pair<Vec4, Vec4>>& queries, int& time, int w) {
    std::vector<Vec4> q(queries.size() * 2);
    for(int i = 0; i < queries.size(); i++) {
        q[2 * i] = queries[i].first;
        q[2 * i + 1] = queries[i].second;
    }
    switch(type) {
    case BVH_Type::none: return run_brute(q, true, time);
    case BVH_Type::threaded: return run_threaded(q, true, time);
    case BVH_Type::stack: return run_stack(q, true, false, time);
    case BVH_Type::stackless: return run_stack(q, true, true, time);
    case BVH_Type::obb: return run_obb(q, true, false, time);
    case BVH_Type::obb_stackless: return run_obb(q, true, true, time);
    case BVH_Type::wide: return run_wide(q, true, 0, time, w);
    case BVH_Type::wide_max: return run_wide(q, true, 1, time, w);
    case BVH_Type::wide_sort: return run_wide(q, true, 2, time, w);
    case BVH_Type::RTX: return run_rtx(q, time);
    default: assert(false);
    }
}

std::vector<Vec4> BVHPipe::run_brute(const std::vector<Vec4>& queries, bool rays, int& time) {

    const auto& bvh_tris = bvh.get_triangles();
    std::vector<GPU_Tri> triangles;
    triangles.reserve(bvh_tris.size());

    for(const auto& t : bvh_tris) {
        triangles.push_back({Vec4{t.v0, 0.0f}, Vec4{t.v1, 0.0f}, Vec4{t.v2, 0.0f}});
    }

    size_t n_queries = std::min(queries.size(), (rays ? 2 * BRUTE_MAX : BRUTE_MAX));
    size_t q_size = n_queries * sizeof(Vec4);
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

        vkResetQueryPool(vk().device(), querypool, 0, 2);

        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 0);
        vkCmdDispatch(cmds, n_queries, 1, 1);
        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 1);

        vk().end_one_time(cmds);

        size_t data[2];
        vkGetQueryPoolResults(vk().device(), querypool, 0, 2, 2 * sizeof(size_t), data, sizeof(size_t), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
        time = (data[1] - data[0]) / 1000000;
    }

    std::vector<Vec4> output(rays ? n_queries / 2 : n_queries);

    Buffer c_output(o_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    g_output.copy_to(c_output);
    c_output.read(output.data(), o_size);

    return output;
}

std::vector<Vec4> BVHPipe::run_threaded(const std::vector<Vec4>& queries,
                                        bool rays, int& time) {

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
        vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, threaded_pipe->p_layout, 0, 1,
                                &threaded_pipe->descriptor_sets[vk().frame()], 0, nullptr);
        vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, threaded_pipe->pipe);

        Constants consts;
        consts.n_nodes = nodes.size();
        consts.n_tris = triangles.size();
        consts.trace_rays = rays;
        consts.start = 0;

        vkCmdPushConstants(cmds, threaded_pipe->p_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                            sizeof(consts), &consts);

        
        vkResetQueryPool(vk().device(), querypool, 0, 2);

        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 0);
        vkCmdDispatch(cmds, rays ? queries.size() / 2 : queries.size(), 1, 1);
        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 1);

        vk().end_one_time(cmds);

        size_t data[2];
        vkGetQueryPoolResults(vk().device(), querypool, 0, 2, 2 * sizeof(size_t), data, sizeof(size_t), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
        time = (data[1] - data[0]) / 1000000;
    }

    std::vector<Vec4> output(rays ? queries.size() / 2 : queries.size());

    Buffer c_output(o_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    g_output.copy_to(c_output);
    c_output.read(output.data(), o_size);

    return output;
}

std::vector<Vec4> BVHPipe::run_stack(const std::vector<Vec4>& queries, bool rays,
                                     bool stackless, int& time) {

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

        vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, stack_pipe->p_layout, 0, 1,
                                &stack_pipe->descriptor_sets[vk().frame()], 0, nullptr);
        vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, stack_pipe->pipe);
        
        Constants consts;
        consts.n_nodes = nodes.size();
        consts.n_tris = triangles.size();
        consts.trace_rays = rays;
        consts.stackless = stackless;
        consts.start = 0;

        vkCmdPushConstants(cmds, stack_pipe->p_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                            sizeof(consts), &consts);

        vkResetQueryPool(vk().device(), querypool, 0, 2);

        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 0);
        vkCmdDispatch(cmds, rays ? queries.size() / 2 : queries.size(), 1, 1);
        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 1);

        vk().end_one_time(cmds);

        size_t data[2];
        vkGetQueryPoolResults(vk().device(), querypool, 0, 2, 2 * sizeof(size_t), data, sizeof(size_t), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
        time = (data[1] - data[0]) / 1000000;
    }

    std::vector<Vec4> output(rays ? queries.size() / 2 : queries.size());

    Buffer c_output(o_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    g_output.copy_to(c_output);
    c_output.read(output.data(), o_size);

    return output;
}

std::vector<Vec4> BVHPipe::run_obb(const std::vector<Vec4>& queries, bool rays,
                                     bool stackless, int& time) {

    const auto& bvh_nodes = obbbvh.get_nodes();
    const auto& bvh_tris = obbbvh.get_triangles();

    std::vector<GPU_Tri> triangles;
    std::vector<OBB_Node> nodes;
    triangles.reserve(bvh_tris.size());
    nodes.reserve(bvh_nodes.size());

    for(const auto& n : bvh_nodes) {
        Vec4 ext{n.bbox.ext, 0.0f};
        if(stackless) {
            if(n.is_leaf()) {
                nodes.push_back({n.bbox.T, ext, n.parent, n.parent, n.start, n.size});
            } else {
                nodes.push_back({n.bbox.T, ext, n.l, n.r, n.parent, 0});
            }
        } else {
            nodes.push_back({n.bbox.T, ext, n.l, n.r, n.start, n.size});
        }
    }
    for(const auto& t : bvh_tris) {
        triangles.push_back({Vec4{t.v0, 0.0f}, Vec4{t.v1, 0.0f}, Vec4{t.v2, 0.0f}});
    }

    size_t q_size = queries.size() * sizeof(Vec4);
    size_t t_size = triangles.size() * sizeof(GPU_Tri);
    size_t n_size = nodes.size() * sizeof(OBB_Node);
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
            write_buf(g_queries, obb_pipe, 0), write_buf(g_output, obb_pipe, 1),
            write_buf(g_tris, obb_pipe, 2), write_buf(g_nodes, obb_pipe, 3)};
        vkUpdateDescriptorSets(vk().device(), 4, writes, 0, nullptr);

        VkCommandBuffer cmds = vk().begin_one_time();

        vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, obb_pipe->p_layout, 0, 1,
                                &obb_pipe->descriptor_sets[vk().frame()], 0, nullptr);
        vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, obb_pipe->pipe);
        
        Constants consts;
        consts.n_nodes = nodes.size();
        consts.n_tris = triangles.size();
        consts.trace_rays = rays;
        consts.stackless = stackless;
        consts.start = 0;

        vkCmdPushConstants(cmds, obb_pipe->p_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                            sizeof(consts), &consts);

        vkResetQueryPool(vk().device(), querypool, 0, 2);

        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 0);
        vkCmdDispatch(cmds, rays ? queries.size() / 2 : queries.size(), 1, 1);
        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 1);

        vk().end_one_time(cmds);

        size_t data[2];
        vkGetQueryPoolResults(vk().device(), querypool, 0, 2, 2 * sizeof(size_t), data, sizeof(size_t), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
        time = (data[1] - data[0]) / 1000000;
    }

    std::vector<Vec4> output(rays ? queries.size() / 2 : queries.size());

    Buffer c_output(o_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    g_output.copy_to(c_output);
    c_output.read(output.data(), o_size);

    return output;
}

std::pair<void*,size_t> BVHPipe::pick_wide_bvh(int w) {
    switch(w) {
    case 0: return {wbvh1.nodes.data(), wbvh1.nodes.size() * sizeof(wbvh1.nodes[0])};
    case 1: return {wbvh2.nodes.data(), wbvh2.nodes.size() * sizeof(wbvh2.nodes[0])};
    case 2: return {wbvh3.nodes.data(), wbvh3.nodes.size() * sizeof(wbvh3.nodes[0])};
    case 3: return {wbvh4.nodes.data(), wbvh4.nodes.size() * sizeof(wbvh4.nodes[0])};
    }
    assert(false);
    return {};
}

std::vector<Vec4> BVHPipe::run_wide(const std::vector<Vec4>& queries, bool rays,
                                     int sort, int& time, int _w) {

    assert(_w >= 1 && _w <= 4);
    int w = _w - 1;

    const auto& bvh_tris = bvh.get_triangles();
    auto [nodes, n_size] = pick_wide_bvh(w);

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

    Buffer g_nodes(n_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                   VMA_MEMORY_USAGE_GPU_ONLY);
    g_nodes.write_staged(nodes, n_size);

    Buffer g_output(o_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);

    {
        VkWriteDescriptorSet writes[] = {
            write_buf(g_queries, wide_pipe[w], 0), write_buf(g_output, wide_pipe[w], 1),
            write_buf(g_tris, wide_pipe[w], 2), write_buf(g_nodes, wide_pipe[w], 3)};
        vkUpdateDescriptorSets(vk().device(), 4, writes, 0, nullptr);

        VkCommandBuffer cmds = vk().begin_one_time();

        vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, wide_pipe[w]->p_layout, 0, 1,
                                &wide_pipe[w]->descriptor_sets[vk().frame()], 0, nullptr);
        vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, wide_pipe[w]->pipe);
        
        Constants consts;
        consts.n_tris = triangles.size();
        consts.trace_rays = rays;
        consts.start = 0;
        consts.sort_children = sort;
        
        vkCmdPushConstants(cmds, wide_pipe[w]->p_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                            sizeof(consts), &consts);

        vkResetQueryPool(vk().device(), querypool, 0, 2);

        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 0);
        vkCmdDispatch(cmds, rays ? queries.size() / 2 : queries.size(), 1, 1);
        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 1);

        vk().end_one_time(cmds);

        size_t data[2];
        vkGetQueryPoolResults(vk().device(), querypool, 0, 2, 2 * sizeof(size_t), data, sizeof(size_t), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
        time = (data[1] - data[0]) / 1000000;
    }

    std::vector<Vec4> output(rays ? queries.size() / 2 : queries.size());

    Buffer c_output(o_size, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    g_output.copy_to(c_output);
    c_output.read(output.data(), o_size);

    return output;
}

std::vector<Vec4> BVHPipe::run_rtx(const std::vector<Vec4>& queries, int& time) {

    const auto& bvh_tris = bvh.get_triangles();

    size_t q_size = queries.size() * sizeof(Vec4);
    size_t o_size = q_size / 2;

    Buffer g_queries(q_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY);
    g_queries.write_staged(queries.data(), q_size);

    Buffer g_output(o_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);

    {
        VkWriteDescriptorSetAccelerationStructureKHR acc_info = {};
        acc_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        acc_info.accelerationStructureCount = 1;
        acc_info.pAccelerationStructures = &TLAS->accel;

        VkWriteDescriptorSet a_write = {};
        a_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        a_write.dstSet = rt_pipe->descriptor_sets[vk().frame()];
        a_write.dstBinding = 2;
        a_write.dstArrayElement = 0;
        a_write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        a_write.descriptorCount = 1;
        a_write.pNext = &acc_info;

        VkWriteDescriptorSet writes[] = {
            write_buf(g_queries, rt_pipe, 0), write_buf(g_output, rt_pipe, 1), a_write};
        vkUpdateDescriptorSets(vk().device(), 3, writes, 0, nullptr);

        VkCommandBuffer cmds = vk().begin_one_time();

        vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipe->p_layout, 0, 1,
                                &rt_pipe->descriptor_sets[vk().frame()], 0, nullptr);
        vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipe->pipe);

        Constants consts;
        vkCmdPushConstants(cmds, rt_pipe->p_layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0,
                           sizeof(consts), &consts);

        unsigned int groupSize = align_up(vk().rtx.properties.shaderGroupHandleSize,
                                        vk().rtx.properties.shaderGroupBaseAlignment);
        unsigned int groupStride = groupSize;
        VkDeviceAddress sbt_addr = sbt->address();

        using Stride = VkStridedDeviceAddressRegionKHR;
        std::array<Stride, 4> addrs{Stride{sbt_addr + 0u * groupSize, groupStride, groupSize}, // raygen
                                    Stride{sbt_addr + 1u * groupSize, groupStride, groupSize}, // miss
                                    Stride{sbt_addr + 2u * groupSize, groupStride, groupSize}, // hit
                                    Stride{0u, 0u, 0u}};

        
        vkResetQueryPool(vk().device(), querypool, 0, 2);

        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 0);
        vk().rtx.vkCmdTraceRaysKHR(cmds, &addrs[0], &addrs[1], &addrs[2], &addrs[3], queries.size() / 2, 1, 1);
        vkCmdWriteTimestamp(cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, querypool, 1);

        vk().end_one_time(cmds);

        size_t data[2];
        vkGetQueryPoolResults(vk().device(), querypool, 0, 2, 2 * sizeof(size_t), data, sizeof(size_t), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
        time = (data[1] - data[0]) / 1000000;
    }

    std::vector<Vec4> output(queries.size() / 2);

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

    for(int w = 0; w < WIDE_PIPES; w++) { // WIDE
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
                                             &wide_pipe[w]->d_layout));

        std::vector<VkDescriptorSetLayout> layouts(Manager::MAX_IN_FLIGHT, wide_pipe[w]->d_layout);

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = vk().pool();
        alloc_info.descriptorSetCount = Manager::MAX_IN_FLIGHT;
        alloc_info.pSetLayouts = layouts.data();

        wide_pipe[w]->descriptor_sets.resize(Manager::MAX_IN_FLIGHT);
        VK_CHECK(vkAllocateDescriptorSets(vk().device(), &alloc_info,
                                          wide_pipe[w]->descriptor_sets.data()));
    }

    { // OBB
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
                                             &obb_pipe->d_layout));

        std::vector<VkDescriptorSetLayout> layouts(Manager::MAX_IN_FLIGHT, obb_pipe->d_layout);

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = vk().pool();
        alloc_info.descriptorSetCount = Manager::MAX_IN_FLIGHT;
        alloc_info.pSetLayouts = layouts.data();

        obb_pipe->descriptor_sets.resize(Manager::MAX_IN_FLIGHT);
        VK_CHECK(vkAllocateDescriptorSets(vk().device(), &alloc_info,
                                          obb_pipe->descriptor_sets.data()));
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

    { // RTX
        VkDescriptorSetLayoutBinding i_bind = {};
        i_bind.binding = 0;
        i_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        i_bind.descriptorCount = 1;
        i_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutBinding o_bind = {};
        o_bind.binding = 1;
        o_bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        o_bind.descriptorCount = 1;
        o_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutBinding a_bind = {};
        a_bind.binding = 2;
        a_bind.descriptorCount = 1;
        a_bind.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        a_bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutBinding bindings[] = {i_bind, o_bind, a_bind};

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 3;
        layout_info.pBindings = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(vk().device(), &layout_info, nullptr, &rt_pipe->d_layout));

        std::vector<VkDescriptorSetLayout> layouts(Manager::MAX_IN_FLIGHT, rt_pipe->d_layout);

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = vk().pool();
        alloc_info.descriptorSetCount = Manager::MAX_IN_FLIGHT;
        alloc_info.pSetLayouts = layouts.data();

        rt_pipe->descriptor_sets.resize(Manager::MAX_IN_FLIGHT);
        VK_CHECK(vkAllocateDescriptorSets(vk().device(), &alloc_info, rt_pipe->descriptor_sets.data()));
    }
}

} // namespace VK
