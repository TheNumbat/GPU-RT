
#include "gpurt.h"
#include "util/profile.h"
#include <imgui/imgui.h>

GPURT::GPURT(Window& window) : window(window) {
}

GPURT::~GPURT() {
}

void GPURT::render() {
}

void GPURT::render_ui() {
    debug_gui.profiler();
}

void GPURT::loop() {

    bool running = true;
    while(running) {

        Profiler::begin_frame();

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
        render();
        render_ui();
        window.complete_frame();

        Mframe::reset();
        Profiler::end_frame();
    }
}

void GPURT::event(SDL_Event e) {

    switch(e.type) {

    case SDL_WINDOWEVENT: {
        if(e.window.event == SDL_WINDOWEVENT_RESIZED ||
           e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            apply_window_dim(window.drawable());
        }
    } break;
    }
}

void GPURT::apply_window_dim(Vec2 new_dim) {
}
