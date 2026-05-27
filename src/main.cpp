#include "pt2d/App.h"

int main() {
    pt2d::App app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    return 0;
}
