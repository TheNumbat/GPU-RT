
#pragma once

#include <SDL2/SDL.h>
#include <lib/mathlib.h>
#include <optional>
#include <vk/vulkan.h>

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

    std::optional<SDL_Event> event();
    bool is_down(SDL_Scancode key);

    bool begin_frame();

private:
    float prev_dpi = 0.0f;
    float prev_scale = 0.0f;

    void init();
    void set_dpi();
    void shutdown();

    SDL_Window* window = nullptr;
    const Uint8* keybuf = nullptr;
};
