
#pragma once

// Code based on LegitProfiler by Alexander Sannikov
// https://github.com/Raikiri/LegitProfiler

#include <util/profile.h>
#include <imgui/imgui.h>
#include <lib/lib.h>

static constexpr char MProfGui_name[] = "MProfGui";
using MProfGuiA = Mallocator<MProfGui_name>;

#define RGBA_LE(col)                                                                               \
    (((col & 0xff000000) >> (3 * 8)) + ((col & 0x00ff0000) >> (1 * 8)) +                           \
     ((col & 0x0000ff00) << (1 * 8)) + ((col & 0x000000ff) << (3 * 8)))
enum class Profile_Color : u32 {
    none,
    turqoise = RGBA_LE(0x1abc9cffu),
    greenSea = RGBA_LE(0x16a085ffu),
    emerald = RGBA_LE(0x2ecc71ffu),
    nephritis = RGBA_LE(0x27ae60ffu),
    peterRiver = RGBA_LE(0x3498dbffu),
    belizeHole = RGBA_LE(0x2980b9ffu),
    amethyst = RGBA_LE(0x9b59b6ffu),
    wisteria = RGBA_LE(0x8e44adffu),
    sunFlower = RGBA_LE(0xf1c40fffu),
    orange = RGBA_LE(0xf39c12ffu),
    carrot = RGBA_LE(0xe67e22ffu),
    pumpkin = RGBA_LE(0xd35400ffu),
    alizarin = RGBA_LE(0xe74c3cffu),
    pomegranate = RGBA_LE(0xc0392bffu),
    clouds = RGBA_LE(0xecf0f1ffu),
    silver = RGBA_LE(0xbdc3c7ffu),
    text = RGBA_LE(0xF2F5FAFFu)
};
#undef RGBA_LE

struct Graph_Entry {
    f32 startTime = 0.0f, endTime = 0.0f;
    Profile_Color color = Profile_Color::none;
    literal name;

    f32 length() {
        return endTime - startTime;
    }
};

struct Profiler_Graph {

    i32 frameWidth = 3, frameSpacing = 1;
    bool useColoredLegendText = false;

    Profiler_Graph(u32 frames);
    ~Profiler_Graph() = default;

    void load_frame_data(Graph_Entry* tasks, u32 count);
    void render_timings(i32 graphWidth, i32 legendWidth, i32 height, i32 frameIndexOffset);

private:
    void rebuild_task_stats(u32 endFrame, u32 framesCount);
    void render_graph(ImDrawList* drawList, Vec2 graphPos, Vec2 graphSize, u32 frameIndexOffset);
    void render_legend(ImDrawList* drawList, Vec2 legendPos, Vec2 legendSize, u32 frameIndexOffset);
    static void rect(ImDrawList* drawList, Vec2 minPoint, Vec2 maxPoint, u32 col,
                     bool filled = true);
    static void text(ImDrawList* drawList, Vec2 point, u32 col, const char* text);
    static void triangle(ImDrawList* drawList, Vec2 points[3], u32 col, bool filled = true);
    static void render_task_marker(ImDrawList* drawList, Vec2 leftMinPoint, Vec2 leftMaxPoint,
                                   Vec2 rightMinPoint, Vec2 rightMaxPoint, u32 col);

    struct Frame_Data {
        Vec<Graph_Entry, MProfGuiA> tasks;
        Vec<u32, MProfGuiA> taskStatsIndex;
    };

    struct Task_Stats {
        f32 maxTime;
        u32 priorityOrder, onScreenIndex;
    };

    Vec<Task_Stats, MProfGuiA> taskStats;
    Vec<Frame_Data, MProfGuiA> frames;
    Map<literal, u32, MProfGuiA> taskNameToStatsIndex;

    u32 currFrameIndex = 0;
};

struct Profiler_Window {

    Profiler_Window();
    ~Profiler_Window() = default;

    void render();

    bool stopProfiling = false;
    Profiler_Graph cpuGraph;

private:
    f32 avgFrameTime = 1.0f;
    u32 fpsFramesCount = 0;
    bool useColoredLegendText = true;
    i32 frameOffset = 0, frameWidth = 3, frameSpacing = 1;
    i32 legendWidth = 200;

    using Time_Point = std::chrono::time_point<std::chrono::system_clock>;
    Time_Point prevFpsFrameTime;
};
