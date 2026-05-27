#include "pt2d/App.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

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

uint64_t kind_to_seed(IntegratorKind kind) {
    return static_cast<uint64_t>(static_cast<int>(kind) + 1) * 0x9e3779b97f4a7c15ULL;
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
    width_ = std::max(16, width);
    height_ = std::max(16, height);
    bounds_ = bounds;
    samples_ = 0;
    accum_.assign(static_cast<size_t>(width_ * height_), make_color(0.0f));
    rgba_.assign(static_cast<size_t>(width_ * height_ * 4), 0);

    if (texture_id_ == 0) {
        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    upload_to_texture();
}

uint64_t FieldAccumulator::seed_for_pixel(int x, int y, int sample_index, const IntegratorSettings& settings) const {
    uint64_t seed = settings.seed;
    seed = hash_combine(seed, kind_to_seed(settings.kind));
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, x)));
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, y)));
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, sample_index)));
    return seed;
}

Vec2 FieldAccumulator::pixel_center_to_world(int x, int y) const {
    return bounds_sample_position(bounds_, x, y, width_, height_, 0.5f, 0.5f);
}

Vec2 FieldAccumulator::sample_position_for_pixel(int x, int y, int sample_index, const IntegratorSettings& settings) const {
    (void)sample_index;
    (void)settings;
    return pixel_center_to_world(x, y);
}

bool FieldAccumulator::world_to_pixel(Vec2 p, int& x, int& y) const {
    if (width_ <= 0 || height_ <= 0) {
        x = 0;
        y = 0;
        return false;
    }

    const float w = bounds_.max.x - bounds_.min.x;
    const float h = bounds_.max.y - bounds_.min.y;
    if (w <= 0.0f || h <= 0.0f) {
        x = 0;
        y = 0;
        return false;
    }

    const float u = (p.x - bounds_.min.x) / w;
    const float v = (bounds_.max.y - p.y) / h;
    const bool inside = u >= 0.0f && u < 1.0f && v >= 0.0f && v < 1.0f;

    x = std::clamp(static_cast<int>(u * static_cast<float>(width_)), 0, width_ - 1);
    y = std::clamp(static_cast<int>(v * static_cast<float>(height_)), 0, height_ - 1);
    return inside;
}

void FieldAccumulator::accumulate(const Scene& scene, const IntegratorSettings& settings, int samples_per_frame) {
    if (accum_.empty()) {
        reset(width_, height_, bounds_);
    }

    const int spp = std::max(1, samples_per_frame);
    for (int s = 0; s < spp; ++s) {
        const int sample_index = samples_ + s;
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                Sampler sampler(seed_for_pixel(x, y, sample_index, settings));
                const Vec2 position = pixel_center_to_world(x, y);
                accum_[static_cast<size_t>(y * width_ + x)] += estimate_at(scene, position, sampler, settings, nullptr);
            }
        }
    }
    samples_ += spp;
}

void FieldAccumulator::update_rgba_buffer() {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            Color c = accum_.empty() || samples_ == 0
                ? make_color(0.0f)
                : accum_[static_cast<size_t>(y * width_ + x)] / static_cast<float>(samples_);
            c = clamp(tonemap(c));
            const size_t i = static_cast<size_t>((y * width_ + x) * 4);
            rgba_[i + 0] = to_srgb8(c.r);
            rgba_[i + 1] = to_srgb8(c.g);
            rgba_[i + 2] = to_srgb8(c.b);
            rgba_[i + 3] = 255;
        }
    }
}

void FieldAccumulator::upload_to_texture() {
    if (texture_id_ == 0) {
        glGenTextures(1, &texture_id_);
    }

    update_rgba_buffer();

    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_.data());
}

bool FieldAccumulator::save_png(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    update_rgba_buffer();
    return write_png_rgba8(path.c_str(), width_, height_, rgba_.data());
}

App::App() = default;

App::~App() {
    shutdown();
}

bool App::init() {
    scene_ = make_default_scene();
    settings_.kind = IntegratorKind::PathTracing;
    settings_.max_depth = 6;
    settings_.seed = 1;

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

    window_ = glfwCreateWindow(1360, 880, "Pathtracing2D Field Debugger", nullptr, nullptr);
    if (!window_) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    field_.reset(field_width_, field_height_, field_bounds_);
    debug_world_position_ = field_.pixel_center_to_world(selected_pixel_x_, selected_pixel_y_);
    retrace_debug_sample();
    return true;
}

void App::shutdown() {
    if (!window_) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
}

void App::run() {
    while (window_ && !glfwWindowShouldClose(window_) && running_) {
        glfwPollEvents();

        if (!render_paused_) {
            int spp = std::max(1, samples_per_frame_);
            if (stop_after_samples_ > 0) {
                const int remaining = stop_after_samples_ - field_.samples();
                if (remaining <= 0) {
                    render_paused_ = true;
                    spp = 0;
                } else {
                    spp = std::min(spp, remaining);
                }
            }

            if (spp > 0) {
                field_.accumulate(scene_, settings_, spp);
                field_.upload_to_texture();
                if (stop_after_samples_ > 0 && field_.samples() >= stop_after_samples_) {
                    render_paused_ = true;
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_main_menu();
        draw_field_panel();
        draw_scene_panel();

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.055f, 0.065f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
}

void App::draw_main_menu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save PNG")) {
                const bool ok = field_.save_png(save_png_path_);
                save_status_ = ok ? ("Saved PNG: " + save_png_path_) : ("Failed to save PNG: " + save_png_path_);
            }
            if (ImGui::MenuItem("Save Scene")) {
                save_scene();
            }
            if (ImGui::MenuItem("Load Scene")) {
                load_scene();
            }
            if (ImGui::MenuItem("Exit")) {
                running_ = false;
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

    int mode = static_cast<int>(settings_.kind);
    const char* modes[] = {
        "Path tracing",
        "Path tracing + NEE",
        "BDPT (planned)"
    };
    if (ImGui::Combo("Integrator", &mode, modes, IM_ARRAYSIZE(modes))) {
        settings_.kind = static_cast<IntegratorKind>(mode);
        reset_accumulation();
        retrace_debug_sample();
    }

    bool changed = false;
    changed |= ImGui::SliderInt("Max depth", &settings_.max_depth, 1, 16);
    changed |= ImGui::SliderInt("SPP / frame", &samples_per_frame_, 1, 8);
    changed |= ImGui::InputInt("Auto stop samples (0=infinite)", &stop_after_samples_);
    stop_after_samples_ = std::max(0, stop_after_samples_);

    int seed_as_int = static_cast<int>(settings_.seed);
    if (ImGui::InputInt("Seed", &seed_as_int)) {
        settings_.seed = static_cast<uint64_t>(std::max(1, seed_as_int));
        changed = true;
    }

    int new_width = field_width_;
    int new_height = field_height_;
    bool resolution_changed = false;
    resolution_changed |= ImGui::SliderInt("Field width", &new_width, 64, 768);
    resolution_changed |= ImGui::SliderInt("Field height", &new_height, 64, 768);

    bool bounds_changed = false;
    bounds_changed |= ImGui::DragFloat2("Bounds min", &field_bounds_.min.x, 0.02f);
    bounds_changed |= ImGui::DragFloat2("Bounds max", &field_bounds_.max.x, 0.02f);

    if (resolution_changed || bounds_changed) {
        field_width_ = new_width;
        field_height_ = new_height;
        reset_accumulation();
        retrace_debug_sample();
    } else if (changed) {
        reset_accumulation();
        retrace_debug_sample();
    }

    if (ImGui::Button(render_paused_ ? "Resume" : "Pause")) {
        render_paused_ = !render_paused_;
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
        const bool ok = field_.save_png(save_png_path_);
        save_status_ = ok ? ("Saved PNG: " + save_png_path_) : ("Failed to save PNG: " + save_png_path_);
    }

    char path_buffer[512];
    std::snprintf(path_buffer, sizeof(path_buffer), "%s", save_png_path_.c_str());
    if (ImGui::InputText("PNG path", path_buffer, sizeof(path_buffer))) {
        save_png_path_ = path_buffer;
    }
    if (!save_status_.empty()) {
        ImGui::TextWrapped("%s", save_status_.c_str());
    }

    ImGui::Text("Accumulated samples: %d", field_.samples());
    if (stop_after_samples_ > 0) {
        ImGui::Text("Auto stop target: %d", stop_after_samples_);
    } else {
        ImGui::Text("Auto stop target: infinite");
    }
    ImGui::TextWrapped("Each pixel is a deterministic world-space probe at the exact pixel center. Rendering and debug retrace use the same world position and the same seed for each (pixel, sample index), so the selected pixel is exactly reproducible.");
    if (settings_.kind == IntegratorKind::PathTracingNEE) {
        ImGui::TextWrapped("NEE debug: every diffuse hit also samples a light point. The canvas shows sampled light points, shadow segments, G_N, and per-vertex L_N / L_P.");
    } else if (settings_.kind == IntegratorKind::BidirectionalPT) {
        ImGui::TextWrapped("BDPT hook is reserved. Next: eye/probe path + light path + vertex connection visualization.");
    }

    ImGui::Separator();

    const float available_width = ImGui::GetContentRegionAvail().x;
    const float aspect = static_cast<float>(field_.height()) / static_cast<float>(std::max(1, field_.width()));
    ImVec2 image_size{std::max(240.0f, available_width), std::max(180.0f, available_width * aspect)};
    const ImVec2 image_pos = ImGui::GetCursorScreenPos();
    ImGui::Image(
        reinterpret_cast<ImTextureID>(static_cast<intptr_t>(field_.texture_id())),
        image_size);

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImGuiIO& io = ImGui::GetIO();
        const float rel_x = std::clamp((io.MousePos.x - image_pos.x) / image_size.x, 0.0f, 0.9999f);
        const float rel_y = std::clamp((io.MousePos.y - image_pos.y) / image_size.y, 0.0f, 0.9999f);
        selected_pixel_x_ = std::clamp(static_cast<int>(rel_x * field_.width()), 0, field_.width() - 1);
        selected_pixel_y_ = std::clamp(static_cast<int>(rel_y * field_.height()), 0, field_.height() - 1);
        debug_uses_selected_pixel_ = true;
        retrace_debug_sample();
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (debug_uses_selected_pixel_) {
        const float x0 = image_pos.x + image_size.x * (static_cast<float>(selected_pixel_x_) / static_cast<float>(field_.width()));
        const float y0 = image_pos.y + image_size.y * (static_cast<float>(selected_pixel_y_) / static_cast<float>(field_.height()));
        const float x1 = image_pos.x + image_size.x * (static_cast<float>(selected_pixel_x_ + 1) / static_cast<float>(field_.width()));
        const float y1 = image_pos.y + image_size.y * (static_cast<float>(selected_pixel_y_ + 1) / static_cast<float>(field_.height()));
        draw_list->AddRect(ImVec2{x0, y0}, ImVec2{x1, y1}, rgba(255, 130, 80), 0.0f, 0, 2.0f);
    }

    ImGui::Separator();
    ImGui::Text("Selected pixel: (%d, %d)", selected_pixel_x_, selected_pixel_y_);
    ImGui::Text("Debug position: %.3f, %.3f", debug_world_position_.x, debug_world_position_.y);

    const int max_sample = std::max(0, field_.samples() - 1);
    debug_sample_index_ = std::clamp(debug_sample_index_, 0, max_sample);
    if (ImGui::SliderInt("Debug sample index", &debug_sample_index_, 0, std::max(0, max_sample))) {
        retrace_debug_sample();
    }
    if (ImGui::Button("Previous sample")) {
        debug_sample_index_ = std::max(0, debug_sample_index_ - 1);
        retrace_debug_sample();
    }
    ImGui::SameLine();
    if (ImGui::Button("Next sample")) {
        debug_sample_index_ += 1;
        retrace_debug_sample();
    }

    ImGui::End();
}

void App::draw_scene_panel() {
    ImGui::Begin("Scene Editor / Path Debug");

    draw_scene_controls();
    ImGui::Separator();

    ImGui::Checkbox("Field overlay", &show_field_in_canvas_);
    ImGui::SameLine();
    ImGui::Checkbox("Rays", &show_debug_rays_);
    ImGui::SameLine();
    ImGui::Checkbox("Hits", &show_debug_hits_);
    ImGui::SameLine();
    ImGui::Checkbox("Normals", &show_normals_);
    ImGui::SameLine();
    ImGui::Checkbox("Labels", &show_debug_labels_);

    ImGui::TextWrapped("Image click: select a field pixel. Canvas click near an object: select/edit it. Canvas click empty space: debug exact world position. Drag selected handles to modify geometry. Right drag pans, mouse wheel zooms.");
    draw_canvas();

    ImGui::End();
}

void App::draw_scene_controls() {
    if (ImGui::Button("Default scene")) {
        scene_ = make_default_scene();
        clear_selection();
        mark_scene_edited();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset view")) {
        view_center_ = {0.0f, 0.0f};
        view_scale_ = 110.0f;
    }

    char scene_path_buffer[512];
    std::snprintf(scene_path_buffer, sizeof(scene_path_buffer), "%s", scene_json_path_.c_str());
    if (ImGui::InputText("Scene JSON path", scene_path_buffer, sizeof(scene_path_buffer))) {
        scene_json_path_ = scene_path_buffer;
    }
    if (ImGui::Button("Save scene")) {
        save_scene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load scene")) {
        load_scene();
    }
    if (!scene_status_.empty()) {
        ImGui::TextWrapped("%s", scene_status_.c_str());
    }

    if (ImGui::Button("Add wall")) {
        const int mat = default_wall_material();
        const Vec2 a = view_center_ + Vec2{-0.7f, -0.25f};
        const Vec2 b = view_center_ + Vec2{ 0.7f,  0.25f};
        const Vec2 n = normalize(perpendicular(b - a));
        selected_kind_ = SelectedObjectKind::Segment;
        selected_index_ = scene_.add_segment(a, b, n, mat);
        selected_handle_ = -1;
        mark_scene_edited();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add light")) {
        const int mat = default_light_material();
        const Vec2 a = view_center_ + Vec2{-0.55f, 0.65f};
        const Vec2 b = view_center_ + Vec2{ 0.55f, 0.65f};
        selected_kind_ = SelectedObjectKind::Segment;
        selected_index_ = scene_.add_segment(a, b, {0.0f, -1.0f}, mat);
        selected_handle_ = -1;
        mark_scene_edited();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add glass circle")) {
        const int mat = default_glass_material();
        selected_kind_ = SelectedObjectKind::Circle;
        selected_index_ = scene_.add_circle(view_center_, 0.45f, mat);
        selected_handle_ = -1;
        mark_scene_edited();
    }

    if (selected_kind_ == SelectedObjectKind::Segment && selected_index_ >= 0 && selected_index_ < static_cast<int>(scene_.segments.size())) {
        Segment& segment = scene_.segments[selected_index_];
        ImGui::Separator();
        ImGui::Text("Selected segment: %d", selected_index_);

        if (ImGui::Button("Delete selected")) {
            scene_.erase_segment(selected_index_);
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
            selected_index_ = scene_.add_segment(copy.a, copy.b, copy.normal, copy.material_id);
            mark_scene_edited();
            return;
        }

        bool edited = false;
        edited |= ImGui::DragFloat2("A", &segment.a.x, 0.02f);
        edited |= ImGui::DragFloat2("B", &segment.b.x, 0.02f);

        int material_id = segment.material_id;
        if (ImGui::BeginCombo("Material", scene_.materials[material_id].name.c_str())) {
            for (int i = 0; i < static_cast<int>(scene_.materials.size()); ++i) {
                const bool selected = i == material_id;
                if (ImGui::Selectable(scene_.materials[i].name.c_str(), selected)) {
                    segment.material_id = i;
                    edited = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        Material& material = scene_.materials[segment.material_id];
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

        if (material.is_dielectric()) {
            if (ImGui::DragFloat("IOR", &material.ior, 0.01f, 1.0f, 3.0f)) {
                edited = true;
            }
        }

        if (edited) {
            scene_.refresh_segment_normal_keep_side(selected_index_);
            mark_scene_edited();
        }
        return;
    }

    if (selected_kind_ == SelectedObjectKind::Circle && selected_index_ >= 0 && selected_index_ < static_cast<int>(scene_.circles.size())) {
        Circle& circle = scene_.circles[selected_index_];
        ImGui::Separator();
        ImGui::Text("Selected circle: %d", selected_index_);

        if (ImGui::Button("Delete selected")) {
            scene_.erase_circle(selected_index_);
            clear_selection();
            mark_scene_edited();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            Circle copy = circle;
            copy.center += Vec2{0.2f, 0.15f};
            selected_index_ = scene_.add_circle(copy.center, copy.radius, copy.material_id);
            selected_kind_ = SelectedObjectKind::Circle;
            mark_scene_edited();
            return;
        }

        bool edited = false;
        edited |= ImGui::DragFloat2("Center", &circle.center.x, 0.02f);
        edited |= ImGui::DragFloat("Radius", &circle.radius, 0.01f, 0.02f, 5.0f);

        int material_id = circle.material_id;
        if (ImGui::BeginCombo("Material", scene_.materials[material_id].name.c_str())) {
            for (int i = 0; i < static_cast<int>(scene_.materials.size()); ++i) {
                const bool selected = i == material_id;
                if (ImGui::Selectable(scene_.materials[i].name.c_str(), selected)) {
                    circle.material_id = i;
                    edited = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        Material& material = scene_.materials[circle.material_id];
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
    canvas_origin_ = ImGui::GetCursorScreenPos();
    canvas_size_ = ImGui::GetContentRegionAvail();
    canvas_size_.x = std::max(canvas_size_.x, 360.0f);
    canvas_size_.y = std::max(canvas_size_.y, 360.0f);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 canvas_max{canvas_origin_.x + canvas_size_.x, canvas_origin_.y + canvas_size_.y};
    draw_list->AddRectFilled(canvas_origin_, canvas_max, rgba(18, 19, 23));
    draw_list->AddRect(canvas_origin_, canvas_max, rgba(80, 84, 96));

    if (show_field_in_canvas_ && field_.texture_id() != 0) {
        const FieldBounds b = field_.bounds();
        const ImVec2 tl = world_to_screen({b.min.x, b.max.y});
        const ImVec2 br = world_to_screen({b.max.x, b.min.y});
        draw_list->AddImage(
            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(field_.texture_id())),
            tl,
            br,
            ImVec2{0.0f, 0.0f},
            ImVec2{1.0f, 1.0f},
            IM_COL32(255, 255, 255, 210));
        draw_list->AddRect(tl, br, rgba(100, 150, 210, 180));
    }

    ImGui::InvisibleButton("scene_canvas", canvas_size_, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiIO& io = ImGui::GetIO();

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const Vec2 world = screen_to_world(io.MousePos);
        select_scene_handle(world, 10.0f / view_scale_);
        if (selected_handle_ < 0) {
            int px = 0;
            int py = 0;
            if (field_.world_to_pixel(world, px, py)) {
                selected_pixel_x_ = px;
                selected_pixel_y_ = py;
                debug_uses_selected_pixel_ = true;
                debug_world_position_ = field_.pixel_center_to_world(px, py);
            } else {
                debug_uses_selected_pixel_ = false;
                debug_world_position_ = world;
            }
            retrace_debug_sample();
        }
    }

    if (hovered && selected_handle_ >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const Vec2 delta{io.MouseDelta.x / view_scale_, -io.MouseDelta.y / view_scale_};
        if (selected_kind_ == SelectedObjectKind::Segment && selected_index_ >= 0 && selected_index_ < static_cast<int>(scene_.segments.size())) {
            if (length_squared(delta) > 0.0f) {
                Segment& segment = scene_.segments[selected_index_];
                if (selected_handle_ == 0 || selected_handle_ == 2) {
                    segment.a += delta;
                }
                if (selected_handle_ == 1 || selected_handle_ == 2) {
                    segment.b += delta;
                }
                scene_.refresh_segment_normal_keep_side(selected_index_);
                mark_scene_edited();
            }
        } else if (selected_kind_ == SelectedObjectKind::Circle && selected_index_ >= 0 && selected_index_ < static_cast<int>(scene_.circles.size())) {
            Circle& circle = scene_.circles[selected_index_];
            if (selected_handle_ == 1) {
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
        selected_handle_ = -1;
    }

    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        const ImVec2 delta = io.MouseDelta;
        view_center_.x -= delta.x / view_scale_;
        view_center_.y += delta.y / view_scale_;
    }

    if (hovered && io.MouseWheel != 0.0f) {
        const float zoom = io.MouseWheel > 0.0f ? 1.10f : 0.90f;
        view_scale_ = std::max(25.0f, std::min(420.0f, view_scale_ * zoom));
    }

    for (int i = -10; i <= 10; ++i) {
        const ImU32 grid_color = i == 0 ? rgba(55, 58, 68) : rgba(35, 38, 45);
        draw_list->AddLine(world_to_screen({static_cast<float>(i), -10.0f}), world_to_screen({static_cast<float>(i), 10.0f}), grid_color);
        draw_list->AddLine(world_to_screen({-10.0f, static_cast<float>(i)}), world_to_screen({10.0f, static_cast<float>(i)}), grid_color);
    }

    for (const Segment& segment : scene_.segments) {
        const Material& material = scene_.materials[segment.material_id];
        const bool light = material.is_light();
        const bool selected = selected_kind_ == SelectedObjectKind::Segment && segment.object_id == selected_index_;
        const ImU32 color = selected ? rgba(255, 145, 90) : (light ? rgba(255, 230, 120) : rgba(210, 215, 225));
        const float thickness = selected ? 4.0f : (light ? 4.0f : 2.0f);
        draw_list->AddLine(world_to_screen(segment.a), world_to_screen(segment.b), color, thickness);

        draw_list->AddCircleFilled(world_to_screen(segment.a), selected ? 5.5f : 3.5f, selected ? rgba(255, 145, 90) : rgba(155, 160, 170));
        draw_list->AddCircleFilled(world_to_screen(segment.b), selected ? 5.5f : 3.5f, selected ? rgba(255, 145, 90) : rgba(155, 160, 170));

        if (show_normals_) {
            const Vec2 mid = (segment.a + segment.b) * 0.5f;
            draw_arrow(draw_list, world_to_screen(mid), world_to_screen(mid + segment.normal * 0.18f), light ? rgba(255, 200, 80) : rgba(120, 145, 180), 1.0f);
        }
    }

    for (const Circle& circle : scene_.circles) {
        const Material& material = scene_.materials[circle.material_id];
        const bool selected = selected_kind_ == SelectedObjectKind::Circle && circle.object_id == selected_index_;
        const ImU32 color = selected ? rgba(255, 145, 90) : (material.is_dielectric() ? rgba(120, 220, 255) : rgba(190, 210, 230));
        const ImVec2 c = world_to_screen(circle.center);
        const float r = circle.radius * view_scale_;
        draw_list->AddCircle(c, r, color, 64, selected ? 3.0f : 2.0f);
        draw_list->AddCircleFilled(c, selected ? 4.5f : 3.0f, selected ? rgba(255, 145, 90) : rgba(120, 220, 255));
        draw_list->AddCircleFilled(world_to_screen(circle.center + Vec2{circle.radius, 0.0f}), selected ? 4.5f : 3.0f, selected ? rgba(255, 145, 90) : rgba(120, 220, 255));
        if (show_debug_labels_ && material.is_dielectric()) {
            char label[80];
            std::snprintf(label, sizeof(label), "glass ior %.2f", material.ior);
            draw_list->AddText(world_to_screen(circle.center + Vec2{-circle.radius, circle.radius + 0.05f}), rgba(160, 230, 255), label);
        }
    }

    draw_list->AddCircleFilled(world_to_screen(debug_world_position_), 5.5f, rgba(255, 140, 90));
    if (show_debug_labels_) {
        const char* src = debug_uses_selected_pixel_ ? "selected pixel sample" : "exact world sample";
        draw_list->AddText(world_to_screen(debug_world_position_ + Vec2{0.08f, 0.08f}), rgba(255, 185, 140), src);
    }

    draw_debug_events();
}

void App::draw_debug_events() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (const DebugEvent& event : debug_recorder_.events()) {
        const ImU32 color = depth_color(event.depth);
        switch (event.type) {
            case DebugEventType::RaySegment:
                if (show_debug_rays_) {
                    draw_arrow(draw_list, world_to_screen(event.ray_origin), world_to_screen(event.ray_end), color, 2.0f);
                }
                break;

            case DebugEventType::Hit:
                if (show_debug_hits_) {
                    draw_list->AddCircleFilled(world_to_screen(event.hit_point), 4.5f, color);
                    if (show_normals_) {
                        draw_arrow(draw_list, world_to_screen(event.hit_point), world_to_screen(event.hit_point + event.normal * 0.25f), rgba(180, 255, 180), 1.5f);
                    }
                    if (show_debug_labels_) {
                        char label[80];
                        std::snprintf(label, sizeof(label), "d%d obj%d", event.depth, event.object_id);
                        draw_list->AddText(world_to_screen(event.hit_point + event.normal * 0.35f), rgba(230, 235, 245), label);
                    }
                }
                break;

            case DebugEventType::BsdfSample:
                if (show_debug_rays_) {
                    const float len = event.depth < 0 ? 0.75f : 0.55f;
                    draw_arrow(draw_list, world_to_screen(event.hit_point), world_to_screen(event.hit_point + event.sampled_dir * len), event.depth < 0 ? rgba(255, 145, 90) : rgba(255, 210, 120), 1.5f);
                    if (show_debug_labels_) {
                        char label[96];
                        std::snprintf(label, sizeof(label), event.depth < 0 ? "initial pdf %.3f" : "pdf %.3f", event.pdf);
                        draw_list->AddText(world_to_screen(event.hit_point + event.sampled_dir * (len + 0.10f)), rgba(255, 230, 160), label);
                    }
                }
                break;

            case DebugEventType::LightSample:
                draw_list->AddCircleFilled(world_to_screen(event.sampled_point), 5.5f, rgba(255, 240, 120));
                if (show_debug_rays_) {
                    draw_list->AddLine(world_to_screen(event.hit_point), world_to_screen(event.sampled_point), event.blocked ? rgba(160, 90, 90, 150) : rgba(255, 235, 120, 180), 1.0f);
                }
                if (show_debug_labels_) {
                    char label[192];
                    std::snprintf(label, sizeof(label), "G_N=%.4f\nL_N=%.4f", event.geometric_term, luminance(event.l_nee));
                    draw_list->AddText(world_to_screen(event.sampled_point + Vec2{0.05f, 0.05f}), event.blocked ? rgba(255, 120, 120) : rgba(255, 245, 150), label);
                }
                break;

            case DebugEventType::ShadowRay:
                if (show_debug_rays_) {
                    draw_list->AddLine(world_to_screen(event.ray_origin), world_to_screen(event.ray_end), event.blocked ? rgba(255, 80, 80) : rgba(120, 255, 150), 1.5f);
                }
                break;

            case DebugEventType::DirectContribution:
                if (show_debug_labels_) {
                    char label[224];
                    std::snprintf(label, sizeof(label), "L_N=%.4f\nL_P=%.4f\nL=%.4f",
                        luminance(event.l_nee),
                        luminance(event.l_path),
                        luminance(event.l_total));
                    draw_list->AddText(world_to_screen(event.hit_point + event.normal * 0.18f + Vec2{0.04f, 0.04f}), rgba(180, 255, 180), label);
                }
                break;

            case DebugEventType::HitLight:
                if (show_debug_hits_) {
                    draw_list->AddCircleFilled(world_to_screen(event.hit_point), 7.0f, rgba(255, 240, 130));
                    if (show_debug_labels_) {
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

void App::retrace_debug_sample() {
    debug_recorder_.clear();

    Sampler sampler(settings_.seed);
    if (debug_uses_selected_pixel_) {
        selected_pixel_x_ = std::clamp(selected_pixel_x_, 0, std::max(0, field_.width() - 1));
        selected_pixel_y_ = std::clamp(selected_pixel_y_, 0, std::max(0, field_.height() - 1));

        sampler = Sampler(field_.seed_for_pixel(selected_pixel_x_, selected_pixel_y_, debug_sample_index_, settings_));
        debug_world_position_ = field_.pixel_center_to_world(selected_pixel_x_, selected_pixel_y_);
    } else {
        uint64_t seed = settings_.seed;
        seed = hash_combine(seed, kind_to_seed(settings_.kind));
        seed = hash_combine(seed, static_cast<uint64_t>(debug_sample_index_));
        seed = hash_combine(seed, static_cast<uint64_t>((debug_world_position_.x + 1000.0f) * 1000.0f));
        seed = hash_combine(seed, static_cast<uint64_t>((debug_world_position_.y + 1000.0f) * 1000.0f));
        sampler = Sampler(seed);
    }

    estimate_at(scene_, debug_world_position_, sampler, settings_, &debug_recorder_);
}


void App::save_scene() {
    SceneDocumentSettings document_settings;
    document_settings.integrator = settings_;
    document_settings.field_bounds_min = field_bounds_.min;
    document_settings.field_bounds_max = field_bounds_.max;
    document_settings.field_width = field_width_;
    document_settings.field_height = field_height_;
    document_settings.samples_per_frame = samples_per_frame_;
    document_settings.stop_after_samples = stop_after_samples_;

    std::string error;
    const bool ok = save_scene_json(scene_json_path_, scene_, document_settings, &error);
    scene_status_ = ok ? ("Saved scene: " + scene_json_path_) : ("Failed to save scene: " + error);
}

void App::load_scene() {
    Scene next_scene;
    SceneDocumentSettings document_settings;
    document_settings.integrator = settings_;
    document_settings.field_bounds_min = field_bounds_.min;
    document_settings.field_bounds_max = field_bounds_.max;
    document_settings.field_width = field_width_;
    document_settings.field_height = field_height_;
    document_settings.samples_per_frame = samples_per_frame_;
    document_settings.stop_after_samples = stop_after_samples_;

    std::string error;
    const bool ok = load_scene_json(scene_json_path_, next_scene, document_settings, &error);
    if (!ok) {
        scene_status_ = "Failed to load scene: " + error;
        return;
    }

    scene_ = std::move(next_scene);
    settings_ = document_settings.integrator;
    field_bounds_.min = document_settings.field_bounds_min;
    field_bounds_.max = document_settings.field_bounds_max;
    field_width_ = document_settings.field_width;
    field_height_ = document_settings.field_height;
    samples_per_frame_ = document_settings.samples_per_frame;
    stop_after_samples_ = document_settings.stop_after_samples;
    clear_selection();
    render_paused_ = false;
    reset_accumulation();
    retrace_debug_sample();
    scene_status_ = "Loaded scene: " + scene_json_path_;
}

void App::reset_accumulation() {
    if (field_bounds_.min.x > field_bounds_.max.x) std::swap(field_bounds_.min.x, field_bounds_.max.x);
    if (field_bounds_.min.y > field_bounds_.max.y) std::swap(field_bounds_.min.y, field_bounds_.max.y);
    field_.reset(field_width_, field_height_, field_bounds_);
    debug_sample_index_ = 0;
}

ImVec2 App::world_to_screen(Vec2 p) const {
    return {
        canvas_origin_.x + canvas_size_.x * 0.5f + (p.x - view_center_.x) * view_scale_,
        canvas_origin_.y + canvas_size_.y * 0.5f - (p.y - view_center_.y) * view_scale_,
    };
}

Vec2 App::screen_to_world(ImVec2 p) const {
    return {
        (p.x - canvas_origin_.x - canvas_size_.x * 0.5f) / view_scale_ + view_center_.x,
        -(p.y - canvas_origin_.y - canvas_size_.y * 0.5f) / view_scale_ + view_center_.y,
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
    selected_handle_ = -1;
    selected_kind_ = SelectedObjectKind::None;
    selected_index_ = -1;

    float best_distance = radius_world;
    SelectedObjectKind best_kind = SelectedObjectKind::None;
    int best_index = -1;
    int best_handle = -1;

    for (const Segment& segment : scene_.segments) {
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

    for (const Circle& circle : scene_.circles) {
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
        for (const Segment& segment : scene_.segments) {
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
        for (const Circle& circle : scene_.circles) {
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
        selected_kind_ = best_kind;
        selected_index_ = best_index;
        selected_handle_ = best_handle;
    }
}

int App::ensure_material(const char* name, Material material) {
    const int existing = scene_.find_material_by_name(name);
    if (existing >= 0) {
        return existing;
    }
    material.name = name;
    return scene_.add_material(material);
}

int App::default_wall_material() const {
    const int id = scene_.find_material_by_name("white diffuse");
    return id >= 0 ? id : 0;
}

int App::default_light_material() const {
    const int id = scene_.find_material_by_name("area light");
    return id >= 0 ? id : 0;
}

int App::default_glass_material() const {
    const int id = scene_.find_material_by_name("glass dielectric");
    return id >= 0 ? id : 0;
}

void App::clear_selection() {
    selected_kind_ = SelectedObjectKind::None;
    selected_index_ = -1;
    selected_handle_ = -1;
}

void App::mark_scene_edited() {
    scene_.rebuild_light_segment_ids();
    reset_accumulation();
    retrace_debug_sample();
}

} // namespace pt2d
