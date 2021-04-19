
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
    void render_ui();
    void apply_window_dim(Vec2 new_dim);

    void UIsidebar();
    void load_scene(bool clear);
    void edit_material(Material& opt);
    void build_accel();

    static inline const char* scene_file_types = "dae,obj,fbx,glb,gltf,3ds,blend,stl,ply";
    static inline const char* image_file_types = "exr,hdr,hdri,jpg,jpeg,png,tga,bmp,psd,gif";

    unsigned int selected_id = 0;

    enum class Camera_Control { none, orbit, move };
    Camera_Control cam_mode = Camera_Control::none;
    Camera cam;

    Window& window;
    Scene scene;
    std::vector<VK::Accel> BLAS;
    std::vector<Mat4> BLAS_T;
    VK::Accel TLAS;
};
