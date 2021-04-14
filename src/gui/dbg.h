
#pragma once

#include "profiler.h"
#include <lib/lib.h>

struct Dbg_Gui {

    Dbg_Gui() = default;
    ~Dbg_Gui() = default;

    void profiler();

private:
    Profiler_Window prof_window;
};
