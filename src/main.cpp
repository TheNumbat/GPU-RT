
#include "gpurt.h"
#include "platform/window.h"
#include <sf_libs/CLI11.hpp>

int main(int argc, char** argv) {

    std::string scene_file;
    CLI::App args{"GPURT"};
    args.add_option("-s,--scene", scene_file, "Scene file to load");

    CLI11_PARSE(args, argc, argv);

    Window window;
    GPURT gpurt(window, scene_file);
    gpurt.loop();
    return 0;
}
