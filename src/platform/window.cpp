
#include "window.h"
#include "font.dat"

#include "imgui_impl_sdl.h"
#include <imgui/imgui.h>

#include <util/profile.h>

#ifdef _WIN32
#include <ShellScalingApi.h>
#include <windows.h>
extern "C" {
__declspec(dllexport) bool NvOptimusEnablement = true;
__declspec(dllexport) bool AmdPowerXpressRequestHighPerformance = true;
}
#endif

void* imgui_alloc(usize sz, void*) {
    return Window::ImGui_Alloc::alloc<u8>(sz);
}

void imgui_free(void* mem, void*) {
    Window::ImGui_Alloc::dealloc(mem);
}

Window::Window() {
    init();
}

Window::~Window() {
    shutdown();
}

void Window::init() {

#ifdef _WIN32
    if(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE) != S_OK)
        warn("Failed to set process DPI aware.");
#endif

    if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        die("Failed to initialize SDL: %s", SDL_GetError());
    }

    window = SDL_CreateWindow("FCPW-GPU", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_VULKAN);
    if(!window) {
        die("Failed to create window: %s", SDL_GetError());
    }

    keybuf = SDL_GetKeyboardState(null);

    ImGui::SetAllocatorFunctions(imgui_alloc, imgui_free, null);
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(window);

    set_dpi();
    vulkan.init(window);
    ImGui::StyleColorsDark();
}

void Window::set_dpi() {

    f32 dpi;
    i32 index = SDL_GetWindowDisplayIndex(window);
    if(index < 0) {
        return;
    }
    if(SDL_GetDisplayDPI(index, null, &dpi, null)) {
        return;
    }
    f32 scale = drawable().x / size().x;
    if(prev_dpi == dpi && prev_scale == scale) return;

    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    style.WindowRounding = 0.0f;
#ifndef __APPLE__
    style.ScaleAllSizes(0.8f * dpi / 96.0f);
#else
    style.ScaleAllSizes(0.8f);
#endif
    ImGui::GetStyle() = style;

    ImGuiIO& IO = ImGui::GetIO();
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    IO.IniFilename = nullptr;
    IO.Fonts->Clear();
#ifdef __APPLE__
    IO.Fonts->AddFontFromMemoryTTF(font_ttf, font_ttf_len, 14.0f * scale, &config);
    IO.FontGlobalScale = 1.0f / scale;
#else
    IO.Fonts->AddFontFromMemoryTTF(font_ttf, font_ttf_len, 14.0f / 96.0f * dpi, &config);
#endif
    IO.Fonts->Build();

    prev_dpi = dpi;
    prev_scale = scale;
}

bool Window::is_down(SDL_Scancode key) {
    return keybuf[key];
}

void Window::shutdown() {

    ImGui_ImplSDL2_Shutdown();
    vulkan.destroy();

    ImGui::DestroyContext();
    SDL_DestroyWindow(window);
    window = null;
    SDL_Quit();
}

void Window::complete_frame() {

    ImGui::Render();
    vulkan.end_frame();
}

Maybe<SDL_Event> Window::event() {

    SDL_Event e;
    if(SDL_PollEvent(&e)) {

        ImGui_ImplSDL2_ProcessEvent(&e);

        switch(e.type) {
        case SDL_WINDOWEVENT: {
            if(e.window.event == SDL_WINDOWEVENT_RESIZED ||
               e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {

                vulkan.trigger_resize();
            }
        } break;
        }

        return Maybe<SDL_Event>(std::move(e));
    }
    return Maybe<SDL_Event>();
}

void Window::begin_frame() {
    set_dpi();
    ImGuiIO& IO = ImGui::GetIO();
    IO.DisplayFramebufferScale = scale({1.0f, 1.0f});

    ImGui_ImplSDL2_NewFrame(window);
    vulkan.begin_frame();
    ImGui::NewFrame();
}

Vec2 Window::scale(Vec2 pt) {
    return pt * drawable() / size();
}

Vec2 Window::size() {
    i32 w, h;
    SDL_GetWindowSize(window, &w, &h);
    return Vec2((f32)w, (f32)h);
}

Vec2 Window::drawable() {
    i32 w, h;
    SDL_GL_GetDrawableSize(window, &w, &h);
    return Vec2((f32)w, (f32)h);
}

void Window::grab_mouse() {
    SDL_SetWindowGrab(window, SDL_TRUE);
}

void Window::ungrab_mouse() {
    SDL_SetWindowGrab(window, SDL_FALSE);
}

Vec2 Window::get_mouse() {
    i32 x, y;
    SDL_GetMouseState(&x, &y);
    return Vec2((f32)x, (f32)y);
}

void Window::capture_mouse() {
    SDL_CaptureMouse(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

void Window::release_mouse() {
    SDL_CaptureMouse(SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

void Window::set_mouse(Vec2 pos) {
    SDL_WarpMouseInWindow(window, (i32)pos.x, (i32)pos.y);
}
