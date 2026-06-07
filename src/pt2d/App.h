#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "pt2d/DebugTrace.h"
#include "pt2d/Integrator.h"
#include "pt2d/Scene.h"
#include "pt2d/RISDirection.h"

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
    RISDirection* ris_direction_for_pixel(int x, int y, const IntegratorSettings& settings);
    const RISDirection* ris_direction_for_pixel(int x, int y) const;

    unsigned int texture_id() const { return m_texture_id; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    int samples() const { return m_samples; }
    int last_photon_count() const { return m_last_photon_count; }
    FieldBounds bounds() const { return m_bounds; }

private:
    void update_rgba_buffer();
    void ensure_ris_directions(const IntegratorSettings& settings);

    int m_width = 320;
    int m_height = 224;
    int m_samples = 0;
    int m_last_photon_count = 0;
    unsigned int m_texture_id = 0;
    FieldBounds m_bounds;
    std::vector<Color> m_accum;
    std::vector<unsigned char> m_rgba;
    
    std::vector<RISDirection> m_ris_direction;
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
    void draw_reservoir_windows();
    void draw_reservoir_polar_plot(const RISDirection& ris_direction, ImVec2 size);
    void draw_debug_sample_controls();
    int debug_sample_max() const;
    void clamp_debug_sample_index();
    void retrace_debug_sample();
    void reset_accumulation();
    void save_scene();
    void load_scene();

    ImVec2 world_to_screen(Vec2 p) const;
    Vec2 screen_to_world(ImVec2 p) const;
    float distance_to_segment(Vec2 p, Vec2 a, Vec2 b) const;
    void select_scene_handle(Vec2 world, float radius_world);
    void mark_scene_edited();
    bool draw_material_editor(Material& material);
    Material default_wall_material() const;
    Material default_light_material() const;
    Material default_glass_material() const;
    void clear_selection();

    GLFWwindow* m_window = nullptr;
    Scene m_scene;
    IntegratorSettings m_settings;
    FieldAccumulator m_field;
    DebugRecorder m_debug_recorder;

    struct ReservoirDebugWindow {
        int id = 0;
        bool open = true;
        int pixel_x = 0;
        int pixel_y = 0;
        Vec2 world_position = {0.0f, 0.0f};
        ImVec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    };

    bool m_running = true;
    bool m_render_paused = false;
    bool m_show_field_in_canvas = true;
    bool m_show_debug_rays = true;
    bool m_show_debug_hits = true;
    bool m_show_debug_labels = true;
    bool m_show_normals = true;
    bool m_show_reservoir_debug = false;
    int m_debug_ray_display_max_depth = 3;
    int m_samples_per_frame = 1;
    int m_stop_after_samples = 0; // 0 = infinite
    int m_field_width = 320;
    int m_field_height = 224;
    FieldBounds m_field_bounds;

    int m_selected_pixel_x = 160;
    int m_selected_pixel_y = 112;
    int m_debug_sample_index = 0;
    bool m_debug_uses_selected_pixel = true;
    Vec2 m_debug_world_position = {0.0f, 0.0f};
    std::vector<ReservoirDebugWindow> m_reservoir_windows;
    int m_next_reservoir_window_id = 1;

    SelectedObjectKind m_selected_kind = SelectedObjectKind::None;
    int m_selected_index = -1;
    int m_selected_handle = -1; // segment: 0 A, 1 B, 2 whole. circle: 0 center, 1 radius, 2 whole
    bool m_dragging_scene_handle = false;

    Vec2 m_view_center = {0.0f, 0.0f};
    float m_view_scale = 110.0f;
    ImVec2 m_canvas_origin = {0.0f, 0.0f};
    ImVec2 m_canvas_size = {1.0f, 1.0f};

    std::string m_save_png_path = "field.png";
    std::string m_scene_json_path = "scene.json";
    std::string m_save_status;
    std::string m_scene_status;
};

} // namespace pt2d
