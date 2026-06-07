#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "pt2d/Color.h"
#include "pt2d/Integrator.h"
#include "pt2d/RISDirection.h"
#include "pt2d/SceneSerializer.h"

namespace pt2d {

enum class ProductionStatus {
    Idle,
    Running,
    Paused,
    Stopping,
    Completed,
    Error
};

class ProductionApp {
public:
    explicit ProductionApp(std::string initial_scene_path = {});
    ~ProductionApp();

    bool init();
    void run();

private:
    struct RenderJobSnapshot {
        Scene scene;
        SceneDocumentSettings settings;
        std::string scene_json_text;
        std::string scene_path;
        std::string output_png_path;
        std::string renderstate_path;
        int target_spp = 1000;
        int thread_count = 0;
        bool resume_existing_state = false;
    };

    void shutdown();
    void draw_fixed_layout();
    void draw_header(const ImVec2& size);
    void draw_preview_panel(const ImVec2& size);
    void draw_control_panel(const ImVec2& size);
    void draw_progress_panel(const ImVec2& size);

    void start_render(bool resume_existing_state);
    void request_pause();
    void request_resume();
    void request_stop();
    void render_worker(RenderJobSnapshot job);

    bool load_scene_from_path(const std::string& path);
    bool load_renderstate_from_path(const std::string& path);
    bool save_renderstate_to_path(const std::string& path, const RenderJobSnapshot& job);
    bool save_current_png_to_path(const std::string& path);

    void initialize_render_buffers(const SceneDocumentSettings& settings);
    void ensure_ris_directions(const IntegratorSettings& settings);
    uint64_t seed_for_pixel(int x, int y, int sample_index, const IntegratorSettings& settings) const;
    Vec2 pixel_center_to_world(int x, int y) const;
    void update_preview_from_accum_locked(bool update_convergence);
    void upload_preview_texture_if_needed();
    void update_progress_estimate();
    float scene_bounds_aspect() const;
    int locked_height_for_width(int width) const;
    int locked_width_for_height(int height) const;
    void apply_aspect_lock_to_settings(SceneDocumentSettings& settings, bool preserve_width) const;
    void update_log_path_from_output();
    void write_log(const std::string& message);

    ProductionStatus status() const;
    void set_status(ProductionStatus status);
    const char* status_label() const;
    ImVec4 status_color() const;

    GLFWwindow* m_window = nullptr;
    bool m_running = true;

    Scene m_scene;
    SceneDocumentSettings m_doc_settings;
    std::string m_scene_json_text;

    std::string m_scene_path;
    std::string m_output_png_path = "production_output.png";
    std::string m_renderstate_path = "production_output.renderstate";
    std::string m_log_path = "production_output.log";
    std::string m_status_message;
    std::string m_last_event = "Ready";

    int m_target_spp = 1000;
    int m_thread_count = 0; // 0 = auto
    int m_current_spp = 0;
    bool m_lock_aspect_to_scene_bounds = true;

    int m_width = 1;
    int m_height = 1;
    Vec2 m_bounds_min = {-3.2f, -2.25f};
    Vec2 m_bounds_max = { 3.2f,  2.25f};
    std::vector<Color> m_accum;
    std::vector<Color> m_last_convergence_average;
    std::vector<RISDirection> m_ris_directions;

    std::vector<unsigned char> m_preview_rgba;
    std::vector<float> m_convergence_values;
    unsigned int m_preview_texture_id = 0;
    bool m_preview_dirty = false;

    mutable std::mutex m_data_mutex;
    mutable std::mutex m_log_mutex;
    std::thread m_worker;
    std::atomic<int> m_status{static_cast<int>(ProductionStatus::Idle)};
    std::atomic<bool> m_pause_requested{false};
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_checkpoint_requested{false};
    std::atomic<int> m_atomic_current_spp{0};
    std::atomic<int> m_atomic_target_spp{1000};

    std::chrono::steady_clock::time_point m_render_start_time{};
    std::chrono::steady_clock::time_point m_last_progress_time{};
    int m_last_estimate_spp = 0;
    int m_estimate_start_spp = 0;
    double m_elapsed_sec = 0.0;
    double m_eta_sec = 0.0;
    double m_frames_per_sec = 0.0;
};

} // namespace pt2d
