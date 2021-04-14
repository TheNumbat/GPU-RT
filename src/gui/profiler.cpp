
#include "profiler.h"
#include <algorithm>

#undef max
#undef min

static const Profile_Color color_order[] = {
    Profile_Color::alizarin,   Profile_Color::greenSea,  Profile_Color::pumpkin,
    Profile_Color::silver,     Profile_Color::turqoise,  Profile_Color::belizeHole,
    Profile_Color::nephritis,  Profile_Color::clouds,    Profile_Color::pomegranate,
    Profile_Color::peterRiver, Profile_Color::sunFlower, Profile_Color::amethyst,
    Profile_Color::carrot,     Profile_Color::wisteria,  Profile_Color::orange,
    Profile_Color::emerald};
static const u32 num_colors = sizeof(color_order) / sizeof(color_order[0]);

Profiler_Graph::Profiler_Graph(u32 framesCount) {
    frames.extend(framesCount);
}

void Profiler_Graph::load_frame_data(Graph_Entry* tasks, u32 count) {

    auto& currFrame = frames[currFrameIndex];
    currFrame.tasks.clear();
    currFrame.taskStatsIndex.clear();

    for(u32 taskIndex = 0; taskIndex < count; taskIndex++) {
        if(tasks[taskIndex].color == Profile_Color::none)
            tasks[taskIndex].color = color_order[hash(tasks[taskIndex].name) % num_colors];

        if(taskIndex == 0)
            currFrame.tasks.push(tasks[taskIndex]);
        else {
            if(tasks[taskIndex - 1].color != tasks[taskIndex].color ||
               tasks[taskIndex - 1].name != tasks[taskIndex].name)
                currFrame.tasks.push(tasks[taskIndex]);
            else
                currFrame.tasks.back().endTime = tasks[taskIndex].endTime;
        }
    }
    currFrame.taskStatsIndex.extend(currFrame.tasks.size());

    for(u32 taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++) {

        auto& task = currFrame.tasks[taskIndex];
        Maybe_Ref<u32> it = taskNameToStatsIndex.try_get(task.name);

        if(!it) {
            taskNameToStatsIndex.insert(task.name, taskStats.size());
            Task_Stats taskStat;
            taskStat.maxTime = -1.0;
            taskStats.push(std::move(taskStat));
        }
        currFrame.taskStatsIndex[taskIndex] = taskNameToStatsIndex.get(task.name);
    }

    currFrameIndex = (currFrameIndex + 1) % frames.size();
    rebuild_task_stats(currFrameIndex, 300);
}

void Profiler_Graph::render_timings(i32 graphWidth, i32 legendWidth, i32 height,
                                    i32 frameIndexOffset) {

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const Vec2 widgetPos(p.x, p.y);
    render_graph(drawList, widgetPos, Vec2((f32)graphWidth, (f32)height), frameIndexOffset);
    render_legend(drawList, widgetPos + Vec2((f32)graphWidth, 0.0f),
                  Vec2((f32)legendWidth, (f32)height), frameIndexOffset);
    ImGui::Dummy(ImVec2(f32(graphWidth + legendWidth), f32(height)));
}

void Profiler_Graph::rebuild_task_stats(u32 endFrame, u32 framesCount) {

    for(auto& taskStat : taskStats) {
        taskStat.maxTime = -1.0f;
        taskStat.priorityOrder = u32(-1);
        taskStat.onScreenIndex = u32(-1);
    }

    for(u32 frameNumber = 0; frameNumber < framesCount; frameNumber++) {

        u32 frameIndex = (endFrame - 1 - frameNumber + frames.size()) % frames.size();
        auto& frame = frames[frameIndex];

        for(u32 taskIndex = 0; taskIndex < frame.tasks.size(); taskIndex++) {
            auto& task = frame.tasks[taskIndex];
            auto& stats = taskStats[frame.taskStatsIndex[taskIndex]];
            stats.maxTime = std::max(stats.maxTime, task.endTime - task.startTime);
        }
    }

    Vec<u32> statPriorities;
    statPriorities.extend(taskStats.size());

    for(u32 statIndex = 0; statIndex < taskStats.size(); statIndex++)
        statPriorities[statIndex] = statIndex;

    std::sort(statPriorities.begin(), statPriorities.end(), [this](u32 left, u32 right) {
        return taskStats[left].maxTime > taskStats[right].maxTime;
    });

    for(u32 statNumber = 0; statNumber < taskStats.size(); statNumber++) {
        u32 statIndex = statPriorities[statNumber];
        taskStats[statIndex].priorityOrder = statNumber;
    }
}

void Profiler_Graph::render_graph(ImDrawList* drawList, Vec2 graphPos, Vec2 graphSize,
                                  u32 frameIndexOffset) {

    rect(drawList, graphPos, graphPos + graphSize, 0xffffffff, false);
    f32 maxFrameTime = 1000.0f / 30.0f;
    f32 heightThreshold = 1.0f;

    for(u32 frameNumber = 0; frameNumber < frames.size(); frameNumber++) {

        u32 frameIndex = (currFrameIndex - frameIndexOffset - 1 - frameNumber + 2 * frames.size()) %
                         frames.size();

        Vec2 framePos = graphPos + Vec2(graphSize.x - 1 - frameWidth -
                                            (frameWidth + frameSpacing) * frameNumber,
                                        graphSize.y - 1);
        if(framePos.x < graphPos.x + 1) break;

        Vec2 taskPos = framePos + Vec2(0.0f, 0.0f);
        auto& frame = frames[frameIndex];

        for(auto task : frame.tasks) {
            f32 taskStartHeight = (f32(task.startTime) / maxFrameTime) * graphSize.y;
            f32 taskEndHeight = (f32(task.endTime) / maxFrameTime) * graphSize.y;
            if(abs(taskEndHeight - taskStartHeight) > heightThreshold)
                rect(drawList, taskPos + Vec2(0.0f, (f32)-taskStartHeight),
                     taskPos + Vec2((f32)frameWidth, (f32)-taskEndHeight), (u32)task.color, true);
        }
    }
}

void Profiler_Graph::render_legend(ImDrawList* drawList, Vec2 legendPos, Vec2 legendSize,
                                   u32 frameIndexOffset) {

    f32 markerLeftRectMargin = 3.0f;
    f32 markerLeftRectWidth = 5.0f;
    f32 maxFrameTime = 1000.0f / 30.0f;
    f32 markerMidWidth = 20.0f;
    f32 markerRightRectWidth = 5.0f;
    f32 markerRigthRectMargin = 3.0f;
    f32 markerRightRectHeight = 10.0f;
    f32 markerRightRectSpacing = 4.0f;
    f32 nameOffset = 40.0f;
    Vec2 textMargin = Vec2(5.0f, -3.0f);

    auto& currFrame =
        frames[(currFrameIndex - frameIndexOffset - 1 + 2 * frames.size()) % frames.size()];
    u32 maxTasksCount = u32(legendSize.y / (markerRightRectHeight + markerRightRectSpacing));

    for(auto& taskStat : taskStats) taskStat.onScreenIndex = u32(-1);

    u32 tasksToShow = std::min<u32>(taskStats.size(), maxTasksCount);
    u32 tasksShownCount = 0;
    for(u32 taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++) {

        auto& task = currFrame.tasks[taskIndex];
        auto& stat = taskStats[currFrame.taskStatsIndex[taskIndex]];

        if(stat.priorityOrder >= tasksToShow) continue;

        if(stat.onScreenIndex == u32(-1))
            stat.onScreenIndex = tasksShownCount++;
        else
            continue;

        f32 taskStartHeight = (f32(task.startTime) / maxFrameTime) * legendSize.y;
        f32 taskEndHeight = (f32(task.endTime) / maxFrameTime) * legendSize.y;

        Vec2 markerLeftRectMin = legendPos + Vec2(markerLeftRectMargin, legendSize.y);
        Vec2 markerLeftRectMax = markerLeftRectMin + Vec2(markerLeftRectWidth, 0.0f);
        markerLeftRectMin.y -= taskStartHeight;
        markerLeftRectMax.y -= taskEndHeight;

        Vec2 markerRightRectMin =
            legendPos +
            Vec2(markerLeftRectMargin + markerLeftRectWidth + markerMidWidth,
                 legendSize.y - markerRigthRectMargin -
                     (markerRightRectHeight + markerRightRectSpacing) * stat.onScreenIndex);
        Vec2 markerRightRectMax =
            markerRightRectMin + Vec2(markerRightRectWidth, -markerRightRectHeight);
        render_task_marker(drawList, markerLeftRectMin, markerLeftRectMax, markerRightRectMin,
                           markerRightRectMax, (u32)task.color);

        u32 textColor = (u32)(useColoredLegendText ? task.color : Profile_Color::text);

        f32 taskTimeMs = f32(task.endTime - task.startTime);

        literal label = scratch_format("[% ", taskTimeMs);
        label.cut(6);

        text(drawList, markerRightRectMax + textMargin, textColor, label.str());
        text(drawList, markerRightRectMax + textMargin + Vec2(nameOffset, 0.0f), textColor,
             scratch_format("ms] %", task.name).str());
    }
}

void Profiler_Graph::rect(ImDrawList* drawList, Vec2 minPoint, Vec2 maxPoint, u32 col,
                          bool filled) {

    if(filled)
        drawList->AddRectFilled(ImVec2(minPoint.x, minPoint.y), ImVec2(maxPoint.x, maxPoint.y),
                                col);
    else
        drawList->AddRect(ImVec2(minPoint.x, minPoint.y), ImVec2(maxPoint.x, maxPoint.y), col);
}

void Profiler_Graph::text(ImDrawList* drawList, Vec2 point, u32 col, const char* text) {
    drawList->AddText(ImVec2(point.x, point.y), col, text);
}

void Profiler_Graph::triangle(ImDrawList* drawList, Vec2 points[3], u32 col, bool filled) {
    if(filled)
        drawList->AddTriangleFilled(ImVec2(points[0].x, points[0].y),
                                    ImVec2(points[1].x, points[1].y),
                                    ImVec2(points[2].x, points[2].y), col);
    else
        drawList->AddTriangle(ImVec2(points[0].x, points[0].y), ImVec2(points[1].x, points[1].y),
                              ImVec2(points[2].x, points[2].y), col);
}

void Profiler_Graph::render_task_marker(ImDrawList* drawList, Vec2 leftMinPoint, Vec2 leftMaxPoint,
                                        Vec2 rightMinPoint, Vec2 rightMaxPoint, u32 col) {

    rect(drawList, leftMinPoint, leftMaxPoint, col, true);
    rect(drawList, rightMinPoint, rightMaxPoint, col, true);

    ImVec2 points[] = {
        ImVec2(leftMaxPoint.x, leftMinPoint.y), ImVec2(leftMaxPoint.x, leftMaxPoint.y),
        ImVec2(rightMinPoint.x, rightMaxPoint.y), ImVec2(rightMinPoint.x, rightMinPoint.y)};
    drawList->AddConvexPolyFilled(points, 4, col);
}

Profiler_Window::Profiler_Window() : cpuGraph(300) {
    prevFpsFrameTime = std::chrono::system_clock::now();
}

void Profiler_Window::render() {

    fpsFramesCount++;
    auto currFrameTime = std::chrono::system_clock::now();
    {
        f32 fpsDeltaTime = std::chrono::duration<f32>(currFrameTime - prevFpsFrameTime).count();
        if(fpsDeltaTime > 0.5f) {
            this->avgFrameTime = fpsDeltaTime / f32(fpsFramesCount);
            fpsFramesCount = 0;
            prevFpsFrameTime = currFrameTime;
        }
    }

    ImGui::SetNextWindowSize({400.0f, 250.0f}, ImGuiCond_Once);
    ImGui::Begin(scratch_format("Profiler [% fps % ms]###ProfileGraph", 1.0f / avgFrameTime,
                                avgFrameTime * 1000.0f)
                     .str(),
                 0, ImGuiWindowFlags_NoScrollbar);
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    i32 sizeMargin = int(ImGui::GetStyle().ItemSpacing.y);
    i32 maxGraphHeight = 300;
    i32 availableGraphHeight = (int(canvasSize.y) - sizeMargin);
    i32 graphHeight = std::min(maxGraphHeight, availableGraphHeight);
    i32 graphWidth = int(canvasSize.x) - legendWidth;
    cpuGraph.render_timings(graphWidth, legendWidth, graphHeight, frameOffset);
    if(graphHeight + sizeMargin + sizeMargin < canvasSize.y) {
        ImGui::Columns(2);
        ImGui::Checkbox("Stop profiling", &stopProfiling);
        ImGui::SameLine();
        ImGui::Checkbox("Colored legend text", &useColoredLegendText);
        ImGui::DragInt("Frame offset", &frameOffset, 1.0f, 0, 400);
        ImGui::DragInt("Legend width", &legendWidth, 1.0f, 50, 500);
        ImGui::NextColumn();

        ImGui::SliderInt("Frame width", &frameWidth, 1, 4);
        ImGui::SliderInt("Frame spacing", &frameSpacing, 0, 2);
        ImGui::SliderFloat("Transparency", &ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w, 0.0f,
                           1.0f);
        ImGui::Columns(1);
    }

    if(!stopProfiling) frameOffset = 0;

    cpuGraph.frameWidth = frameWidth;
    cpuGraph.frameSpacing = frameSpacing;
    cpuGraph.useColoredLegendText = useColoredLegendText;
    ImGui::End();
}
