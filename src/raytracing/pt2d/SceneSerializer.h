#pragma once

#include <cstdint>
#include <string>

#include "pt2d/Integrator.h"
#include "pt2d/Math.h"
#include "pt2d/Scene.h"

namespace pt2d {

struct SceneDocumentSettings {
    IntegratorSettings integrator;
    Vec2 field_bounds_min = {-3.2f, -2.25f};
    Vec2 field_bounds_max = { 3.2f,  2.25f};
    int field_width = 320;
    int field_height = 224;
    int samples_per_frame = 1;
    int stop_after_samples = 0;
};

bool save_scene_json(
    const std::string& path,
    const Scene& scene,
    const SceneDocumentSettings& settings,
    std::string* error_message = nullptr);

bool load_scene_json(
    const std::string& path,
    Scene& scene,
    SceneDocumentSettings& settings,
    std::string* error_message = nullptr);

} // namespace pt2d
