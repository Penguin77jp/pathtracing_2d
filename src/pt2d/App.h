#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "pt2d/DebugTrace.h"
#include "pt2d/Integrator.h"
#include "pt2d/Scene.h"

namespace pt2d {

struct FieldBounds {
    Vec2 min = {-3.2f, -2.25f};
    Vec2 max = { 3.2f,  2.25f};
};

enum class SelectedObjectKind {
    None,
    Segment,
    Circle
};

class FieldAccumulator {
public:
    void reset(int width, int height, FieldBounds bounds);
    void accumulate(const Scene& scene, const IntegratorSettings& settings, int samples_per_frame);
    void upload_to_texture();
    bool save_png(const std::string& path);

    Vec2 pixel_center_to_world(int x, int y) const;
    Vec2 sample_position_for_pixel(int x, int y, int sample_index, const IntegratorSettings& settings) const;
    uint64_t seed_for_pixel(int x, int y, int sample_index, const IntegratorSettings& settings) const;
    bool world_to_pixel(Vec2 p, int& x, int& y) const;

    unsigned int texture_id() const { return texture_id_; }
    int width() const { return width_; }
    int height() const { return height_; }
    int samples() const { return samples_; }
    FieldBounds bounds() const { return bounds_; }

private:
    void update_rgba_buffer();

    int width_ = 320;
    int height_ = 224;
    int samples_ = 0;
    unsigned int texture_id_ = 0;
    FieldBounds bounds_;
    std::vector<Color> accum_;
    std::vector<unsigned char> rgba_;
};

class App {
public:
    App();
    ~App();

    bool init();
    void run();

private:
    void shutdown();
    void draw_main_menu();
    void draw_field_panel();
    void draw_scene_panel();
    void draw_scene_controls();
    void draw_canvas();
    void draw_debug_events();
    void retrace_debug_sample();
    void reset_accumulation();
    void save_scene();
    void load_scene();

    ImVec2 world_to_screen(Vec2 p) const;
    Vec2 screen_to_world(ImVec2 p) const;
    float distance_to_segment(Vec2 p, Vec2 a, Vec2 b) const;
    void select_scene_handle(Vec2 world, float radius_world);
    void mark_scene_edited();
    int ensure_material(const char* name, Material material);
    int default_wall_material() const;
    int default_light_material() const;
    int default_glass_material() const;
    void clear_selection();

    GLFWwindow* window_ = nullptr;
    Scene scene_;
    IntegratorSettings settings_;
    FieldAccumulator field_;
    DebugRecorder debug_recorder_;

    bool running_ = true;
    bool render_paused_ = false;
    bool show_field_in_canvas_ = true;
    bool show_debug_rays_ = true;
    bool show_debug_hits_ = true;
    bool show_debug_labels_ = true;
    bool show_normals_ = true;
    int samples_per_frame_ = 1;
    int stop_after_samples_ = 0; // 0 = infinite
    int field_width_ = 320;
    int field_height_ = 224;
    FieldBounds field_bounds_;

    int selected_pixel_x_ = 160;
    int selected_pixel_y_ = 112;
    int debug_sample_index_ = 0;
    bool debug_uses_selected_pixel_ = true;
    Vec2 debug_world_position_ = {0.0f, 0.0f};

    SelectedObjectKind selected_kind_ = SelectedObjectKind::None;
    int selected_index_ = -1;
    int selected_handle_ = -1; // segment: 0 A, 1 B, 2 whole. circle: 0 center, 1 radius, 2 whole

    Vec2 view_center_ = {0.0f, 0.0f};
    float view_scale_ = 110.0f;
    ImVec2 canvas_origin_ = {0.0f, 0.0f};
    ImVec2 canvas_size_ = {1.0f, 1.0f};

    std::string save_png_path_ = "field.png";
    std::string scene_json_path_ = "scene.json";
    std::string save_status_;
    std::string scene_status_;
};

} // namespace pt2d
