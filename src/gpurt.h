
#pragma once

#include <SDL2/SDL.h>
#include <lib/lib.h>

#include "gui/dbg.h"
#include "platform/window.h"

class GPURT {
public:
    GPURT(Window& window);
    ~GPURT();

    void loop();

private:
    void event(SDL_Event e);
    void render();
    void render_ui();
    void apply_window_dim(Vec2 new_dim);

    Window& window;
    Dbg_Gui debug_gui;
};
