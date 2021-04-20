
#include "gpurt.h"
#include <imgui/imgui.h>
#include <nfd/nfd.h>
#include <util/image.h>

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
}

GPURT::~GPURT() {
}

void GPURT::render() {

    VK::Manager& vk = VK::vk();
    Frame& f = frames[vk.frame()];

    VkCommandBuffer cmds = vk.begin();

    VkClearValue col, depth;
    col.color = {0.22f, 0.22f, 0.22f, 1.0f};
    depth.depthStencil = {0.0f, 0};

    mesh_pipe.update_uniforms(cam);

    mesh_pass->begin(cmds, f.fb, {col, depth});

    scene.for_objs([&, this](const Object& obj) {
        obj.mesh().render(cmds, mesh_pipe.pipe, obj.pose.transform());
    });

    mesh_pass->end(cmds);

    vk.end_frame(frames[vk.frame()].color_view);
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

        f.color->recreate(ext.width, ext.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY);
        f.depth->recreate(ext.width, ext.height, vk.find_depth_format(), VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        f.color->transition(VK_IMAGE_LAYOUT_GENERAL);

        f.color_view->recreate(f.color, VK_IMAGE_ASPECT_COLOR_BIT);
        f.depth_view->recreate(f.depth, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
}

void GPURT::build_pipe() {

    VK::Manager& vk = VK::vk();
    VkExtent2D ext = vk.extent();

    mesh_pipe.recreate(mesh_pass, ext);
    rt_pipe.recreate(scene);

    for(Frame& f : frames) {
        std::vector<std::reference_wrapper<VK::ImageView>> views = {f.color_view, f.depth_view};
        f.fb->recreate(ext.width, ext.height, mesh_pass, views);
    }
}

void GPURT::build_rt() {
    rt_pipe.recreate(scene);

    std::vector<std::reference_wrapper<VK::ImageView>> views;
    for(auto& f : frames)
        views.push_back(f.color_view);
    rt_pipe.use_images(views);
}

void GPURT::build_accel() {

    BLAS.clear();
    scene.for_objs([this](const Object& obj) {
        BLAS.push_back(VK::Accel(obj.mesh()));
        BLAS_T.push_back(obj.pose.transform());
    });

    TLAS.recreate(BLAS, BLAS_T);
    rt_pipe.use_accel(TLAS);
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

    ImGui::Text("Edit Scene");
    if(ImGui::Button("Open Scene")) load_scene(true);
    if(ImGui::Button("Import Objects")) load_scene(false);
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
                ImGui::DragFloat3(label.c_str(), data.data, sens);
            };

            pose.clamp_euler();
            sliders("Position", pose.pos, 0.1f);
            sliders("Rotation", pose.euler, 1.0f);
            sliders("Scale", pose.scale, 0.03f);
            if(ImGui::Button("Delete [del]")) {
                scene.erase(selected_id);
                selected_id = 0;
            }

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

        window.begin_frame();
        UIsidebar();
        render();
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
