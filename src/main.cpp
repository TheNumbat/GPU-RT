
#include "gpurt.h"
#include "platform/window.h"
#include "util/profile.h"

int main(i32, char**) {

    Profiler::start_thread();

    Window window;
    GPURT gpurt(window);
    gpurt.loop();

    Profiler::end_thread();

    return 0;
}
