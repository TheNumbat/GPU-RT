
#include "window.h"
#include "font.dat"

#include "imgui_impl_sdl.h"
#include <imgui/imgui.h>

#ifdef _WIN32
#include <ShellScalingApi.h>
#include <windows.h>
extern "C" {
__declspec(dllexport) bool NvOptimusEnablement = true;
__declspec(dllexport) bool AmdPowerXpressRequestHighPerformance = true;
}
#endif

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

    window =
        SDL_CreateWindow("FCPW-GPU", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_VULKAN);
    if(!window) {
        die("Failed to create window: %s", SDL_GetError());
    }

    keybuf = SDL_GetKeyboardState(nullptr);

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(window);

    set_dpi();
    VK::vk().init(window);
    ImGui::StyleColorsDark();
}

void Window::set_dpi() {

    float dpi;
    int index = SDL_GetWindowDisplayIndex(window);
    if(index < 0) {
        return;
    }
    if(SDL_GetDisplayDPI(index, nullptr, &dpi, nullptr)) {
        return;
    }
    float scale = drawable().x / size().x;
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
    VK::vk().destroy();

    ImGui::DestroyContext();
    SDL_DestroyWindow(window);
    window = nullptr;
    SDL_Quit();
}

void Window::complete_frame() {
    VK::vk().end_frame();
}

std::optional<SDL_Event> Window::event() {

    SDL_Event e;
    if(SDL_PollEvent(&e)) {

        ImGui_ImplSDL2_ProcessEvent(&e);

        switch(e.type) {
        case SDL_WINDOWEVENT: {
            if(e.window.event == SDL_WINDOWEVENT_RESIZED ||
               e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {

                VK::vk().trigger_resize();
            }
        } break;
        }

        return {std::move(e)};
    }
    return std::nullopt;
}

void Window::begin_frame() {
    set_dpi();
    ImGuiIO& IO = ImGui::GetIO();
    IO.DisplayFramebufferScale = scale({1.0f, 1.0f});

    ImGui_ImplSDL2_NewFrame(window);
    VK::vk().begin_frame();
    ImGui::NewFrame();
}

Vec2 Window::scale(Vec2 pt) {
    return pt * drawable() / size();
}

Vec2 Window::size() {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    return Vec2((float)w, (float)h);
}

Vec2 Window::drawable() {
    int w, h;
    SDL_GL_GetDrawableSize(window, &w, &h);
    return Vec2((float)w, (float)h);
}

void Window::grab_mouse() {
    SDL_SetWindowGrab(window, SDL_TRUE);
}

void Window::ungrab_mouse() {
    SDL_SetWindowGrab(window, SDL_FALSE);
}

Vec2 Window::get_mouse() {
    int x, y;
    SDL_GetMouseState(&x, &y);
    return Vec2((float)x, (float)y);
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
    SDL_WarpMouseInWindow(window, (int)pos.x, (int)pos.y);
}
