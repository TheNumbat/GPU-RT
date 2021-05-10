
#include "gpurt.h"

#include <algorithm>
#include <chrono>
#include <imgui/imgui.h>
#include <nfd/nfd.h>
#include <util/image.h>

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
}

GPURT::~GPURT() {
}

void GPURT::render() {

    VK::Manager& vk = VK::vk();
    Frame& f = frames[vk.frame()];

    VkCommandBuffer cmds = vk.begin();

    if(use_rt) {
        build_accel();

        rt_pipe.use_image(rt_target_view);
        rt_pipe.use_accel(TLAS);
        rt_pipe.update_uniforms(cam);
        rt_pipe.trace(cam, cmds, {rt_target->w, rt_target->h});
        
        rt_target->transition(cmds, VK_IMAGE_LAYOUT_GENERAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        effect_pass->begin(cmds, f.ef_fb, {});
        effect_pipe.tonemap(cmds, rt_target_view);
        effect_pass->end(cmds);

        rt_target->transition(cmds, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_LAYOUT_GENERAL);

    } else {
        VkClearValue col, depth;
        col.color = {0.22f, 0.22f, 0.22f, 1.0f};
        depth.depthStencil = {0.0f, 0};

        mesh_pipe.update_uniforms(cam);
        mesh_pass->begin(cmds, f.m_fb, {col, depth});

        scene.for_objs([&, this](const Object& obj) {
            obj.mesh().render(cmds, mesh_pipe.pipe, Mat4::scale(Vec3{scene.scale}) * obj.pose.transform());
        });

        mesh_pass->end(cmds);
    }
}

void GPURT::build_pass() {

    {
        VkAttachmentDescription color = {};
        color.format = frames[0].color->format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

    {
        VkAttachmentDescription color = {};
        color.format = frames[0].color->format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference color_ref = {};
        color_ref.attachment = 0;
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

        effect_pass->recreate({{color}, {sp}, {d0, d1}});
    }
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

        f.color->transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        f.color_view->recreate(f.color, VK_IMAGE_ASPECT_COLOR_BIT);
        f.depth_view->recreate(f.depth, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    rt_target->recreate(ext.width, ext.height, VK_FORMAT_R32G32B32A32_SFLOAT,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VMA_MEMORY_USAGE_GPU_ONLY);
    rt_target->transition(VK_IMAGE_LAYOUT_GENERAL);
    rt_target_view->recreate(rt_target, VK_IMAGE_ASPECT_COLOR_BIT);
}

void GPURT::build_pipe() {

    VK::Manager& vk = VK::vk();
    VkExtent2D ext = vk.extent();

    effect_pipe.recreate(effect_pass, ext);
    mesh_pipe.recreate(mesh_pass, ext);
    rt_pipe.recreate(scene);

    for(Frame& f : frames) {
        std::vector<std::reference_wrapper<VK::ImageView>> views = {f.color_view, f.depth_view};
        f.m_fb->recreate(ext.width, ext.height, mesh_pass, views);

        std::vector<std::reference_wrapper<VK::ImageView>> views_col = {f.color_view};
        f.ef_fb->recreate(ext.width, ext.height, effect_pass, views_col);
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
        scene.for_objs([this](const Object& obj) { BLAS_T.push_back(Mat4::scale(Vec3{scene.scale}) * obj.pose.transform()); });

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

    bool change = false;
    change = change || ImGui::Checkbox("Use RTX", &use_rt);
    change = change || ImGui::SliderInt("Max Frames", &rt_pipe.max_frames, 1, 2048);
    change = change || ImGui::SliderInt("Samples", &rt_pipe.samples_per_frame, 1, 128);
    change = change || ImGui::SliderInt("Depth", &rt_pipe.max_depth, 1, 32);
    change = change || ImGui::ColorEdit3("ClearCol", rt_pipe.clear.data);
    change = change || ImGui::ColorEdit3("EnvLight", rt_pipe.env.data);
    change = change || ImGui::DragFloat("Intensity", &rt_pipe.env_scale, 0.1f, 0.0f, FLT_MAX);
    change = change || ImGui::Checkbox("Normal Maps", &rt_pipe.use_normal_map);
    if(change) rt_pipe.reset_frame();
    
    ImGui::DragFloat("Exposure", &effect_pipe.exposure, 0.01f, 0.01f, 100.0f);
    ImGui::DragFloat("Gamma", &effect_pipe.gamma, 0.01f, 0.01f, 5.0f);
    ImGui::SliderInt("Tonemap", &effect_pipe.tonemap_type, 0, 1);
    
    if(ImGui::DragFloat("Scale", &scene.scale, 0.01f, 0.01f, 10.0f)) {
        rebuild_tlas = true;
    }

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
