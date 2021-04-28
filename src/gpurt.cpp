
#include "gpurt.h"

#include <chrono>
#include <imgui/imgui.h>
#include <nfd/nfd.h>
#include <util/image.h>

constexpr int BRUTE_MAX = 10000;

static std::string bvh_name(VK::BVH_Type type) {
    switch(type) {
    case VK::BVH_Type::none: return "[brute force]: ";
    case VK::BVH_Type::threaded: return "[threaded]: ";
    case VK::BVH_Type::stack: return "[stackful]: ";
    case VK::BVH_Type::stackless: return "[stackless]: ";
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

    scene.load(Scene::Load_Opts(), scene_file, cam);

    build_images();
    build_pass();
    build_pipe();
    build_rt();
    build_accel();

    VK::vk().on_resize([this]() {
        build_images();
        build_pass();
        build_pipe();
    });

    test_cpq(true, VK::BVH_Type::none);
    test_ray(true, VK::BVH_Type::none);
    test_cpq(true, VK::BVH_Type::threaded);
    test_ray(true, VK::BVH_Type::threaded);
}

GPURT::~GPURT() {
}

void GPURT::test_cpq(bool print, VK::BVH_Type type) {

    const VK::Mesh& obj = scene.get(1).mesh();
    cpq_bvh.build(obj, 8);

    std::vector<Vec4> queries;
    std::vector<Vec4> reference;

    std::ifstream f_queries("queries.txt");
    while(f_queries.good()) {
        Vec4 q;
        f_queries >> q.x >> q.y >> q.z;
        if(f_queries.good()) queries.push_back(q);
        if(type == VK::BVH_Type::none && queries.size() >= BRUTE_MAX) break;
    }

    std::ifstream f_cps("points.txt");
    while(f_cps.good()) {
        Vec4 q;
        f_cps >> q.x >> q.y >> q.z;
        if(f_cps.good()) reference.push_back(q);
        if(type == VK::BVH_Type::none && reference.size() >= BRUTE_MAX) break;
    }

    assert(queries.size() == reference.size());

    auto t1 = std::chrono::high_resolution_clock::now();
    auto output = bvh_pipe.cpqs(type, cpq_bvh, queries);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

    if(print) {
        std::cout << "Checking " << bvh_name(type);
        std::cout << queries.size() << " CPQs done in " << ms_int << std::endl;

        for(int i = 0; i < queries.size(); i++) {
            float d_ref = (reference[i].xyz() - queries[i].xyz()).norm();
            float d_comp = (output[i].xyz() - queries[i].xyz()).norm();
            if(std::abs(d_ref - d_comp) > EPS_F) {
                std::cout << "CPQ FAILED: " << reference[i].xyz() << " vs " << output[i].xyz()
                          << std::endl;
            }
        }
    }
}

void GPURT::test_ray(bool print, VK::BVH_Type type) {

    const VK::Mesh& obj = scene.get(1).mesh();
    cpq_bvh.build(obj, 8);

    std::vector<std::pair<Vec4, Vec4>> queries;
    std::vector<Vec4> reference;

    std::ifstream f_queries("rqueries.txt");
    while(f_queries.good()) {
        Vec4 o, d;
        f_queries >> o.x >> o.y >> o.z;
        f_queries >> d.x >> d.y >> d.z;
        if(f_queries.good()) queries.push_back({o, d});
        if(type == VK::BVH_Type::none && queries.size() >= BRUTE_MAX) break;
    }

    std::ifstream f_cps("rpoints.txt");
    while(f_cps.good()) {
        int ok;
        Vec4 q;
        f_cps >> ok >> q.x >> q.y >> q.z;
        if(!ok) q.x = q.y = q.z = INFINITY;
        if(f_cps.good()) reference.push_back(q);
        if(type == VK::BVH_Type::none && reference.size() >= BRUTE_MAX) break;
    }

    assert(queries.size() == reference.size());

    auto t1 = std::chrono::high_resolution_clock::now();
    auto output = bvh_pipe.rays(type, cpq_bvh, queries);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

    if(print) {
        std::cout << "Checking " << bvh_name(type);
        std::cout << queries.size() << " rays done in " << ms_int << std::endl;

        for(int i = 0; i < queries.size(); i++) {
            Vec3 i_ref = reference[i].xyz();
            Vec3 i_comp = output[i].xyz();
            if(!i_ref.valid() && !i_comp.valid()) return;
            if(std::abs(i_ref.x - i_comp.x) > EPS_F || std::abs(i_ref.y - i_comp.y) > EPS_F ||
               std::abs(i_ref.z - i_comp.z) > EPS_F) {
                std::cout << "RAY FAILED: " << reference[i].xyz() << " vs " << output[i].xyz()
                          << std::endl;
            }
        }
    }
}

void GPURT::time_cpqs(int N, VK::BVH_Type type) {

    const VK::Mesh& obj = scene.get(1).mesh();
    cpq_bvh.build(obj, 8);

    std::vector<Vec4> queries = gen_points(N, cpq_bvh.box());

    auto t1 = std::chrono::high_resolution_clock::now();
    auto output = bvh_pipe.cpqs(type, cpq_bvh, queries);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

    std::cout << bvh_name(type) << queries.size() << " CPQs done in " << ms_int << std::endl;
}

void GPURT::time_rays(int N, VK::BVH_Type type) {

    const VK::Mesh& obj = scene.get(1).mesh();
    cpq_bvh.build(obj, 8);

    std::vector<std::pair<Vec4, Vec4>> queries = gen_rays(N, cpq_bvh.box());

    auto t1 = std::chrono::high_resolution_clock::now();
    auto output = bvh_pipe.rays(type, cpq_bvh, queries);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

    std::cout << bvh_name(type) << queries.size() << " rays done in " << ms_int << std::endl;
}

void GPURT::render() {

    VK::Manager& vk = VK::vk();
    Frame& f = frames[vk.frame()];

    VkCommandBuffer cmds = vk.begin();

    if(use_rt) {

        build_accel();

        rt_pipe.use_image(f.rt_target_view);
        rt_pipe.use_accel(TLAS);
        rt_pipe.update_uniforms(cam);
        rt_pipe.trace(cmds, {f.color->w, f.color->h});

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
    }
}

void GPURT::load_scene(bool clear) {

    char* path = nullptr;
    NFD_OpenDialog(scene_file_types, nullptr, &path);
    if(!path) return;

    if(clear) {
        selected_id = 0;
    }

    Scene::Load_Opts load_opt;
    load_opt.new_scene = clear;

    scene.load(load_opt, std::string(path), cam);
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
    if(ImGui::Button("Open Scene")) load_scene(true);
    if(ImGui::Button("Import Objects")) load_scene(false);
    ImGui::Separator();

    ImGui::Checkbox("Use RTX", &use_rt);
    ImGui::Separator();

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

    ImGui::Combo("Type", (int*)&opt.type, Material_Type_Names, (int)Material_Type::count);

    switch(opt.type) {
    case Material_Type::lambertian: {
        ImGui::ColorEdit3("Albedo", opt.albedo.data);
    } break;
    case Material_Type::mirror: {
        ImGui::ColorEdit3("Reflectance", opt.reflectance.data);
    } break;
    case Material_Type::refract: {
        ImGui::ColorEdit3("Transmittance", opt.transmittance.data);
        ImGui::DragFloat("Index of Refraction", &opt.ior, 0.1f, 0.0f,
                         std::numeric_limits<float>::max(), "%.2f");
    } break;
    case Material_Type::glass: {
        ImGui::ColorEdit3("Reflectance", opt.reflectance.data);
        ImGui::ColorEdit3("Transmittance", opt.transmittance.data);
        ImGui::DragFloat("Index of Refraction", &opt.ior, 0.1f, 0.0f,
                         std::numeric_limits<float>::max(), "%.2f");
    } break;
    case Material_Type::diffuse_light: {
        ImGui::ColorEdit3("Emissive", opt.emissive.data);
        ImGui::DragFloat("Intensity", &opt.intensity, 0.1f, 0.0f, std::numeric_limits<float>::max(),
                         "%.2f");
    } break;
    default: break;
    }
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
