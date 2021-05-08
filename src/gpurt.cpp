
#include "gpurt.h"

#include <algorithm>
#include <chrono>
#include <imgui/imgui.h>
#include <nfd/nfd.h>
#include <util/image.h>

constexpr int QUERY_MAX = 1000000;

static std::string bvh_name(VK::BVH_Type type, int w) {
    switch(type) {
    case VK::BVH_Type::none: return "[brute force]: ";
    case VK::BVH_Type::threaded: return "[threaded]: ";
    case VK::BVH_Type::stack: return "[stackful]: ";
    case VK::BVH_Type::stackless: return "[stackless]: ";
    case VK::BVH_Type::obb: return "[obb stackful]: ";
    case VK::BVH_Type::obb_stackless: return "[obb stackless]: ";
    case VK::BVH_Type::wide: return "[wide " + std::to_string(1 << w) + "]: ";
    case VK::BVH_Type::wide_max: return "[wide_max " + std::to_string(1 << w) + "]: ";
    case VK::BVH_Type::wide_sort: return "[wide_sort " + std::to_string(1 << w) + "]: ";
    case VK::BVH_Type::RTX: return "[RTX]: ";
    }
    return "[unknown]: ";
}

static void split_box(BBox boundingBox, std::vector<BBox>& boxes, int depth) {
    if(depth == 0) {
        boxes.emplace_back(boundingBox);
    } else {
        Vec3 e = boundingBox.max - boundingBox.min;
        int splitDim = 0;
        if(e.z > e.y && e.z > e.x) splitDim = 2;
        if(e.y > e.z && e.y > e.x) splitDim = 1;
        if(e.x > e.y && e.x > e.z) splitDim = 0;

        float splitCoord = (boundingBox.min[splitDim] + boundingBox.max[splitDim]) * 0.5f;
        BBox boxLeft = boundingBox;
        boxLeft.max[splitDim] = splitCoord;
        split_box(boxLeft, boxes, depth - 1);
        BBox boxRight = boundingBox;
        boxRight.min[splitDim] = splitCoord;
        split_box(boxRight, boxes, depth - 1);
    }
}

static float randf() {
    return (float)rand() / RAND_MAX;
}

static std::vector<Vec4> gen_points(int N, const BBox& boundingBox) {

    std::vector<Vec4> points;
    std::vector<BBox> boxes;
    split_box(boundingBox, boxes, 6);

    int nBoxes = (int)boxes.size();
    int nQueriesPerBox = (int)std::ceil((float)N / nBoxes);

    for(int i = 0; i < nBoxes; i++) {
        Vec3 e = boxes[i].max - boxes[i].min;

        for(int j = 0; j < nQueriesPerBox; j++) {
            Vec3 o = boxes[i].min + e * Vec3{randf(), randf(), randf()};
            points.emplace_back(Vec4(o, 0.0f));
        }
    }
    points.resize(N);
    return points;
}

static std::vector<std::pair<Vec4, Vec4>> gen_rays(int N, const BBox& boundingBox) {

    std::vector<std::pair<Vec4, Vec4>> rays;
    std::vector<BBox> boxes;
    split_box(boundingBox, boxes, 6);

    int nBoxes = (int)boxes.size();
    int nQueriesPerBox = (int)std::ceil((float)N / nBoxes);

    for(int i = 0; i < nBoxes; i++) {
        Vec3 e = boxes[i].max - boxes[i].min;

        for(int j = 0; j < nQueriesPerBox; j++) {
            Vec3 o = boxes[i].min + e * Vec3{randf(), randf(), randf()};
            Vec3 d = Vec3{randf(), randf(), randf()};
            rays.push_back({Vec4(o, 0.0f), Vec4(d.unit(), 0.0f)});
        }
    }
    rays.resize(N);
    return rays;
}

GPURT::GPURT(Window& window, std::string scene_file) : window(window), cam(window.drawable()) {

    scene.load(scene_file, cam);

    build_images();
    build_pass();
    build_pipe();
    build_rt();
    build_accel();

    VK::vk().on_resize([this]() {
        build_images();
        build_pass();
        build_pipe();
        rt_pipe.reset_frame();
    });

#if 0
    std::ifstream fin_queries("queries.txt");
    while(fin_queries.good()) {
        Vec4 q;
        fin_queries >> q.x >> q.y >> q.z;
        if(fin_queries.good()) f_queries.push_back(q);
        if(f_queries.size() >= QUERY_MAX) break;
    }

    std::ifstream fin_points("points.txt");
    while(fin_points.good()) {
        Vec4 q;
        fin_points >> q.x >> q.y >> q.z;
        if(fin_points.good()) f_reference.push_back(q);
        if(f_reference.size() >= QUERY_MAX) break;
    }

    assert(f_queries.size() == f_reference.size());

    std::ifstream fin_rqueries("rqueries.txt");
    while(fin_rqueries.good()) {
        Vec4 o, d;
        fin_rqueries >> o.x >> o.y >> o.z;
        fin_rqueries >> d.x >> d.y >> d.z;
        if(fin_rqueries.good()) f_rqueries.push_back({o, d});
        if(f_rqueries.size() >= QUERY_MAX) break;
    }

    std::ifstream fin_rpoints("rpoints.txt");
    while(fin_rpoints.good()) {
        int ok;
        Vec4 q;
        fin_rpoints >> ok >> q.x >> q.y >> q.z;
        if(!ok) q.x = q.y = q.z = INFINITY;
        if(fin_rpoints.good()) f_rreference.push_back(q);
        if(f_rreference.size() >= QUERY_MAX) break;
    }

    assert(f_rqueries.size() == f_rreference.size());

    const VK::Mesh& obj = scene.get(1).mesh();
    bvh_pipe.build(obj, 16);

    if(scene_file == "bunny.obj") {
        std::cout << "Testing file: " << scene_file << std::endl;
        run_tests();
    }
    std::cout << "Benchmarking random coherent: " << std::endl;
    benchmark_rng();
    std::cout << "Benchmarking primary rays: " << std::endl;
    benchmark_primary();
#endif
}

GPURT::~GPURT() {
}

void GPURT::benchmark_primary() {

    const VK::Mesh& obj = scene.get(1).mesh();
    bvh_pipe.build(obj, 1);

    Mat4 iV = cam.get_view().inverse();
    Mat4 iP = cam.get_proj().inverse();
    Vec4 o = Vec4(cam.pos(), 0.0f);

    std::vector<std::pair<Vec4,Vec4>> queries;
    for(int y = 0; y < 720; y++) {
        for(int x = 0; x < 1280; x++) {

            Vec2 pixelCenter = Vec2(x,y) + Vec2(0.5f);
            Vec2 inUV = pixelCenter / Vec2(x,y);
            Vec2 s = inUV * 2.0f - 1.0f;
            Vec4 target = iP * Vec4(s.x, s.y, 0.0f, 1.0f);
            Vec4 d = iV * Vec4(target.xyz(), 0.0f);
            queries.push_back({o, d});
        }
    }

    time_rays(queries, VK::BVH_Type::threaded);
    time_rays(queries, VK::BVH_Type::stack);
    time_rays(queries, VK::BVH_Type::stackless);
    time_rays(queries, VK::BVH_Type::obb);
    time_rays(queries, VK::BVH_Type::obb_stackless);
    time_rays(queries, VK::BVH_Type::RTX);
    time_rays(queries, VK::BVH_Type::wide, 1);
    time_rays(queries, VK::BVH_Type::wide, 2);
    time_rays(queries, VK::BVH_Type::wide, 3);
    time_rays(queries, VK::BVH_Type::wide, 4);
    time_rays(queries, VK::BVH_Type::wide_max, 2);
    time_rays(queries, VK::BVH_Type::wide_max, 3);
    time_rays(queries, VK::BVH_Type::wide_max, 4);
    time_rays(queries, VK::BVH_Type::wide_sort, 2);
    time_rays(queries, VK::BVH_Type::wide_sort, 3);
    time_rays(queries, VK::BVH_Type::wide_sort, 4);
}

void GPURT::benchmark_rng() {

    std::vector<Vec4> queries = gen_points(QUERY_MAX, bvh_pipe.box());
    std::vector<std::pair<Vec4,Vec4>> rqueries = gen_rays(10 * QUERY_MAX, bvh_pipe.box());

    time_cpqs(queries, VK::BVH_Type::threaded);
    time_cpqs(queries, VK::BVH_Type::stack);
    time_cpqs(queries, VK::BVH_Type::stackless);
    time_cpqs(queries, VK::BVH_Type::obb);
    time_cpqs(queries, VK::BVH_Type::obb_stackless);
    time_cpqs(queries, VK::BVH_Type::wide, 1);
    time_cpqs(queries, VK::BVH_Type::wide, 2);
    time_cpqs(queries, VK::BVH_Type::wide, 3);
    time_cpqs(queries, VK::BVH_Type::wide, 4);
    time_cpqs(queries, VK::BVH_Type::wide_max, 2);
    time_cpqs(queries, VK::BVH_Type::wide_max, 3);
    time_cpqs(queries, VK::BVH_Type::wide_max, 4);
    time_cpqs(queries, VK::BVH_Type::wide_sort, 2);
    time_cpqs(queries, VK::BVH_Type::wide_sort, 3);
    time_cpqs(queries, VK::BVH_Type::wide_sort, 4);

    time_rays(rqueries, VK::BVH_Type::threaded);
    time_rays(rqueries, VK::BVH_Type::stack);
    time_rays(rqueries, VK::BVH_Type::stackless);
    time_rays(rqueries, VK::BVH_Type::obb);
    time_rays(rqueries, VK::BVH_Type::obb_stackless);
    time_rays(rqueries, VK::BVH_Type::RTX);
    time_rays(rqueries, VK::BVH_Type::wide, 1);
    time_rays(rqueries, VK::BVH_Type::wide, 2);
    time_rays(rqueries, VK::BVH_Type::wide, 3);
    time_rays(rqueries, VK::BVH_Type::wide, 4);
    time_rays(rqueries, VK::BVH_Type::wide_max, 2);
    time_rays(rqueries, VK::BVH_Type::wide_max, 3);
    time_rays(rqueries, VK::BVH_Type::wide_max, 4);
    time_rays(rqueries, VK::BVH_Type::wide_sort, 2);
    time_rays(rqueries, VK::BVH_Type::wide_sort, 3);
    time_rays(rqueries, VK::BVH_Type::wide_sort, 4);
}

void GPURT::run_tests() {

    test_cpq(f_queries, f_reference, true, VK::BVH_Type::none);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::threaded);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::stack);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::stackless);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide, 1);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide, 2);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide, 3);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide, 4);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide_max, 2);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide_max, 3);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide_max, 4);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide_sort, 2);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide_sort, 3);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::wide_sort, 4);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::obb);
    test_cpq(f_queries, f_reference, true, VK::BVH_Type::obb_stackless);

    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::none);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::threaded);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::stack);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::stackless);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::RTX);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide, 1);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide, 2);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide, 3);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide, 4);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide_max, 2);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide_max, 3);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide_max, 4);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide_sort, 2);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide_sort, 3);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::wide_sort, 4);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::obb);
    test_ray(f_rqueries, f_rreference, true, VK::BVH_Type::obb_stackless);
}

void GPURT::test_cpq(const std::vector<Vec4>& queries, const std::vector<Vec4>& reference,
                     bool print, VK::BVH_Type type, int w) {

    int ms_int = 0;
    auto output = bvh_pipe.cpqs(type, queries, ms_int, w);

    if(print) {
        size_t n_queries =
            type == VK::BVH_Type::none ? std::min(VK::BRUTE_MAX, queries.size()) : queries.size();
        std::cout << "Checking " << bvh_name(type, w);
        std::cout << n_queries << " CPQs done in " << ms_int << std::endl;

        for(int i = 0; i < n_queries; i++) {
            float d_ref = (reference[i].xyz() - queries[i].xyz()).norm();
            float d_comp = (output[i].xyz() - queries[i].xyz()).norm();
            if(std::abs(d_ref - d_comp) > 0.001f) {
                std::cout << "CPQ FAILED: " << reference[i].xyz() << " vs " << output[i].xyz()
                          << std::endl;
            }
        }
    }
}

void GPURT::test_ray(const std::vector<std::pair<Vec4, Vec4>>& queries,
                     const std::vector<Vec4>& reference, bool print, VK::BVH_Type type, int w) {

    int ms_int = 0;
    auto output = bvh_pipe.rays(type, queries, ms_int, w);

    if(print) {
        size_t n_queries =
            type == VK::BVH_Type::none ? std::min(VK::BRUTE_MAX, queries.size()) : queries.size();
        std::cout << "Checking " << bvh_name(type, w);
        std::cout << n_queries << " rays done in " << ms_int << std::endl;

        for(int i = 0; i < n_queries; i++) {
            Vec3 i_ref = reference[i].xyz();
            Vec3 i_comp = output[i].xyz();
            if(std::abs(i_ref.x - i_comp.x) > 0.001f || std::abs(i_ref.y - i_comp.y) > 0.001f ||
               std::abs(i_ref.z - i_comp.z) > 0.001f || i_ref.valid() != i_comp.valid()) {
                std::cout << "RAY FAILED: " << reference[i].xyz() << " vs " << output[i].xyz()
                          << std::endl;
            }
        }
    }
}

void GPURT::time_cpqs(const std::vector<Vec4>& queries, VK::BVH_Type type, int w) {

    int ms_int = 0;
    auto output = bvh_pipe.cpqs(type, queries, ms_int, w);
    std::cout << bvh_name(type, w) << queries.size() << " CPQs done in " << ms_int << std::endl;
}

void GPURT::time_rays(const std::vector<std::pair<Vec4, Vec4>>& queries, VK::BVH_Type type, int w) {

    int ms_int = 0;
    auto output = bvh_pipe.rays(type, queries, ms_int, w);
    std::cout << bvh_name(type, w) << queries.size() << " rays done in " << ms_int << std::endl;
}

void GPURT::render() {

    VK::Manager& vk = VK::vk();
    Frame& f = frames[vk.frame()];

    VkCommandBuffer cmds = vk.begin();

    if(use_rt) {

        // run_tests();
        // benchmark_rng();
        
        build_accel();

        rt_pipe.use_image(f.rt_target_view);
        rt_pipe.use_accel(TLAS);
        rt_pipe.update_uniforms(cam);
        rt_pipe.trace(cam, cmds, {f.color->w, f.color->h});

        VkImageBlit region = {};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[1] = {(int)f.rt_target->w, (int)f.rt_target->h, 1};
        region.srcOffsets[1] = {(int)f.rt_target->w, (int)f.rt_target->h, 1};

        f.rt_target->transition(cmds, VK_IMAGE_LAYOUT_GENERAL,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        f.color->transition(cmds, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkCmdBlitImage(cmds, f.rt_target->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, f.color->img,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);
        f.color->transition(cmds, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        f.rt_target->transition(cmds, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_IMAGE_LAYOUT_GENERAL);

    } else {
        VkClearValue col, depth;
        col.color = {0.22f, 0.22f, 0.22f, 1.0f};
        depth.depthStencil = {0.0f, 0};

        mesh_pipe.update_uniforms(cam);
        mesh_pass->begin(cmds, f.fb, {col, depth});

        scene.for_objs([&, this](const Object& obj) {
            obj.mesh().render(cmds, mesh_pipe.pipe, obj.pose.transform());
        });

        mesh_pass->end(cmds);
    }
}

void GPURT::build_pass() {

    VkAttachmentDescription color = {};
    color.format = frames[0].color->format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depth = {};
    depth.format = frames[0].depth->format;
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

    VkSubpassDependency d0 = {}, d1 = {};
    d0.srcSubpass = VK_SUBPASS_EXTERNAL;
    d0.dstSubpass = 0;
    d0.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    d0.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    d0.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    d0.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    d0.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    d1.srcSubpass = 0;
    d1.dstSubpass = VK_SUBPASS_EXTERNAL;
    d1.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    d1.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    d1.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    d1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    d1.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VK::Pass::Subpass sp;
    sp.bind = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.color = {color_ref};
    sp.depth = depth_ref;

    mesh_pass->recreate({{color, depth}, {sp}, {d0, d1}});
}

void GPURT::build_images() {

    VK::Manager& vk = VK::vk();
    VkExtent2D ext = vk.extent();
    for(Frame& f : frames) {

        f.color->recreate(ext.width, ext.height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY);
        f.depth->recreate(ext.width, ext.height, vk.find_depth_format(), VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        f.rt_target->recreate(ext.width, ext.height, VK_FORMAT_R32G32B32A32_SFLOAT,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              VMA_MEMORY_USAGE_GPU_ONLY);

        f.color->transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        f.rt_target->transition(VK_IMAGE_LAYOUT_GENERAL);

        f.color_view->recreate(f.color, VK_IMAGE_ASPECT_COLOR_BIT);
        f.depth_view->recreate(f.depth, VK_IMAGE_ASPECT_DEPTH_BIT);
        f.rt_target_view->recreate(f.rt_target, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void GPURT::build_pipe() {

    VK::Manager& vk = VK::vk();
    VkExtent2D ext = vk.extent();

    mesh_pipe.recreate(mesh_pass, ext);
    rt_pipe.recreate(scene);
    bvh_pipe.recreate();

    for(Frame& f : frames) {
        std::vector<std::reference_wrapper<VK::ImageView>> views = {f.color_view, f.depth_view};
        f.fb->recreate(ext.width, ext.height, mesh_pass, views);
    }
}

void GPURT::build_rt() {
    rt_pipe.recreate(scene);
}

void GPURT::build_accel() {

    if(rebuild_blas) {
        BLAS.clear();
        scene.for_objs([this](const Object& obj) { BLAS.push_back({VK::Accel(obj.mesh())}); });
        rebuild_blas = false;
    }

    if(rebuild_tlas) {

        BLAS_T.clear();
        scene.for_objs([this](const Object& obj) { BLAS_T.push_back(obj.pose.transform()); });

        TLAS.drop();
        TLAS->recreate(BLAS, BLAS_T);
        rebuild_tlas = false;

        rt_pipe.recreate(scene);
    }
}

void GPURT::load_scene() {

    char* path = nullptr;
    NFD_OpenDialog(scene_file_types, nullptr, &path);
    if(!path) return;

    selected_id = 0;

    scene.load(std::string(path), cam);
    free(path);

    rebuild_blas = rebuild_tlas = true;
    build_accel();
}

void GPURT::UIsidebar() {

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

    Vec2 window_dim = window.drawable();
    ImGui::SetNextWindowPos({0.0, 0.0f});
    ImGui::SetNextWindowSizeConstraints({window_dim.x / 4.75f, window_dim.y},
                                        {window_dim.x, window_dim.y});
    ImGui::Begin("Menu", nullptr, flags);

    ImGui::Text("FPS: %f", ImGui::GetIO().Framerate);

    ImGui::Text("Edit Scene");
    if(ImGui::Button("Open Scene")) load_scene();
    ImGui::Separator();

    ImGui::Checkbox("Use RTX", &use_rt);
    ImGui::Separator();

    ImGui::SliderInt("Max Frames", &rt_pipe.max_frames, 1, 128);
    ImGui::SliderInt("Samples", &rt_pipe.samples_per_frame, 1, 128);

    if(!scene.empty()) {
        ImGui::Text("Select an Object");

        unsigned int s = scene.size();
        unsigned int i = 0;
        ImGui::Columns(2);

        scene.for_objs([&, this](Object& obj) {
            if(i++ == (s + 1) / 2) ImGui::NextColumn();
            ImGui::PushID(obj.id());

            ImGui::Text("Obj %d", obj.id());

            bool is_selected = obj.id() == selected_id;
            ImGui::SameLine();
            if(ImGui::Checkbox("##selected", &is_selected)) {
                if(is_selected)
                    selected_id = obj.id();
                else
                    selected_id = 0;
            }

            ImGui::PopID();
        });

        ImGui::Columns();

        ImGui::Separator();
    }

    if(selected_id) {

        Object& selected = scene.get(selected_id);
        Pose& pose = selected.pose;

        if(ImGui::CollapsingHeader("Edit Pose")) {
            ImGui::Indent();

            auto sliders = [](std::string label, Vec3& data, float sens) {
                return ImGui::DragFloat3(label.c_str(), data.data, sens);
            };

            pose.clamp_euler();

            bool u = false;
            u = u || sliders("Position", pose.pos, 0.1f);
            u = u || sliders("Rotation", pose.euler, 1.0f);
            u = u || sliders("Scale", pose.scale, 0.03f);

            if(ImGui::Button("Delete [del]")) {
                scene.erase(selected_id);
                selected_id = 0;
                rebuild_blas = true;
                rebuild_tlas = true;
            }

            if(u) rebuild_tlas = true;

            ImGui::Unindent();
        }

        if(ImGui::CollapsingHeader("Edit Material")) {
            ImGui::Indent();
            edit_material(selected.material);
            ImGui::Unindent();
        }
    }

    ImGui::End();
}

void GPURT::edit_material(Material& opt) {

    ImGui::ColorEdit3("Albedo", opt.albedo.data);
    ImGui::ColorEdit3("Emissive", opt.emissive.data);
    ImGui::DragFloat2("Metal/Rough", opt.metal_rough.data, 0.1f, 0.0f, 1.0f);

    ImGui::SliderInt("Albedo tex", &opt.albedo_tex, -1, scene.n_textures() - 1);
    ImGui::SliderInt("Emissive tex", &opt.emissive_tex, -1, scene.n_textures() - 1);
    ImGui::SliderInt("Metal/Rough tex", &opt.metal_rough_tex, -1, scene.n_textures() - 1);
}

void GPURT::loop() {

    bool running = true;
    while(running) {

        while(auto opt = window.event()) {

            const SDL_Event& evt = opt.value();
            switch(evt.type) {
            case SDL_QUIT: {
                running = false;
            } break;
            }

            event(evt);
        }

        bool skip_render = window.begin_frame();
        UIsidebar();

        if(!skip_render) render();

        VK::vk().end_frame(frames[VK::vk().frame()].color_view);
    }
}

void GPURT::event(SDL_Event e) {

    ImGuiIO& IO = ImGui::GetIO();
    IO.DisplayFramebufferScale = window.scale(Vec2{1.0f, 1.0f});

    switch(e.type) {

    case SDL_WINDOWEVENT: {
        if(e.window.event == SDL_WINDOWEVENT_RESIZED ||
           e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            apply_window_dim(window.drawable());
        }
    } break;

    case SDL_MOUSEMOTION: {

        Vec2 d(e.motion.xrel, e.motion.yrel);
        Vec2 p = window.scale(Vec2{e.button.x, e.button.y});
        Vec2 dim = window.drawable();
        Vec2 n = Vec2(2.0f * p.x / dim.x - 1.0f, 2.0f * p.y / dim.y - 1.0f);

        if(cam_mode == Camera_Control::orbit) {
            cam.mouse_orbit(d);
        } else if(cam_mode == Camera_Control::move) {
            cam.mouse_move(d);
        }

    } break;

    case SDL_MOUSEBUTTONDOWN: {

        if(IO.WantCaptureMouse) break;

        Vec2 p = window.scale(Vec2{e.button.x, e.button.y});
        Vec2 dim = window.drawable();
        Vec2 n = Vec2(2.0f * p.x / dim.x - 1.0f, 2.0f * p.y / dim.y - 1.0f);

        if(e.button.button == SDL_BUTTON_LEFT) {
            if(cam_mode == Camera_Control::none &&
               (window.is_down(SDL_SCANCODE_LSHIFT) | window.is_down(SDL_SCANCODE_RSHIFT))) {
                cam_mode = Camera_Control::orbit;
            }
        } else if(e.button.button == SDL_BUTTON_RIGHT) {
            if(cam_mode == Camera_Control::none) {
                cam_mode = Camera_Control::move;
            }
        } else if(e.button.button == SDL_BUTTON_MIDDLE) {
            cam_mode = Camera_Control::orbit;
        }

    } break;

    case SDL_MOUSEBUTTONUP: {

        Vec2 p = window.scale(Vec2{e.button.x, e.button.y});
        Vec2 dim = window.drawable();
        Vec2 n = Vec2(2.0f * p.x / dim.x - 1.0f, 2.0f * p.y / dim.y - 1.0f);

        if(e.button.button == SDL_BUTTON_LEFT) {
            if(!IO.WantCaptureMouse) {
                window.ungrab_mouse();
                break;
            }
        }

        if((e.button.button == SDL_BUTTON_LEFT && cam_mode == Camera_Control::orbit) ||
           (e.button.button == SDL_BUTTON_MIDDLE && cam_mode == Camera_Control::orbit) ||
           (e.button.button == SDL_BUTTON_RIGHT && cam_mode == Camera_Control::move)) {
            cam_mode = Camera_Control::none;
        }

    } break;

    case SDL_MOUSEWHEEL: {
        if(IO.WantCaptureMouse) break;
        cam.mouse_radius((float)e.wheel.y);
    } break;
    }
}

void GPURT::apply_window_dim(Vec2 new_dim) {
}
