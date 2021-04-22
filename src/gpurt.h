
#pragma once

#include <SDL2/SDL.h>
#include <lib/mathlib.h>
#include <scene/scene.h>

#include "platform/window.h"

class GPURT {
public:
    GPURT(Window& window, std::string scene_file);
    ~GPURT();

    void loop();

private:
    void event(SDL_Event e);
    void render();
    void apply_window_dim(Vec2 new_dim);

    void UIsidebar();
    void load_scene(bool clear);
    void edit_material(Material& opt);

    void build_images();
    void build_accel();
    void build_frames();
    void build_pipe();
    void build_pass();
    void build_rt();

    static inline const char* scene_file_types = "dae,obj,fbx,glb,gltf,3ds,blend,stl,ply";
    static inline const char* image_file_types = "exr,hdr,hdri,jpg,jpeg,png,tga,bmp,psd,gif";

    unsigned int selected_id = 0;

    enum class Camera_Control { none, orbit, move };
    Camera_Control cam_mode = Camera_Control::none;
    Camera cam;

    Window& window;
    Scene scene;

    std::vector<VK::Drop<VK::Accel>> BLAS;
    std::vector<Mat4> BLAS_T;
    VK::Drop<VK::Accel> TLAS;

    struct Frame {
        VK::Drop<VK::Image> color, depth, rt_target;
        VK::Drop<VK::ImageView> color_view, depth_view, rt_target_view;
        VK::Drop<VK::Framebuffer> fb;
    };

    bool use_rt = true;
    bool rebuild_tlas = true;
    bool rebuild_blas = true;

    std::array<Frame, VK::Manager::MAX_IN_FLIGHT> frames;
    VK::MeshPipe mesh_pipe;
    VK::RTPipe rt_pipe;
    VK::Drop<VK::Pass> mesh_pass;
};
