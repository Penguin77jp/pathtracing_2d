#include "pt2d/App.h"
#include "pt2d/ProductionApp.h"

#include <iostream>
#include <string>

namespace {

bool is_arg(const char* arg, const char* name) {
    return arg != nullptr && std::string(arg) == name;
}

} // namespace

int main(int argc, char** argv) {
    bool production_mode = false;
    std::string production_scene_path;

    // No arguments is the same as --interactive.
    for (int i = 1; i < argc; ++i) {
        if (is_arg(argv[i], "--interactive")) {
            production_mode = false;
        } else if (is_arg(argv[i], "--production")) {
            production_mode = true;
            if (i + 1 < argc && argv[i + 1] != nullptr && argv[i + 1][0] != '-') {
                production_scene_path = argv[++i];
            }
        } else {
            std::cerr << "Unknown argument: " << argv[i] << "\n";
            std::cerr << "Usage: pathtracing_2d [--interactive] [--production [scene.json]]\n";
            return 1;
        }
    }

    if (production_mode) {
        pt2d::ProductionApp app(production_scene_path);
        if (!app.init()) {
            return 1;
        }
        app.run();
        return 0;
    }

    pt2d::App app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    return 0;
}
