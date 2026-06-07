#include "pt2d/ProductionApp.h"

#include <string>

int main(int argc, char** argv) {
    std::string initial_scene_path;
    if (argc >= 2 && argv[1]) {
        initial_scene_path = argv[1];
    }

    pt2d::ProductionApp app(initial_scene_path);
    if (!app.init()) {
        return 1;
    }
    app.run();
    return 0;
}
