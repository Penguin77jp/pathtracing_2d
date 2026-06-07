#include "pt2d/App.h"

#include <iostream>
#include <filesystem>

int main() {
    pt2d::App app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    return 0;
}
