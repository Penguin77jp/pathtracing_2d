#pragma once

#include <string>

#include "math.h"

namespace pg {

enum class RenderMode {
    NormalPreview,
    PathTracerExercise
};

enum class ScenePreset {
    MaterialTest,
    FinalBookScene
};

struct CameraConfig {
    Vec3f lookfrom{};
    Vec3f lookat{};
    Vec3f vup{0.0f, 1.0f, 0.0f};
    float vertical_fov_degrees = 40.0f;
    float focus_distance = 1.0f;
    float defocus_angle_degrees = 0.0f;
};

struct RenderConfig {
    int image_width = 400;
    int image_height = 225;
    int samples_per_pixel = 64;
    int max_depth = 20;
    RenderMode mode = RenderMode::NormalPreview;
    ScenePreset scene = ScenePreset::MaterialTest;
    std::string output_path = "ray_tracing_in_one_weekend_simd.png";
    CameraConfig camera{};
    void print() const {
        std::cout << "RenderConfig:\n";
        std::cout << "  image_width: " << image_width << "\n";
        std::cout << "  image_height: " << image_height << "\n";
        std::cout << "  samples_per_pixel: " << samples_per_pixel << "\n";
        std::cout << "  max_depth: " << max_depth << "\n";
        std::cout << "  mode: "
            << (mode == RenderMode::NormalPreview ? "NormalPreview" : "PathTracerExercise")
            << "\n";
        std::cout << "  scene: "
            << (scene == ScenePreset::MaterialTest ? "MaterialTest" : "FinalBookScene")
            << "\n";
        std::cout << "  output_path: " << output_path << "\n";
        std::cout << "  camera:\n";
        std::cout << "    lookfrom: (" 
            << camera.lookfrom.x << ", "
            << camera.lookfrom.y << ", "
            << camera.lookfrom.z << ")\n";
        std::cout << "    lookat: (" 
            << camera.lookat.x << ", "
            << camera.lookat.y << ", "
            << camera.lookat.z << ")\n";
        std::cout << "    vup: (" 
            << camera.vup.x << ", "
            << camera.vup.y << ", "
            << camera.vup.z << ")\n";
        std::cout << "    vertical_fov_degrees: "
            << camera.vertical_fov_degrees
            << "\n";
        std::cout << "    focus_distance: "
            << camera.focus_distance
            << "\n";
        std::cout << "    defocus_angle_degrees: "
            << camera.defocus_angle_degrees
			<< "\n";
    }
};

inline RenderConfig material_test_config() {
    RenderConfig config;
    config.image_width = 400;
    config.image_height = 225;
    config.samples_per_pixel = 64;
    config.max_depth = 20;
    config.scene = ScenePreset::MaterialTest;
    config.camera.lookfrom = Vec3f(3.0f, 3.0f, 2.0f);
    config.camera.lookat = Vec3f(0.0f, 0.0f, -1.0f);
    config.camera.vertical_fov_degrees = 20.0f;
    config.camera.focus_distance = (config.camera.lookfrom - config.camera.lookat)
        .normalized()
        .dot(config.camera.lookfrom - config.camera.lookat);
    config.camera.defocus_angle_degrees = 0.0f;
    return config;
}

inline RenderConfig final_scene_config() {
    RenderConfig config;
    config.image_width = 640;
    config.image_height = 200;
    //config.image_width = 1200;
    //config.image_height = 675;
    config.samples_per_pixel = 8 * 25;
    config.max_depth = 8;
    config.scene = ScenePreset::FinalBookScene;
    config.camera.lookfrom = Vec3f(13.0f, 2.0f, 3.0f);
    config.camera.lookat = Vec3f(0.0f, 0.0f, 0.0f);
    config.camera.vertical_fov_degrees = 20.0f;
    config.camera.focus_distance = 10.0f;
    config.camera.defocus_angle_degrees = 0.6f;
    return config;
}

inline void apply_quick_settings(RenderConfig& config) {
    config.image_width = 200;
    config.image_height = 112;
    config.samples_per_pixel = 8;
    config.max_depth = 8;
}

} // namespace pg
