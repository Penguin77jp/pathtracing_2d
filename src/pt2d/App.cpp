#include "pt2d/App.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <optional>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include "pt2d/PngWriter.h"
#include "pt2d/SceneSerializer.h"

namespace pt2d {

namespace {

ImU32 rgba(int r, int g, int b, int a = 255) {
    return IM_COL32(r, g, b, a);
}

float luminance(Color c) {
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

uint64_t integrator_kind_seed(IntegratorKind kind) {
    return static_cast<uint64_t>(static_cast<int>(kind) + 1) * 0x9e3779b97f4a7c15ULL;
}

uint64_t sample_seed_base(const IntegratorSettings& settings) {
    return hash_combine(settings.seed, integrator_kind_seed(settings.kind));
}

uint64_t field_sample_seed(const IntegratorSettings& settings, int x, int y, int sample_index) {
    uint64_t seed = sample_seed_base(settings);
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, x)));
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, y)));
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, sample_index)));
    return seed;
}

uint64_t world_sample_seed(const IntegratorSettings& settings, Vec2 position, int sample_index) {
    uint64_t seed = sample_seed_base(settings);
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, sample_index)));
    seed = hash_combine(seed, static_cast<uint64_t>((position.x + 1000.0f) * 1000.0f));
    seed = hash_combine(seed, static_cast<uint64_t>((position.y + 1000.0f) * 1000.0f));
    return seed;
}

ImU32 depth_color(int depth) {
    static constexpr std::array<ImU32, 8> colors = {
        IM_COL32(120, 200, 255, 255),
        IM_COL32(255, 170, 100, 255),
        IM_COL32(170, 255, 130, 255),
        IM_COL32(255, 120, 190, 255),
        IM_COL32(190, 170, 255, 255),
        IM_COL32(255, 230, 120, 255),
        IM_COL32(120, 255, 220, 255),
        IM_COL32(230, 230, 230, 255),
    };
    if (depth < 0) {
        return IM_COL32(255, 145, 90, 255);
    }
    return colors[static_cast<size_t>(depth) % colors.size()];
}

void draw_arrow(ImDrawList* draw_list, ImVec2 a, ImVec2 b, ImU32 color, float thickness = 1.5f) {
    draw_list->AddLine(a, b, color, thickness);
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) return;
    const float ux = dx / len;
    const float uy = dy / len;
    const float size = 8.0f;
    ImVec2 l{b.x - ux * size - uy * size * 0.45f, b.y - uy * size + ux * size * 0.45f};
    ImVec2 r{b.x - ux * size + uy * size * 0.45f, b.y - uy * size - ux * size * 0.45f};
    draw_list->AddLine(b, l, color, thickness);
    draw_list->AddLine(b, r, color, thickness);
}

Vec2 bounds_sample_position(FieldBounds bounds, int x, int y, int width, int height, float jx, float jy) {
    const float u = (static_cast<float>(x) + jx) / static_cast<float>(std::max(1, width));
    const float v = (static_cast<float>(y) + jy) / static_cast<float>(std::max(1, height));
    return {
        bounds.min.x + u * (bounds.max.x - bounds.min.x),
        bounds.max.y - v * (bounds.max.y - bounds.min.y),
    };
}

} // namespace

void FieldAccumulator::reset(int width, int height, FieldBounds bounds) {
    m_width = std::max(1, width);
    m_height = std::max(1, height);
    m_bounds = bounds;
    m_samples = 0;
    m_last_photon_count = 0;
    m_accum.assign(static_cast<size_t>(m_width * m_height), make_color(0.0f));
    m_rgba.assign(static_cast<size_t>(m_width * m_height * 4), 0);
    m_ris_direction.clear();

    if (m_texture_id == 0) {
        glGenTextures(1, &m_texture_id);
        glBindTexture(GL_TEXTURE_2D, m_texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    upload_to_texture();
}

uint64_t FieldAccumulator::seed_for_pixel(int x, int y, int sample_index, const IntegratorSettings& settings) const {
    return field_sample_seed(settings, x, y, sample_index);
}

Vec2 FieldAccumulator::pixel_center_to_world(int x, int y) const {
    return bounds_sample_position(m_bounds, x, y, m_width, m_height, 0.5f, 0.5f);
}

Vec2 FieldAccumulator::sample_position_for_pixel(int x, int y, int sample_index, const IntegratorSettings& settings) const {
    (void)sample_index;
    (void)settings;
    return pixel_center_to_world(x, y);
}

bool FieldAccumulator::world_to_pixel(Vec2 p, int& x, int& y) const {
    if (m_width <= 0 || m_height <= 0) {
        x = 0;
        y = 0;
        return false;
    }

    const float w = m_bounds.max.x - m_bounds.min.x;
    const float h = m_bounds.max.y - m_bounds.min.y;
    if (w <= 0.0f || h <= 0.0f) {
        x = 0;
        y = 0;
        return false;
    }

    const float u = (p.x - m_bounds.min.x) / w;
    const float v = (m_bounds.max.y - p.y) / h;
    const bool inside = u >= 0.0f && u < 1.0f && v >= 0.0f && v < 1.0f;

    x = std::clamp(static_cast<int>(u * static_cast<float>(m_width)), 0, m_width - 1);
    y = std::clamp(static_cast<int>(v * static_cast<float>(m_height)), 0, m_height - 1);
    return inside;
}

void FieldAccumulator::ensure_ris_directions(const IntegratorSettings& settings) {
    const size_t pixel_count = static_cast<size_t>(m_width * m_height);
    if (m_ris_direction.size() == pixel_count) {
        return;
    }

    m_ris_direction.clear();
    m_ris_direction.reserve(pixel_count);
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const uint64_t ris_seed = seed_for_pixel(x, y, 0, settings) ^ 0x7269736469726563ULL;
            m_ris_direction.emplace_back(
                settings.ris_direction.num_bins,
                settings.ris_direction.min_probability_percent,
                settings.ris_direction.smooth_sigma_deg,
                ris_seed);
        }
    }
}

RISDirection* FieldAccumulator::ris_direction_for_pixel(int x, int y, const IntegratorSettings& settings) {
    if (settings.kind != IntegratorKind::RISDirection) {
        return nullptr;
    }
    if (m_width <= 0 || m_height <= 0) {
        return nullptr;
    }

    ensure_ris_directions(settings);
    x = std::clamp(x, 0, m_width - 1);
    y = std::clamp(y, 0, m_height - 1);
    return &m_ris_direction[static_cast<size_t>(y * m_width + x)];
}

const RISDirection* FieldAccumulator::ris_direction_for_pixel(int x, int y) const {
    if (m_width <= 0 || m_height <= 0) {
        return nullptr;
    }
    if (m_ris_direction.size() != static_cast<size_t>(m_width * m_height)) {
        return nullptr;
    }

    x = std::clamp(x, 0, m_width - 1);
    y = std::clamp(y, 0, m_height - 1);
    return &m_ris_direction[static_cast<size_t>(y * m_width + x)];
}

void FieldAccumulator::accumulate(const Scene& scene, const IntegratorSettings& settings, int samples_per_frame) {
    if (m_accum.empty()) {
        reset(m_width, m_height, m_bounds);
    }

    const int spp = std::max(1, samples_per_frame);
    if (settings.kind != IntegratorKind::PhotonMapping) {
        m_last_photon_count = 0;
    }
    
    if (settings.kind == IntegratorKind::RISDirection) {
        ensure_ris_directions(settings);
    } else {
        m_ris_direction.clear();
    }

    for (int s = 0; s < spp; ++s) {
        const int sample_index = m_samples + s;

        PhotonMap photon_map;
        const PhotonMap* photon_map_ptr = nullptr;
        if (settings.kind == IntegratorKind::PhotonMapping) {
            const uint64_t photon_seed = hash_combine(sample_seed_base(settings), static_cast<uint64_t>(sample_index) ^ 0x70686f746f6eULL);
            photon_map = build_photon_map(scene, settings, photon_seed);
            m_last_photon_count = static_cast<int>(photon_map.photons.size());
            photon_map_ptr = &photon_map;
        }

        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                Sampler sampler(seed_for_pixel(x, y, sample_index, settings));
                const Vec2 position = pixel_center_to_world(x, y);
                RISDirection* ris_direction_ptr = nullptr;
                if (settings.kind == IntegratorKind::RISDirection) {
                    ris_direction_ptr = &m_ris_direction[static_cast<size_t>(y * m_width + x)];
                }
                m_accum[static_cast<size_t>(y * m_width + x)] += estimate_at(scene, position, sampler, settings, nullptr, photon_map_ptr, ris_direction_ptr);
            }
        }
    }
    m_samples += spp;
}

void FieldAccumulator::update_rgba_buffer() {
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            Color c = m_accum.empty() || m_samples == 0
                ? make_color(0.0f)
                : m_accum[static_cast<size_t>(y * m_width + x)] / static_cast<float>(m_samples);
            c = clamp(tonemap(c));
            const size_t i = static_cast<size_t>((y * m_width + x) * 4);
            m_rgba[i + 0] = to_srgb8(c.r);
            m_rgba[i + 1] = to_srgb8(c.g);
            m_rgba[i + 2] = to_srgb8(c.b);
            m_rgba[i + 3] = 255;
        }
    }
}

void FieldAccumulator::upload_to_texture() {
    if (m_texture_id == 0) {
        glGenTextures(1, &m_texture_id);
    }

    update_rgba_buffer();

    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_rgba.data());
}

bool FieldAccumulator::save_png(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    update_rgba_buffer();
    return write_png_rgba8(path.c_str(), m_width, m_height, m_rgba.data());
}

App::App() = default;

App::~App() {
    shutdown();
}

bool App::init() {
    m_scene = make_default_scene();
    m_settings.kind = IntegratorKind::PathTracing;
    m_settings.max_depth = 6;
    m_settings.seed = 1;

    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    m_window = glfwCreateWindow(1360, 880, "Pathtracing2D Field Debugger", nullptr, nullptr);
    if (!m_window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    m_field.reset(m_field_width, m_field_height, m_field_bounds);
    m_debug_world_position = m_field.pixel_center_to_world(m_selected_pixel_x, m_selected_pixel_y);
    retrace_debug_sample();
    return true;
}

void App::shutdown() {
    if (!m_window) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
    m_window = nullptr;
}

void App::run() {
    while (m_window && !glfwWindowShouldClose(m_window) && m_running) {
        glfwPollEvents();

        if (!m_render_paused) {
            int spp = std::max(1, m_samples_per_frame);
            if (m_stop_after_samples > 0) {
                const int remaining = m_stop_after_samples - m_field.samples();
                if (remaining <= 0) {
                    m_render_paused = true;
                    spp = 0;
                } else {
                    spp = std::min(spp, remaining);
                }
            }

            if (spp > 0) {
                m_field.accumulate(m_scene, m_settings, spp);
                m_field.upload_to_texture();
                if (m_stop_after_samples > 0 && m_field.samples() >= m_stop_after_samples) {
                    m_render_paused = true;
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_main_menu();
        draw_field_panel();
        draw_scene_panel();
        draw_reservoir_windows();

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.055f, 0.065f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }
}

void App::draw_main_menu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save PNG")) {
                const bool ok = m_field.save_png(m_save_png_path);
                m_save_status = ok ? ("Saved PNG: " + m_save_png_path) : ("Failed to save PNG: " + m_save_png_path);
            }
            if (ImGui::MenuItem("Save Scene")) {
                save_scene();
            }
            if (ImGui::MenuItem("Load Scene")) {
                load_scene();
            }
            if (ImGui::MenuItem("Exit")) {
                m_running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem("Retrace selected sample", "Space")) {
                retrace_debug_sample();
            }
            if (ImGui::MenuItem("Reset accumulation", "R")) {
                reset_accumulation();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        retrace_debug_sample();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
        reset_accumulation();
    }
}

void App::draw_field_panel() {
    ImGui::Begin("2D Luminance Field");

    int mode = static_cast<int>(m_settings.kind);
    const char* modes[] = {
        "Path tracing",
        "Path tracing + NEE",
        "RIS direction",
        "Photon mapping"
    };
    if (ImGui::Combo("Integrator", &mode, modes, IM_ARRAYSIZE(modes))) {
        m_settings.kind = static_cast<IntegratorKind>(mode);
        reset_accumulation();
        retrace_debug_sample();
    }

    bool changed = false;
    changed |= ImGui::SliderInt("Max depth", &m_settings.max_depth, 1, 16);
    changed |= ImGui::SliderInt("SPP / frame", &m_samples_per_frame, 1, 8);
    changed |= ImGui::InputInt("Auto stop samples (0=infinite)", &m_stop_after_samples);
    m_stop_after_samples = std::max(0, m_stop_after_samples);

    int seed_as_int = static_cast<int>(m_settings.seed);
    if (ImGui::InputInt("Seed", &seed_as_int)) {
        m_settings.seed = static_cast<uint64_t>(std::max(1, seed_as_int));
        changed = true;
    }

    if (m_settings.kind == IntegratorKind::RISDirection) {
        ImGui::Separator();
        ImGui::TextUnformatted("RIS direction");
        changed |= ImGui::SliderInt("RIS num bins", &m_settings.ris_direction.num_bins, 1, 256);
        changed |= ImGui::DragFloat("RIS exploration %", &m_settings.ris_direction.min_probability_percent, 0.5f, 0.0f, 100.0f, "%.1f");
        changed |= ImGui::SliderInt("RIS smooth sigma deg", &m_settings.ris_direction.smooth_sigma_deg, 0, 180);
        changed |= ImGui::SliderInt("RIS candidate count", &m_settings.ris_direction.candidate_count, 1, 128);
        m_settings.ris_direction.num_bins = std::clamp(m_settings.ris_direction.num_bins, 1, 256);
        m_settings.ris_direction.min_probability_percent = std::clamp(m_settings.ris_direction.min_probability_percent, 0.0f, 100.0f);
        m_settings.ris_direction.smooth_sigma_deg = std::clamp(m_settings.ris_direction.smooth_sigma_deg, 0, 180);
        m_settings.ris_direction.candidate_count = std::clamp(m_settings.ris_direction.candidate_count, 1, 128);
        ImGui::TextWrapped("RIS direction learns a per-pixel angular distribution. Changing these settings resets accumulation and rebuilds the RIS state.");
    }

    if (m_settings.kind == IntegratorKind::PhotonMapping) {
        ImGui::Separator();
        ImGui::TextUnformatted("Photon mapping");
        changed |= ImGui::InputInt("Photon count", &m_settings.photon_mapping.photon_count);
        m_settings.photon_mapping.photon_count = std::max(0, m_settings.photon_mapping.photon_count);
        changed |= ImGui::SliderInt("Photon max depth", &m_settings.photon_mapping.photon_max_depth, 1, 24);
        changed |= ImGui::DragFloat("Gather radius", &m_settings.photon_mapping.gather_radius, 0.005f, 0.005f, 2.0f, "%.3f");
        changed |= ImGui::DragFloat("Photon strength", &m_settings.photon_mapping.strength, 0.02f, 0.0f, 10.0f, "%.2f");
        changed |= ImGui::Checkbox("Caustics only", &m_settings.photon_mapping.caustics_only);
        ImGui::TextWrapped("Photon mapping pass: shoot photons from segment lights, keep photons that hit diffuse surfaces after glass/specular bounces, then add nearby photons at diffuse path hits.");
    }

    int new_width = m_field_width;
    int new_height = m_field_height;
    bool resolution_changed = false;
    resolution_changed |= ImGui::InputInt("Field width", &new_width);
    resolution_changed |= ImGui::InputInt("Field height", &new_height);
    new_width = std::max(1, new_width);
    new_height = std::max(1, new_height);

    bool bounds_changed = false;
    bounds_changed |= ImGui::DragFloat2("Bounds min", &m_field_bounds.min.x, 0.02f);
    bounds_changed |= ImGui::DragFloat2("Bounds max", &m_field_bounds.max.x, 0.02f);

    if (resolution_changed || bounds_changed) {
        m_field_width = new_width;
        m_field_height = new_height;
        reset_accumulation();
        retrace_debug_sample();
    } else if (changed) {
        reset_accumulation();
        retrace_debug_sample();
    }

    if (ImGui::Button(m_render_paused ? "Resume" : "Pause")) {
        m_render_paused = !m_render_paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset accumulation")) {
        reset_accumulation();
    }
    ImGui::SameLine();
    if (ImGui::Button("Retrace debug sample")) {
        retrace_debug_sample();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save PNG")) {
        const bool ok = m_field.save_png(m_save_png_path);
        m_save_status = ok ? ("Saved PNG: " + m_save_png_path) : ("Failed to save PNG: " + m_save_png_path);
    }

    char path_buffer[512];
    std::snprintf(path_buffer, sizeof(path_buffer), "%s", m_save_png_path.c_str());
    if (ImGui::InputText("PNG path", path_buffer, sizeof(path_buffer))) {
        m_save_png_path = path_buffer;
    }
    if (!m_save_status.empty()) {
        ImGui::TextWrapped("%s", m_save_status.c_str());
    }

    ImGui::Text("Accumulated samples: %d", m_field.samples());
    if (m_stop_after_samples > 0) {
        ImGui::Text("Auto stop target: %d", m_stop_after_samples);
    } else {
        ImGui::Text("Auto stop target: infinite");
    }
    ImGui::TextWrapped("Each pixel is a deterministic world-space probe at the exact pixel center. Rendering and debug retrace use the same world position and the same seed for each (pixel, sample index), so the selected pixel is exactly reproducible.");
    if (m_settings.kind == IntegratorKind::PathTracingNEE) {
        ImGui::TextWrapped("NEE debug: every diffuse hit also samples a light point. The canvas shows sampled light points, shadow segments, G_N, and per-vertex L_N / L_P.");
    } else if (m_settings.kind == IntegratorKind::PhotonMapping) {
        ImGui::Text("Stored photons in last render sample: %d", m_field.last_photon_count());
        ImGui::TextWrapped("Photon mapping debug: L_N includes direct NEE plus photon final gather. Photon hits are not drawn yet; use photon count/radius/strength to tune caustic visibility.");
    }

    ImGui::Separator();

    const float available_width = ImGui::GetContentRegionAvail().x;
    const float aspect = static_cast<float>(m_field.height()) / static_cast<float>(std::max(1, m_field.width()));
    ImVec2 image_size{std::max(240.0f, available_width), std::max(180.0f, available_width * aspect)};
    const ImVec2 image_pos = ImGui::GetCursorScreenPos();
    ImGui::Image(
        reinterpret_cast<ImTextureID>(static_cast<intptr_t>(m_field.texture_id())),
        image_size);

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImGuiIO& io = ImGui::GetIO();
        const float rel_x = std::clamp((io.MousePos.x - image_pos.x) / image_size.x, 0.0f, 0.9999f);
        const float rel_y = std::clamp((io.MousePos.y - image_pos.y) / image_size.y, 0.0f, 0.9999f);
        m_selected_pixel_x = std::clamp(static_cast<int>(rel_x * m_field.width()), 0, m_field.width() - 1);
        m_selected_pixel_y = std::clamp(static_cast<int>(rel_y * m_field.height()), 0, m_field.height() - 1);
        m_debug_uses_selected_pixel = true;
        retrace_debug_sample();
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (m_debug_uses_selected_pixel) {
        const float x0 = image_pos.x + image_size.x * (static_cast<float>(m_selected_pixel_x) / static_cast<float>(m_field.width()));
        const float y0 = image_pos.y + image_size.y * (static_cast<float>(m_selected_pixel_y) / static_cast<float>(m_field.height()));
        const float x1 = image_pos.x + image_size.x * (static_cast<float>(m_selected_pixel_x + 1) / static_cast<float>(m_field.width()));
        const float y1 = image_pos.y + image_size.y * (static_cast<float>(m_selected_pixel_y + 1) / static_cast<float>(m_field.height()));
        draw_list->AddRect(ImVec2{x0, y0}, ImVec2{x1, y1}, rgba(255, 130, 80), 0.0f, 0, 2.0f);
    }

    ImGui::Separator();
    ImGui::Text("Selected pixel: (%d, %d)", m_selected_pixel_x, m_selected_pixel_y);
    ImGui::Text("Debug position: %.3f, %.3f", m_debug_world_position.x, m_debug_world_position.y);

    const int max_sample = std::max(0, m_field.samples() - 1);
    m_debug_sample_index = std::clamp(m_debug_sample_index, 0, max_sample);
    if (ImGui::SliderInt("Debug sample index", &m_debug_sample_index, 0, std::max(0, max_sample))) {
        retrace_debug_sample();
    }
    if (ImGui::Button("Previous sample")) {
        m_debug_sample_index = std::max(0, m_debug_sample_index - 1);
        retrace_debug_sample();
    }
    ImGui::SameLine();
    if (ImGui::Button("Next sample")) {
        m_debug_sample_index += 1;
        retrace_debug_sample();
    }

    ImGui::End();
}

void App::draw_scene_panel() {
    ImGui::Begin("Scene Editor / Path Debug");

    draw_scene_controls();
    ImGui::Separator();

    ImGui::Checkbox("Field overlay", &m_show_field_in_canvas);
    ImGui::SameLine();
    ImGui::Checkbox("Rays", &m_show_debug_rays);
    ImGui::SameLine();
    ImGui::Checkbox("Hits", &m_show_debug_hits);
    ImGui::SameLine();
    ImGui::Checkbox("Normals", &m_show_normals);
    ImGui::SameLine();
    ImGui::Checkbox("Labels", &m_show_debug_labels);
    ImGui::SameLine();
    ImGui::Checkbox("Reservoir", &m_show_reservoir_debug);

    ImGui::TextWrapped("Image click: select a field pixel. Canvas click near an object: select/edit it. Canvas click empty space: debug exact world position. Drag selected handles to modify geometry. Right drag pans, mouse wheel zooms. Reservoir opens a RIS distribution window for clicked field pixels while RIS direction is active.");
    draw_canvas();

    ImGui::End();
}

void App::draw_scene_controls() {
    if (ImGui::Button("Default scene")) {
        m_scene = make_default_scene();
        clear_selection();
        mark_scene_edited();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset view")) {
        m_view_center = {0.0f, 0.0f};
        m_view_scale = 110.0f;
    }

    char scene_path_buffer[512];
    std::snprintf(scene_path_buffer, sizeof(scene_path_buffer), "%s", m_scene_json_path.c_str());
    if (ImGui::InputText("Scene JSON path", scene_path_buffer, sizeof(scene_path_buffer))) {
        m_scene_json_path = scene_path_buffer;
    }
    if (ImGui::Button("Save scene")) {
        save_scene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load scene")) {
        load_scene();
    }
    if (!m_scene_status.empty()) {
        ImGui::TextWrapped("%s", m_scene_status.c_str());
    }

    if (ImGui::Button("Add wall")) {
        const int mat = default_wall_material();
        const Vec2 a = m_view_center + Vec2{-0.7f, -0.25f};
        const Vec2 b = m_view_center + Vec2{ 0.7f,  0.25f};
        const Vec2 n = normalize(perpendicular(b - a));
        m_selected_kind = SelectedObjectKind::Segment;
        m_selected_index = m_scene.add_segment(a, b, n, mat);
        m_selected_handle = -1;
        mark_scene_edited();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add light")) {
        const int mat = default_light_material();
        const Vec2 a = m_view_center + Vec2{-0.55f, 0.65f};
        const Vec2 b = m_view_center + Vec2{ 0.55f, 0.65f};
        m_selected_kind = SelectedObjectKind::Segment;
        m_selected_index = m_scene.add_segment(a, b, {0.0f, -1.0f}, mat);
        m_selected_handle = -1;
        mark_scene_edited();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add glass circle")) {
        const int mat = default_glass_material();
        m_selected_kind = SelectedObjectKind::Circle;
        m_selected_index = m_scene.add_circle(m_view_center, 0.45f, mat);
        m_selected_handle = -1;
        mark_scene_edited();
    }

    if (m_selected_kind == SelectedObjectKind::Segment && m_selected_index >= 0 && m_selected_index < static_cast<int>(m_scene.segments.size())) {
        Segment& segment = m_scene.segments[m_selected_index];
        ImGui::Separator();
        ImGui::Text("Selected segment: %d", m_selected_index);

        if (ImGui::Button("Delete selected")) {
            m_scene.erase_segment(m_selected_index);
            clear_selection();
            mark_scene_edited();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Flip normal")) {
            segment.normal = -segment.normal;
            mark_scene_edited();
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            Segment copy = segment;
            copy.a += Vec2{0.15f, 0.15f};
            copy.b += Vec2{0.15f, 0.15f};
            m_selected_index = m_scene.add_segment(copy.a, copy.b, copy.normal, copy.material_id);
            mark_scene_edited();
            return;
        }

        bool edited = false;
        edited |= ImGui::DragFloat2("A", &segment.a.x, 0.02f);
        edited |= ImGui::DragFloat2("B", &segment.b.x, 0.02f);

        int material_id = segment.material_id;
        if (ImGui::BeginCombo("Material", m_scene.materials[material_id].name.c_str())) {
            for (int i = 0; i < static_cast<int>(m_scene.materials.size()); ++i) {
                const bool selected = i == material_id;
                if (ImGui::Selectable(m_scene.materials[i].name.c_str(), selected)) {
                    segment.material_id = i;
                    edited = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        Material& material = m_scene.materials[segment.material_id];
        float albedo[3] = {material.albedo.r, material.albedo.g, material.albedo.b};
        if (ImGui::ColorEdit3("Material albedo", albedo)) {
            material.albedo = {albedo[0], albedo[1], albedo[2]};
            edited = true;
        }
        float emission[3] = {material.emission.r, material.emission.g, material.emission.b};
        if (ImGui::DragFloat3("Material emission", emission, 0.05f, 0.0f, 50.0f)) {
            material.emission = {emission[0], emission[1], emission[2]};
            edited = true;
        }
        if (material.is_light()) {
            float angle = material.emission_angle_deg;
            if (ImGui::InputFloat("Emission angle deg (0-360)", &angle, 1.0f, 15.0f, "%.1f")) {
                material.emission_angle_deg = std::max(0.0f, std::min(360.0f, angle));
                edited = true;
            }
            ImGui::TextWrapped("Angle is centered around the segment normal. 180 deg matches the original one-sided light; 360 deg emits to both sides.");
        }

        if (material.is_dielectric()) {
            if (ImGui::DragFloat("IOR", &material.ior, 0.01f, 1.0f, 3.0f)) {
                edited = true;
            }
        }

        if (edited) {
            m_scene.refresh_segment_normal_keep_side(m_selected_index);
            mark_scene_edited();
        }
        return;
    }

    if (m_selected_kind == SelectedObjectKind::Circle && m_selected_index >= 0 && m_selected_index < static_cast<int>(m_scene.circles.size())) {
        Circle& circle = m_scene.circles[m_selected_index];
        ImGui::Separator();
        ImGui::Text("Selected circle: %d", m_selected_index);

        if (ImGui::Button("Delete selected")) {
            m_scene.erase_circle(m_selected_index);
            clear_selection();
            mark_scene_edited();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            Circle copy = circle;
            copy.center += Vec2{0.2f, 0.15f};
            m_selected_index = m_scene.add_circle(copy.center, copy.radius, copy.material_id);
            m_selected_kind = SelectedObjectKind::Circle;
            mark_scene_edited();
            return;
        }

        bool edited = false;
        edited |= ImGui::DragFloat2("Center", &circle.center.x, 0.02f);
        edited |= ImGui::DragFloat("Radius", &circle.radius, 0.01f, 0.02f, 5.0f);

        int material_id = circle.material_id;
        if (ImGui::BeginCombo("Material", m_scene.materials[material_id].name.c_str())) {
            for (int i = 0; i < static_cast<int>(m_scene.materials.size()); ++i) {
                const bool selected = i == material_id;
                if (ImGui::Selectable(m_scene.materials[i].name.c_str(), selected)) {
                    circle.material_id = i;
                    edited = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        Material& material = m_scene.materials[circle.material_id];
        int kind = static_cast<int>(material.kind);
        const char* kinds[] = {"Diffuse", "Dielectric"};
        if (ImGui::Combo("Material kind", &kind, kinds, IM_ARRAYSIZE(kinds))) {
            material.kind = static_cast<MaterialKind>(kind);
            edited = true;
        }
        float albedo[3] = {material.albedo.r, material.albedo.g, material.albedo.b};
        if (ImGui::ColorEdit3("Material albedo / glass tint", albedo)) {
            material.albedo = {albedo[0], albedo[1], albedo[2]};
            edited = true;
        }
        if (material.is_dielectric()) {
            if (ImGui::DragFloat("IOR", &material.ior, 0.01f, 1.0f, 3.0f)) {
                edited = true;
            }
        }

        if (edited) {
            circle.radius = std::max(0.02f, circle.radius);
            mark_scene_edited();
        }
        return;
    }

    ImGui::Text("Selected object: none");
}

void App::draw_canvas() {
    m_canvas_origin = ImGui::GetCursorScreenPos();
    m_canvas_size = ImGui::GetContentRegionAvail();
    m_canvas_size.x = std::max(m_canvas_size.x, 360.0f);
    m_canvas_size.y = std::max(m_canvas_size.y, 360.0f);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 canvas_max{m_canvas_origin.x + m_canvas_size.x, m_canvas_origin.y + m_canvas_size.y};
    draw_list->AddRectFilled(m_canvas_origin, canvas_max, rgba(18, 19, 23));
    draw_list->AddRect(m_canvas_origin, canvas_max, rgba(80, 84, 96));

    if (m_show_field_in_canvas && m_field.texture_id() != 0) {
        const FieldBounds b = m_field.bounds();
        const ImVec2 tl = world_to_screen({b.min.x, b.max.y});
        const ImVec2 br = world_to_screen({b.max.x, b.min.y});
        draw_list->AddImage(
            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(m_field.texture_id())),
            tl,
            br,
            ImVec2{0.0f, 0.0f},
            ImVec2{1.0f, 1.0f},
            IM_COL32(255, 255, 255, 210));
        draw_list->AddRect(tl, br, rgba(100, 150, 210, 180));
    }

    ImGui::InvisibleButton("scene_canvas", m_canvas_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiIO& io = ImGui::GetIO();

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const Vec2 world = screen_to_world(io.MousePos);
        select_scene_handle(world, 10.0f / m_view_scale);
        m_dragging_scene_handle = m_selected_handle >= 0;
        if (!m_dragging_scene_handle) {
            int px = 0;
            int py = 0;
            if (m_field.world_to_pixel(world, px, py)) {
                m_selected_pixel_x = px;
                m_selected_pixel_y = py;
                m_debug_uses_selected_pixel = true;
                m_debug_world_position = m_field.pixel_center_to_world(px, py);
                if (m_show_reservoir_debug && m_settings.kind == IntegratorKind::RISDirection) {
                    m_reservoir_windows.push_back({
                        m_next_reservoir_window_id++,
                        true,
                        px,
                        py,
                        m_debug_world_position
                    });
                }
            } else {
                m_debug_uses_selected_pixel = false;
                m_debug_world_position = world;
            }
            retrace_debug_sample();
        }
    }

    if (hovered && m_dragging_scene_handle && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const Vec2 delta{io.MouseDelta.x / m_view_scale, -io.MouseDelta.y / m_view_scale};
        if (m_selected_kind == SelectedObjectKind::Segment && m_selected_index >= 0 && m_selected_index < static_cast<int>(m_scene.segments.size())) {
            if (length_squared(delta) > 0.0f) {
                Segment& segment = m_scene.segments[m_selected_index];
                if (m_selected_handle == 0 || m_selected_handle == 2) {
                    segment.a += delta;
                }
                if (m_selected_handle == 1 || m_selected_handle == 2) {
                    segment.b += delta;
                }
                m_scene.refresh_segment_normal_keep_side(m_selected_index);
                mark_scene_edited();
            }
        } else if (m_selected_kind == SelectedObjectKind::Circle && m_selected_index >= 0 && m_selected_index < static_cast<int>(m_scene.circles.size())) {
            Circle& circle = m_scene.circles[m_selected_index];
            if (m_selected_handle == 1) {
                const Vec2 world = screen_to_world(io.MousePos);
                circle.radius = std::max(0.02f, length(world - circle.center));
                mark_scene_edited();
            } else if (length_squared(delta) > 0.0f) {
                circle.center += delta;
                mark_scene_edited();
            }
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        m_dragging_scene_handle = false;
        m_selected_handle = -1;
    }

    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        const ImVec2 delta = io.MouseDelta;
        m_view_center.x -= delta.x / m_view_scale;
        m_view_center.y += delta.y / m_view_scale;
    }

    if (hovered && io.MouseWheel != 0.0f) {
        const float zoom = io.MouseWheel > 0.0f ? 1.10f : 0.90f;
        m_view_scale = std::max(25.0f, std::min(420.0f, m_view_scale * zoom));
    }

    for (int i = -10; i <= 10; ++i) {
        const ImU32 grid_color = i == 0 ? rgba(55, 58, 68) : rgba(35, 38, 45);
        draw_list->AddLine(world_to_screen({static_cast<float>(i), -10.0f}), world_to_screen({static_cast<float>(i), 10.0f}), grid_color);
        draw_list->AddLine(world_to_screen({-10.0f, static_cast<float>(i)}), world_to_screen({10.0f, static_cast<float>(i)}), grid_color);
    }

    for (const Segment& segment : m_scene.segments) {
        const Material& material = m_scene.materials[segment.material_id];
        const bool light = material.is_light();
        const bool selected = m_selected_kind == SelectedObjectKind::Segment && segment.object_id == m_selected_index;
        const ImU32 color = selected ? rgba(255, 145, 90) : (light ? rgba(255, 230, 120) : rgba(210, 215, 225));
        const float thickness = selected ? 4.0f : (light ? 4.0f : 2.0f);
        draw_list->AddLine(world_to_screen(segment.a), world_to_screen(segment.b), color, thickness);

        draw_list->AddCircleFilled(world_to_screen(segment.a), selected ? 5.5f : 3.5f, selected ? rgba(255, 145, 90) : rgba(155, 160, 170));
        draw_list->AddCircleFilled(world_to_screen(segment.b), selected ? 5.5f : 3.5f, selected ? rgba(255, 145, 90) : rgba(155, 160, 170));

        if (m_show_normals) {
            const Vec2 mid = (segment.a + segment.b) * 0.5f;
            draw_arrow(draw_list, world_to_screen(mid), world_to_screen(mid + segment.normal * 0.18f), light ? rgba(255, 200, 80) : rgba(120, 145, 180), 1.0f);
        }
    }

    for (const Circle& circle : m_scene.circles) {
        const Material& material = m_scene.materials[circle.material_id];
        const bool selected = m_selected_kind == SelectedObjectKind::Circle && circle.object_id == m_selected_index;
        const ImU32 color = selected ? rgba(255, 145, 90) : (material.is_dielectric() ? rgba(120, 220, 255) : rgba(190, 210, 230));
        const ImVec2 c = world_to_screen(circle.center);
        const float r = circle.radius * m_view_scale;
        draw_list->AddCircle(c, r, color, 64, selected ? 3.0f : 2.0f);
        draw_list->AddCircleFilled(c, selected ? 4.5f : 3.0f, selected ? rgba(255, 145, 90) : rgba(120, 220, 255));
        draw_list->AddCircleFilled(world_to_screen(circle.center + Vec2{circle.radius, 0.0f}), selected ? 4.5f : 3.0f, selected ? rgba(255, 145, 90) : rgba(120, 220, 255));
        if (m_show_debug_labels && material.is_dielectric()) {
            char label[80];
            std::snprintf(label, sizeof(label), "glass ior %.2f", material.ior);
            draw_list->AddText(world_to_screen(circle.center + Vec2{-circle.radius, circle.radius + 0.05f}), rgba(160, 230, 255), label);
        }
    }

    draw_list->AddCircleFilled(world_to_screen(m_debug_world_position), 5.5f, rgba(255, 140, 90));
    if (m_show_debug_labels) {
        const char* src = m_debug_uses_selected_pixel ? "selected pixel sample" : "exact world sample";
        draw_list->AddText(world_to_screen(m_debug_world_position + Vec2{0.08f, 0.08f}), rgba(255, 185, 140), src);
    }

    draw_debug_events();
}

void App::draw_debug_events() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (const DebugEvent& event : m_debug_recorder.events()) {
        const ImU32 color = depth_color(event.depth);
        switch (event.type) {
            case DebugEventType::RaySegment:
                if (m_show_debug_rays) {
                    draw_arrow(draw_list, world_to_screen(event.ray_origin), world_to_screen(event.ray_end), color, 2.0f);
                }
                break;

            case DebugEventType::Hit:
                if (m_show_debug_hits) {
                    draw_list->AddCircleFilled(world_to_screen(event.hit_point), 4.5f, color);
                    if (m_show_normals) {
                        draw_arrow(draw_list, world_to_screen(event.hit_point), world_to_screen(event.hit_point + event.normal * 0.25f), rgba(180, 255, 180), 1.5f);
                    }
                    if (m_show_debug_labels) {
                        char label[80];
                        std::snprintf(label, sizeof(label), "d%d obj%d", event.depth, event.object_id);
                        draw_list->AddText(world_to_screen(event.hit_point + event.normal * 0.35f), rgba(230, 235, 245), label);
                    }
                }
                break;

            case DebugEventType::BsdfSample:
                if (m_show_debug_rays) {
                    const float len = event.depth < 0 ? 0.75f : 0.55f;
                    draw_arrow(draw_list, world_to_screen(event.hit_point), world_to_screen(event.hit_point + event.sampled_dir * len), event.depth < 0 ? rgba(255, 145, 90) : rgba(255, 210, 120), 1.5f);
                    if (m_show_debug_labels) {
                        char label[96];
                        std::snprintf(label, sizeof(label), event.depth < 0 ? "initial pdf %.3f" : "pdf %.3f", event.pdf);
                        draw_list->AddText(world_to_screen(event.hit_point + event.sampled_dir * (len + 0.10f)), rgba(255, 230, 160), label);
                    }
                }
                break;

            case DebugEventType::LightSample:
                draw_list->AddCircleFilled(world_to_screen(event.sampled_point), 5.5f, rgba(255, 240, 120));
                if (m_show_debug_rays) {
                    draw_list->AddLine(world_to_screen(event.hit_point), world_to_screen(event.sampled_point), event.blocked ? rgba(160, 90, 90, 150) : rgba(255, 235, 120, 180), 1.0f);
                }
                if (m_show_debug_labels) {
                    char label[192];
                    std::snprintf(label, sizeof(label), "G_N=%.4f\nL_N=%.4f", event.geometric_term, luminance(event.l_nee));
                    draw_list->AddText(world_to_screen(event.sampled_point + Vec2{0.05f, 0.05f}), event.blocked ? rgba(255, 120, 120) : rgba(255, 245, 150), label);
                }
                break;

            case DebugEventType::ShadowRay:
                if (m_show_debug_rays) {
                    draw_list->AddLine(world_to_screen(event.ray_origin), world_to_screen(event.ray_end), event.blocked ? rgba(255, 80, 80) : rgba(120, 255, 150), 1.5f);
                }
                break;

            case DebugEventType::DirectContribution:
                if (m_show_debug_labels) {
                    char label[224];
                    std::snprintf(label, sizeof(label), "L_N=%.4f\nL_P=%.4f\nL=%.4f",
                        luminance(event.l_nee),
                        luminance(event.l_path),
                        luminance(event.l_total));
                    draw_list->AddText(world_to_screen(event.hit_point + event.normal * 0.18f + Vec2{0.04f, 0.04f}), rgba(180, 255, 180), label);
                }
                break;

            case DebugEventType::HitLight:
                if (m_show_debug_hits) {
                    draw_list->AddCircleFilled(world_to_screen(event.hit_point), 7.0f, rgba(255, 240, 130));
                    if (m_show_debug_labels) {
                        char label[128];
                        std::snprintf(label, sizeof(label), "hit light\nL_P=%.4f", luminance(event.l_path));
                        draw_list->AddText(world_to_screen(event.hit_point + event.normal * 0.35f), rgba(255, 245, 150), label);
                    }
                }
                break;

            case DebugEventType::Miss:
            case DebugEventType::Terminated:
                break;
        }
    }
}

void App::draw_reservoir_polar_plot(const RISDirection& ris_direction, ImVec2 size) {
    size.x = std::max(size.x, 220.0f);
    size.y = std::max(size.y, 220.0f);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 max{origin.x + size.x, origin.y + size.y};
    draw_list->AddRectFilled(origin, max, rgba(14, 15, 18));
    draw_list->AddRect(origin, max, rgba(70, 75, 86));

    const ImVec2 center{origin.x + size.x * 0.5f, origin.y + size.y * 0.5f};
    const float radius = std::max(1.0f, std::min(size.x, size.y) * 0.42f);

    for (int i = 1; i <= 3; ++i) {
        draw_list->AddCircle(center, radius * static_cast<float>(i) / 3.0f, rgba(65, 70, 82), 64, 1.0f);
    }

    draw_list->AddLine(ImVec2{center.x - radius, center.y}, ImVec2{center.x + radius, center.y}, rgba(80, 86, 100), 1.0f);
    draw_list->AddLine(ImVec2{center.x, center.y - radius}, ImVec2{center.x, center.y + radius}, rgba(80, 86, 100), 1.0f);
    draw_list->AddText(ImVec2{center.x + radius + 6.0f, center.y - 7.0f}, rgba(170, 176, 190), "0");
    draw_list->AddText(ImVec2{center.x - 10.0f, center.y - radius - 18.0f}, rgba(170, 176, 190), "90");
    draw_list->AddText(ImVec2{center.x - radius - 24.0f, center.y - 7.0f}, rgba(170, 176, 190), "180");
    draw_list->AddText(ImVec2{center.x - 14.0f, center.y + radius + 4.0f}, rgba(170, 176, 190), "270");

    const std::vector<float> probabilities = ris_direction.probabilities();
    const int num_bins = static_cast<int>(probabilities.size());
    if (num_bins <= 0) {
        draw_list->AddText(ImVec2{origin.x + 12.0f, origin.y + 12.0f}, rgba(255, 150, 120), "No RIS data");
        return;
    }

    float max_probability = 0.0f;
    int max_bin = 0;
    for (int i = 0; i < num_bins; ++i) {
        if (probabilities[static_cast<size_t>(i)] > max_probability) {
            max_probability = probabilities[static_cast<size_t>(i)];
            max_bin = i;
        }
    }

    if (max_probability <= 0.0f || !std::isfinite(max_probability)) {
        return;
    }

    std::vector<ImVec2> endpoints;
    endpoints.reserve(static_cast<size_t>(num_bins));
    for (int i = 0; i < num_bins; ++i) {
        const float theta = (static_cast<float>(i) + 0.5f) * ris_direction.bin_width();
        const float normalized = std::clamp(probabilities[static_cast<size_t>(i)] / max_probability, 0.0f, 1.0f);
        const float r = radius * normalized;
        const ImVec2 endpoint{center.x + std::cos(theta) * r, center.y - std::sin(theta) * r};
        endpoints.push_back(endpoint);
        draw_list->AddLine(center, endpoint, rgba(120, 185, 255, 120), 1.0f);
    }

    for (int i = 0; i < num_bins; ++i) {
        const ImVec2 a = endpoints[static_cast<size_t>(i)];
        const ImVec2 b = endpoints[static_cast<size_t>((i + 1) % num_bins)];
        draw_list->AddLine(a, b, rgba(120, 210, 255), 2.0f);
    }

    const float max_theta = (static_cast<float>(max_bin) + 0.5f) * ris_direction.bin_width();
    const ImVec2 max_endpoint{center.x + std::cos(max_theta) * radius, center.y - std::sin(max_theta) * radius};
    draw_list->AddLine(center, max_endpoint, rgba(255, 190, 90), 3.0f);
}

void App::draw_reservoir_windows() {
    for (ReservoirDebugWindow& window : m_reservoir_windows) {
        if (!window.open) {
            continue;
        }

        char title[128];
        std::snprintf(title, sizeof(title), "RIS Reservoir (%d, %d)###ris_reservoir_%d", window.pixel_x, window.pixel_y, window.id);
        if (!ImGui::Begin(title, &window.open)) {
            ImGui::End();
            continue;
        }

        ImGui::Text("Pixel: (%d, %d)", window.pixel_x, window.pixel_y);
        ImGui::Text("World: %.3f, %.3f", window.world_position.x, window.world_position.y);
        ImGui::Text("Accumulated samples: %d", m_field.samples());

        const RISDirection* ris_direction = nullptr;
        if (m_settings.kind == IntegratorKind::RISDirection) {
            ris_direction = m_field.ris_direction_for_pixel(window.pixel_x, window.pixel_y);
        }

        if (ris_direction == nullptr) {
            ImGui::TextWrapped("No active RIS state for this pixel. Switch to RIS direction mode and render at least one sample, or reset windows after changing resolution/settings.");
            ImGui::End();
            continue;
        }

        const std::vector<float> probabilities = ris_direction->probabilities();
        if (probabilities.empty()) {
            ImGui::TextWrapped("No RIS probability data.");
            ImGui::End();
            continue;
        }

        float max_probability = 0.0f;
        int max_bin = 0;
        for (int i = 0; i < static_cast<int>(probabilities.size()); ++i) {
            if (probabilities[static_cast<size_t>(i)] > max_probability) {
                max_probability = probabilities[static_cast<size_t>(i)];
                max_bin = i;
            }
        }
        const float max_direction_deg = (static_cast<float>(max_bin) + 0.5f) * ris_direction->bin_width() * 180.0f / kPi;
        ImGui::Text("Max direction: %.1f deg", max_direction_deg);
        ImGui::Text("Max probability: %.4f", max_probability);
        if (ris_direction->score_sum() <= 0.0f) {
            ImGui::TextWrapped("No learned score yet. The distribution is uniform.");
        }

        const float available_width = ImGui::GetContentRegionAvail().x;
        const float plot_size = std::max(240.0f, std::min(available_width, 420.0f));
        draw_reservoir_polar_plot(*ris_direction, ImVec2{plot_size, plot_size});
        ImGui::End();
    }

    m_reservoir_windows.erase(
        std::remove_if(m_reservoir_windows.begin(), m_reservoir_windows.end(),
            [](const ReservoirDebugWindow& window) { return !window.open; }),
        m_reservoir_windows.end());
}

void App::retrace_debug_sample() {
    m_debug_recorder.clear();

    Sampler sampler(m_settings.seed);
    if (m_debug_uses_selected_pixel) {
        m_selected_pixel_x = std::clamp(m_selected_pixel_x, 0, std::max(0, m_field.width() - 1));
        m_selected_pixel_y = std::clamp(m_selected_pixel_y, 0, std::max(0, m_field.height() - 1));

        sampler = Sampler(m_field.seed_for_pixel(m_selected_pixel_x, m_selected_pixel_y, m_debug_sample_index, m_settings));
        m_debug_world_position = m_field.pixel_center_to_world(m_selected_pixel_x, m_selected_pixel_y);
    } else {
        sampler = Sampler(world_sample_seed(m_settings, m_debug_world_position, m_debug_sample_index));
    }

    PhotonMap photon_map;
    const PhotonMap* photon_map_ptr = nullptr;
    if (m_settings.kind == IntegratorKind::PhotonMapping) {
        const uint64_t photon_seed = hash_combine(sample_seed_base(m_settings), static_cast<uint64_t>(m_debug_sample_index) ^ 0x70686f746f6eULL);
        photon_map = build_photon_map(m_scene, m_settings, photon_seed);
        photon_map_ptr = &photon_map;
    }

    std::optional<RISDirection> debug_ris_direction;
    RISDirection* ris_direction_ptr = nullptr;
    if (m_settings.kind == IntegratorKind::RISDirection) {
        if (m_debug_uses_selected_pixel) {
            if (RISDirection* trained_ris = m_field.ris_direction_for_pixel(m_selected_pixel_x, m_selected_pixel_y, m_settings)) {
                debug_ris_direction = *trained_ris;
            }
        } else {
            const uint64_t ris_seed = world_sample_seed(m_settings, m_debug_world_position, 0) ^ 0x7269736469726563ULL;
            debug_ris_direction.emplace(
                m_settings.ris_direction.num_bins,
                m_settings.ris_direction.min_probability_percent,
                m_settings.ris_direction.smooth_sigma_deg,
                ris_seed);
        }

        if (debug_ris_direction.has_value()) {
            ris_direction_ptr = &debug_ris_direction.value();
        }
    }

    estimate_at(m_scene, m_debug_world_position, sampler, m_settings, &m_debug_recorder, photon_map_ptr, ris_direction_ptr);
}


void App::save_scene() {
    SceneDocumentSettings document_settings;
    document_settings.integrator = m_settings;
    document_settings.field_bounds_min = m_field_bounds.min;
    document_settings.field_bounds_max = m_field_bounds.max;
    document_settings.field_width = m_field_width;
    document_settings.field_height = m_field_height;
    document_settings.samples_per_frame = m_samples_per_frame;
    document_settings.stop_after_samples = m_stop_after_samples;

    std::string error;
    const bool ok = save_scene_json(m_scene_json_path, m_scene, document_settings, &error);
    m_scene_status = ok ? ("Saved scene: " + m_scene_json_path) : ("Failed to save scene: " + error);
}

void App::load_scene() {
    Scene next_scene;
    SceneDocumentSettings document_settings;
    document_settings.integrator = m_settings;
    document_settings.field_bounds_min = m_field_bounds.min;
    document_settings.field_bounds_max = m_field_bounds.max;
    document_settings.field_width = m_field_width;
    document_settings.field_height = m_field_height;
    document_settings.samples_per_frame = m_samples_per_frame;
    document_settings.stop_after_samples = m_stop_after_samples;

    std::string error;
    const bool ok = load_scene_json(m_scene_json_path, next_scene, document_settings, &error);
    if (!ok) {
        m_scene_status = "Failed to load scene: " + error;
        return;
    }

    m_scene = std::move(next_scene);
    m_settings = document_settings.integrator;
    m_field_bounds.min = document_settings.field_bounds_min;
    m_field_bounds.max = document_settings.field_bounds_max;
    m_field_width = document_settings.field_width;
    m_field_height = document_settings.field_height;
    m_samples_per_frame = document_settings.samples_per_frame;
    m_stop_after_samples = document_settings.stop_after_samples;
    clear_selection();
    m_render_paused = false;
    reset_accumulation();
    retrace_debug_sample();
    m_scene_status = "Loaded scene: " + m_scene_json_path;
}

void App::reset_accumulation() {
    if (m_field_bounds.min.x > m_field_bounds.max.x) std::swap(m_field_bounds.min.x, m_field_bounds.max.x);
    if (m_field_bounds.min.y > m_field_bounds.max.y) std::swap(m_field_bounds.min.y, m_field_bounds.max.y);
    m_field.reset(m_field_width, m_field_height, m_field_bounds);
    m_debug_sample_index = 0;

    // When auto-stop has paused rendering at the target sample count, any scene
    // edit or render-setting reset must start a fresh accumulation run.
    // Keep manual pause behavior unchanged when auto-stop is disabled.
    if (m_stop_after_samples > 0) {
        m_render_paused = false;
    }
}

ImVec2 App::world_to_screen(Vec2 p) const {
    return {
        m_canvas_origin.x + m_canvas_size.x * 0.5f + (p.x - m_view_center.x) * m_view_scale,
        m_canvas_origin.y + m_canvas_size.y * 0.5f - (p.y - m_view_center.y) * m_view_scale,
    };
}

Vec2 App::screen_to_world(ImVec2 p) const {
    return {
        (p.x - m_canvas_origin.x - m_canvas_size.x * 0.5f) / m_view_scale + m_view_center.x,
        -(p.y - m_canvas_origin.y - m_canvas_size.y * 0.5f) / m_view_scale + m_view_center.y,
    };
}

float App::distance_to_segment(Vec2 p, Vec2 a, Vec2 b) const {
    const Vec2 ab = b - a;
    const float denom = length_squared(ab);
    if (denom <= 1.0e-10f) {
        return length(p - a);
    }
    const float t = clamp01(dot(p - a, ab) / denom);
    const Vec2 q = a + ab * t;
    return length(p - q);
}

void App::select_scene_handle(Vec2 world, float radius_world) {
    m_selected_handle = -1;
    m_selected_kind = SelectedObjectKind::None;
    m_selected_index = -1;

    float best_distance = radius_world;
    SelectedObjectKind best_kind = SelectedObjectKind::None;
    int best_index = -1;
    int best_handle = -1;

    for (const Segment& segment : m_scene.segments) {
        const float da = length(world - segment.a);
        if (da < best_distance) {
            best_distance = da;
            best_kind = SelectedObjectKind::Segment;
            best_index = segment.object_id;
            best_handle = 0;
        }
        const float db = length(world - segment.b);
        if (db < best_distance) {
            best_distance = db;
            best_kind = SelectedObjectKind::Segment;
            best_index = segment.object_id;
            best_handle = 1;
        }
    }

    for (const Circle& circle : m_scene.circles) {
        const Vec2 radius_point = circle.center + Vec2{circle.radius, 0.0f};
        const float dr = length(world - radius_point);
        if (dr < best_distance) {
            best_distance = dr;
            best_kind = SelectedObjectKind::Circle;
            best_index = circle.object_id;
            best_handle = 1;
        }
        const float dc = length(world - circle.center);
        if (dc < best_distance) {
            best_distance = dc;
            best_kind = SelectedObjectKind::Circle;
            best_index = circle.object_id;
            best_handle = 0;
        }
    }

    if (best_kind == SelectedObjectKind::None) {
        float segment_best = radius_world * 0.75f;
        for (const Segment& segment : m_scene.segments) {
            const float d = distance_to_segment(world, segment.a, segment.b);
            if (d < segment_best) {
                segment_best = d;
                best_kind = SelectedObjectKind::Segment;
                best_index = segment.object_id;
                best_handle = 2;
            }
        }
    }

    if (best_kind == SelectedObjectKind::None) {
        for (const Circle& circle : m_scene.circles) {
            const float d = std::abs(length(world - circle.center) - circle.radius);
            if (d < radius_world * 0.75f) {
                best_kind = SelectedObjectKind::Circle;
                best_index = circle.object_id;
                best_handle = 2;
                break;
            }
            if (length(world - circle.center) < circle.radius) {
                best_kind = SelectedObjectKind::Circle;
                best_index = circle.object_id;
                best_handle = 2;
                break;
            }
        }
    }

    if (best_kind != SelectedObjectKind::None) {
        m_selected_kind = best_kind;
        m_selected_index = best_index;
        m_selected_handle = best_handle;
    }
}

int App::ensure_material(const char* name, Material material) {
    const int existing = m_scene.find_material_by_name(name);
    if (existing >= 0) {
        return existing;
    }
    material.name = name;
    return m_scene.add_material(material);
}

int App::default_wall_material() const {
    const int id = m_scene.find_material_by_name("white diffuse");
    return id >= 0 ? id : 0;
}

int App::default_light_material() const {
    const int id = m_scene.find_material_by_name("area light");
    return id >= 0 ? id : 0;
}

int App::default_glass_material() const {
    const int id = m_scene.find_material_by_name("glass dielectric");
    return id >= 0 ? id : 0;
}

void App::clear_selection() {
    m_selected_kind = SelectedObjectKind::None;
    m_selected_index = -1;
    m_selected_handle = -1;
    m_dragging_scene_handle = false;
}

void App::mark_scene_edited() {
    m_scene.rebuild_light_segment_ids();
    reset_accumulation();
    retrace_debug_sample();
}

} // namespace pt2d
