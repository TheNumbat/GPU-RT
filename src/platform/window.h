
#pragma once

#include "vulkan.h"
#include <SDL2/SDL.h>
#include <lib/lib.h>

class Window {
public:
    Window();
    ~Window();

    Vec2 size();
    Vec2 drawable();
    Vec2 scale(Vec2 pt);

    void capture_mouse();
    void release_mouse();
    void set_mouse(Vec2 pos);
    Vec2 get_mouse();
    void grab_mouse();
    void ungrab_mouse();

    Maybe<SDL_Event> event();
    bool is_down(SDL_Scancode key);

    void begin_frame();
    void complete_frame();

private:
    f32 prev_dpi = 0.0f;
    f32 prev_scale = 0.0f;

    void init();
    void set_dpi();
    void shutdown();

    VK::Manager vulkan;
    SDL_Window* window = nullptr;
    const Uint8* keybuf = nullptr;

    static constexpr char imgui_name[] = "ImGui";
    using ImGui_Alloc = Mallocator<imgui_name>;
    friend void* imgui_alloc(usize, void*);
    friend void imgui_free(void*, void*);
};
