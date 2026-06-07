#include "pt2d/ProductionApp.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ctime>
#include <optional>
#include <sstream>
#include <thread>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include "pt2d/PngWriter.h"
#include "pt2d/Sampler.h"

namespace pt2d {
namespace {

constexpr char kRenderStateMagic[8] = {'P', 'T', '2', 'D', 'S', 'T', 'A', 'T'};
constexpr std::uint32_t kRenderStateVersion = 1;

uint64_t integrator_kind_seed(IntegratorKind kind) {
    return static_cast<uint64_t>(static_cast<int>(kind) + 1) * 0x9e3779b97f4a7c15ULL;
}

uint64_t sample_seed_base(const IntegratorSettings& settings) {
    return hash_combine(settings.seed, integrator_kind_seed(settings.kind));
}

Vec2 bounds_sample_position(Vec2 bounds_min, Vec2 bounds_max, int x, int y, int width, int height) {
    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(std::max(1, width));
    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(std::max(1, height));
    return {
        bounds_min.x + u * (bounds_max.x - bounds_min.x),
        bounds_max.y - v * (bounds_max.y - bounds_min.y),
    };
}

float luminance(Color c) {
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

std::string format_duration(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        return "--:--";
    }
    const int total = static_cast<int>(seconds + 0.5);
    const int h = total / 3600;
    const int m = (total / 60) % 60;
    const int s = total % 60;
    char buf[32];
    if (h > 0) {
        std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
    } else {
        std::snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    }
    return buf;
}

std::string format_frame_rate(double frames_per_sec) {
    if (!std::isfinite(frames_per_sec) || frames_per_sec <= 0.0) {
        return "-- fps";
    }
    char buf[32];
    if (frames_per_sec >= 1.0) {
        std::snprintf(buf, sizeof(buf), "%.2f fps", frames_per_sec);
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f s/f", 1.0 / frames_per_sec);
    }
    return buf;
}

int clamp_render_dimension(int value) {
    return std::clamp(value, 1, 16384);
}

void make_rgba_from_accum(const std::vector<Color>& accum, int samples, int width, int height, std::vector<unsigned char>& rgba) {
    rgba.assign(static_cast<std::size_t>(std::max(1, width) * std::max(1, height) * 4), 0);
    const float inv_samples = samples > 0 ? 1.0f / static_cast<float>(samples) : 0.0f;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Color c = samples > 0 ? accum[static_cast<std::size_t>(y * width + x)] * inv_samples : make_color(0.0f);
            c = clamp(tonemap(c));
            const std::size_t i = static_cast<std::size_t>((y * width + x) * 4);
            rgba[i + 0] = to_srgb8(c.r);
            rgba[i + 1] = to_srgb8(c.g);
            rgba[i + 2] = to_srgb8(c.b);
            rgba[i + 3] = 255;
        }
    }
}

template <typename T>
bool write_pod(std::ostream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return static_cast<bool>(out);
}

template <typename T>
bool read_pod(std::istream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

bool write_string(std::ostream& out, const std::string& value) {
    const std::uint64_t size = static_cast<std::uint64_t>(value.size());
    if (!write_pod(out, size)) return false;
    if (size > 0) {
        out.write(value.data(), static_cast<std::streamsize>(size));
    }
    return static_cast<bool>(out);
}

bool read_string(std::istream& in, std::string& value) {
    std::uint64_t size = 0;
    if (!read_pod(in, size)) return false;
    if (size > 64ull * 1024ull * 1024ull) return false;
    value.assign(static_cast<std::size_t>(size), '\0');
    if (size > 0) {
        in.read(value.data(), static_cast<std::streamsize>(size));
    }
    return static_cast<bool>(in);
}

bool read_file_text(const std::string& path, std::string& text) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    text = buffer.str();
    return true;
}

std::filesystem::path temp_scene_path_from_state() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("pt2d_renderstate_scene_" + std::to_string(now) + ".json");
}

std::string timestamp_now_string() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

std::string default_log_path_for_output(const std::string& output_png_path) {
    if (output_png_path.empty()) {
        return "production_output.log";
    }
    std::filesystem::path p(output_png_path);
    p.replace_extension(".log");
    return p.string();
}

namespace crashlog {
#ifdef _WIN32
char g_path[MAX_PATH] = "production_output.log";
void write_raw(const char* text) {
    HANDLE h = CreateFileA(g_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(h, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
    CloseHandle(h);
}
LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* info) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "[CRASH] Windows unhandled exception code=0x%08lx address=%p\n",
        static_cast<unsigned long>(info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0),
        info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr);
    write_raw(buf);
    return EXCEPTION_EXECUTE_HANDLER;
}
void set_path(const std::string& path) {
    if (!path.empty()) {
        std::snprintf(g_path, sizeof(g_path), "%s", path.c_str());
    }
    SetUnhandledExceptionFilter(unhandled_exception_filter);
}
#else
char g_path[4096] = "production_output.log";
void write_raw(const char* text) {
    const int fd = ::open(g_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        return;
    }
    const char* p = text;
    while (*p) {
        ++p;
    }
    (void)::write(fd, text, static_cast<size_t>(p - text));
    ::close(fd);
}
void signal_handler(int sig) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "[CRASH] signal=%d\n", sig);
    write_raw(buf);
    std::_Exit(128 + sig);
}
void set_path(const std::string& path) {
    if (!path.empty()) {
        std::snprintf(g_path, sizeof(g_path), "%s", path.c_str());
    }
    std::signal(SIGSEGV, signal_handler);
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGFPE, signal_handler);
    std::signal(SIGILL, signal_handler);
}
#endif

void terminate_handler() {
    write_raw("[CRASH] std::terminate called\n");
    std::abort();
}
} // namespace crashlog

} // namespace

ProductionApp::ProductionApp(std::string initial_scene_path)
    : m_scene_path(std::move(initial_scene_path)) {
    if (!m_scene_path.empty()) {
        const std::filesystem::path scene_path(m_scene_path);
        const std::filesystem::path stem = scene_path.stem();
        m_output_png_path = stem.string() + "_production.png";
        m_renderstate_path = stem.string() + ".renderstate";
        update_log_path_from_output();
    } else {
        update_log_path_from_output();
    }
}

ProductionApp::~ProductionApp() {
    request_stop();
    if (m_worker.joinable()) {
        m_worker.join();
    }
    shutdown();
}

bool ProductionApp::init() {
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

    m_window = glfwCreateWindow(1280, 800, "Pathtracing2D Production Render", nullptr, nullptr);
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

    glGenTextures(1, &m_preview_texture_id);
    glBindTexture(GL_TEXTURE_2D, m_preview_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    update_log_path_from_output();
    crashlog::set_path(m_log_path);
    std::set_terminate(crashlog::terminate_handler);
    write_log("Production GUI initialized");

    m_scene = make_default_scene();
    m_doc_settings.field_width = 320;
    m_doc_settings.field_height = 224;
    m_doc_settings.integrator.kind = IntegratorKind::RISDirection;
    initialize_render_buffers(m_doc_settings);

    if (!m_scene_path.empty()) {
        load_scene_from_path(m_scene_path);
    }

    return true;
}

void ProductionApp::shutdown() {
    if (!m_window) {
        return;
    }
    if (m_preview_texture_id != 0) {
        glDeleteTextures(1, &m_preview_texture_id);
        m_preview_texture_id = 0;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(m_window);
    glfwTerminate();
    m_window = nullptr;
}

void ProductionApp::run() {
    while (m_window && !glfwWindowShouldClose(m_window) && m_running) {
        glfwPollEvents();
        update_progress_estimate();
        upload_preview_texture_if_needed();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw_fixed_layout();
        ImGui::Render();

        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.045f, 0.048f, 0.055f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
}

void ProductionApp::draw_fixed_layout() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("ProductionRenderRoot", nullptr, flags);

    const ImVec2 root_size = ImGui::GetContentRegionAvail();
    constexpr float header_h = 48.0f;
    constexpr float bottom_h = 150.0f;
    constexpr float control_w = 330.0f;
    constexpr float gap = 10.0f;

    draw_header(ImVec2(root_size.x, header_h));
    ImGui::Separator();

    const float middle_h = std::max(100.0f, root_size.y - header_h - bottom_h - gap * 2.0f - 8.0f);
    ImGui::BeginChild("PreviewPanel", ImVec2(std::max(100.0f, root_size.x - control_w - gap), middle_h), true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    draw_preview_panel(ImGui::GetContentRegionAvail());
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);

    ImGui::BeginChild("ControlPanel", ImVec2(control_w, middle_h), true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    draw_control_panel(ImGui::GetContentRegionAvail());
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::BeginChild("ProgressPanel", ImVec2(root_size.x, bottom_h), true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    draw_progress_panel(ImGui::GetContentRegionAvail());
    ImGui::EndChild();

    ImGui::End();
}

void ProductionApp::draw_header(const ImVec2& size) {
    (void)size;
    const ImVec4 color = status_color();
    ImGui::TextColored(color, "*");
    ImGui::SameLine();
    ImGui::TextUnformatted(status_label());
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextUnformatted(std::filesystem::path(m_output_png_path).filename().string().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("target %d spp", m_target_spp);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("%dx%d", m_width, m_height);

    if (!m_last_event.empty()) {
        ImGui::TextDisabled("%s", m_last_event.c_str());
    }
}

void ProductionApp::draw_preview_panel(const ImVec2& size) {
    ImGui::TextUnformatted("Preview");
    const ImVec2 panel_min = ImGui::GetCursorScreenPos();
    const ImVec2 panel_size(size.x, std::max(1.0f, size.y - ImGui::GetTextLineHeightWithSpacing()));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(panel_min, ImVec2(panel_min.x + panel_size.x, panel_min.y + panel_size.y), IM_COL32(18, 18, 22, 255));

    if (m_preview_texture_id != 0 && m_width > 0 && m_height > 0) {
        const float scale = std::min(panel_size.x / static_cast<float>(m_width), panel_size.y / static_cast<float>(m_height));
        const ImVec2 image_size(static_cast<float>(m_width) * scale, static_cast<float>(m_height) * scale);
        const ImVec2 image_pos(panel_min.x + (panel_size.x - image_size.x) * 0.5f,
                               panel_min.y + (panel_size.y - image_size.y) * 0.5f);
        draw_list->AddImage(
            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(m_preview_texture_id)),
            image_pos,
            ImVec2(image_pos.x + image_size.x, image_pos.y + image_size.y));
        draw_list->AddRect(image_pos, ImVec2(image_pos.x + image_size.x, image_pos.y + image_size.y), IM_COL32(80, 85, 95, 255));
    }

    ImGui::Dummy(panel_size);
}

void ProductionApp::draw_control_panel(const ImVec2& size) {
    (void)size;
    const ProductionStatus s = status();
    const bool busy = s == ProductionStatus::Running || s == ProductionStatus::Stopping;
    const bool paused = s == ProductionStatus::Paused;
    const bool completed = s == ProductionStatus::Completed;
    const bool idle_like = s == ProductionStatus::Idle || s == ProductionStatus::Error;

    ImGui::TextUnformatted("Controls");
    ImGui::Separator();

    ImGui::BeginDisabled(busy);
    char scene_buf[512];
    std::snprintf(scene_buf, sizeof(scene_buf), "%s", m_scene_path.c_str());
    if (ImGui::InputText("Scene", scene_buf, sizeof(scene_buf))) {
        m_scene_path = scene_buf;
    }
    if (ImGui::Button("Load scene")) {
        load_scene_from_path(m_scene_path);
    }

    char state_buf[512];
    std::snprintf(state_buf, sizeof(state_buf), "%s", m_renderstate_path.c_str());
    if (ImGui::InputText("State", state_buf, sizeof(state_buf))) {
        m_renderstate_path = state_buf;
    }
    if (ImGui::Button("Load .renderstate")) {
        load_renderstate_from_path(m_renderstate_path);
    }

    char output_buf[512];
    std::snprintf(output_buf, sizeof(output_buf), "%s", m_output_png_path.c_str());
    if (ImGui::InputText("Output PNG", output_buf, sizeof(output_buf))) {
        m_output_png_path = output_buf;
        update_log_path_from_output();
        crashlog::set_path(m_log_path);
    }
    ImGui::TextDisabled("Log: %s", std::filesystem::path(m_log_path).filename().string().c_str());

    ImGui::Separator();
    ImGui::TextUnformatted("Output resolution");
    const bool resolution_locked = m_atomic_current_spp.load() > 0;
    ImGui::BeginDisabled(resolution_locked);
    bool lock_changed = ImGui::Checkbox("Lock aspect to scene bounds", &m_lock_aspect_to_scene_bounds);
    int new_width = m_width;
    int new_height = m_height;
    const bool width_changed = ImGui::InputInt("Width", &new_width);
    new_width = clamp_render_dimension(new_width);
    if (m_lock_aspect_to_scene_bounds && width_changed) {
        new_height = locked_height_for_width(new_width);
    }
    const bool height_changed = ImGui::InputInt("Height", &new_height);
    new_height = clamp_render_dimension(new_height);
    if (m_lock_aspect_to_scene_bounds && height_changed) {
        new_width = locked_width_for_height(new_height);
    }
    if (lock_changed && m_lock_aspect_to_scene_bounds) {
        new_height = locked_height_for_width(new_width);
    }
    if ((width_changed || height_changed || lock_changed) && (new_width != m_width || new_height != m_height)) {
        m_doc_settings.field_width = new_width;
        m_doc_settings.field_height = new_height;
        initialize_render_buffers(m_doc_settings);
        m_last_event = "Resolution changed";
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Scene aspect %.3f", scene_bounds_aspect());
    if (resolution_locked) {
        ImGui::TextDisabled("Resolution is locked for loaded/partial renders.");
    }
    if (!m_lock_aspect_to_scene_bounds) {
        const float image_aspect = static_cast<float>(std::max(1, m_width)) / static_cast<float>(std::max(1, m_height));
        if (std::abs(image_aspect - scene_bounds_aspect()) > 0.01f) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "Aspect mismatch may stretch output.");
        }
    }

    ImGui::InputInt("Target spp", &m_target_spp);
    m_target_spp = std::max(m_current_spp + 1, m_target_spp);
    ImGui::InputInt("Threads", &m_thread_count);
    m_thread_count = std::max(0, m_thread_count);
    ImGui::TextDisabled("Threads: 0 = auto");
    ImGui::EndDisabled();

    ImGui::Spacing();

    if (idle_like) {
        if (m_atomic_current_spp.load() > 0) {
            ImGui::TextDisabled("Partial state is loaded.");
            ImGui::InputInt("New target spp", &m_target_spp);
            m_target_spp = std::max(m_atomic_current_spp.load() + 1, m_target_spp);
            if (ImGui::Button("Continue to larger spp", ImVec2(-FLT_MIN, 36.0f))) {
                start_render(true);
            }
        } else if (ImGui::Button("Start", ImVec2(-FLT_MIN, 36.0f))) {
            start_render(false);
        }
    } else if (busy) {
        if (ImGui::Button("Pause", ImVec2(-FLT_MIN, 36.0f))) {
            request_pause();
        }
        if (ImGui::Button("Stop", ImVec2(-FLT_MIN, 32.0f))) {
            request_stop();
        }
        if (ImGui::Button("Save checkpoint", ImVec2(-FLT_MIN, 28.0f))) {
            m_checkpoint_requested.store(true);
        }
    } else if (paused) {
        if (ImGui::Button("Resume", ImVec2(-FLT_MIN, 36.0f))) {
            request_resume();
        }
        if (ImGui::Button("Stop", ImVec2(-FLT_MIN, 32.0f))) {
            request_stop();
        }
        if (ImGui::Button("Save checkpoint", ImVec2(-FLT_MIN, 28.0f))) {
            RenderJobSnapshot job;
            job.scene = m_scene;
            job.settings = m_doc_settings;
            job.settings.field_width = clamp_render_dimension(m_width);
            job.settings.field_height = clamp_render_dimension(m_height);
            job.scene_json_text = m_scene_json_text;
            job.scene_path = m_scene_path;
            job.output_png_path = m_output_png_path;
            job.renderstate_path = m_renderstate_path;
            job.target_spp = m_target_spp;
            job.thread_count = m_thread_count;
            save_renderstate_to_path(m_renderstate_path, job);
            m_last_event = "Checkpoint saved";
        }
    } else if (completed) {
        ImGui::TextDisabled("Completed output was saved automatically.");
        ImGui::InputInt("New target spp", &m_target_spp);
        m_target_spp = std::max(m_current_spp + 1, m_target_spp);
        if (ImGui::Button("Continue to larger spp", ImVec2(-FLT_MIN, 36.0f))) {
            start_render(true);
        }
    }

    if (!m_status_message.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", m_status_message.c_str());
        ImGui::TextDisabled("See log: %s", m_log_path.c_str());
    }
}

void ProductionApp::draw_progress_panel(const ImVec2& size) {
    const int current = m_atomic_current_spp.load();
    const int target = std::max(1, m_atomic_target_spp.load());
    const float progress = std::clamp(static_cast<float>(current) / static_cast<float>(target), 0.0f, 1.0f);

    ImGui::TextUnformatted("Progress");
    ImGui::ProgressBar(progress, ImVec2(size.x * 0.52f, 24.0f), "");
    ImGui::SameLine();
    ImGui::Text("%d / %d spp", current, target);
    ImGui::SameLine();
    ImGui::TextDisabled("ETA %s", format_duration(m_eta_sec).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", format_frame_rate(m_frames_per_sec).c_str());

    ImGui::Spacing();
    ImGui::TextUnformatted("Convergence");
    if (!m_convergence_values.empty()) {
        const float max_delta = std::max(1.0e-6f, *std::max_element(m_convergence_values.begin(), m_convergence_values.end()));
        ImGui::PlotLines("##Convergence", m_convergence_values.data(), static_cast<int>(m_convergence_values.size()), 0, nullptr, 0.0f, max_delta, ImVec2(size.x - 20.0f, 70.0f));
    } else {
        ImGui::Dummy(ImVec2(size.x - 20.0f, 70.0f));
    }
}

void ProductionApp::start_render(bool resume_existing_state) {
    if (status() == ProductionStatus::Running || status() == ProductionStatus::Stopping) {
        return;
    }
    if (m_worker.joinable()) {
        m_worker.join();
    }

    if (!resume_existing_state) {
        const int requested_width = clamp_render_dimension(m_width);
        const int requested_height = clamp_render_dimension(m_height);
        if (!load_scene_from_path(m_scene_path)) {
            return;
        }
        m_doc_settings.field_width = requested_width;
        if (m_lock_aspect_to_scene_bounds) {
            m_doc_settings.field_height = locked_height_for_width(requested_width);
        } else {
            m_doc_settings.field_height = requested_height;
        }
        initialize_render_buffers(m_doc_settings);
    }

    m_doc_settings.field_width = clamp_render_dimension(m_width);
    m_doc_settings.field_height = clamp_render_dimension(m_height);
    m_target_spp = std::max(m_current_spp + 1, m_target_spp);

    RenderJobSnapshot job;
    job.scene = m_scene;
    job.settings = m_doc_settings;
    job.settings.field_width = clamp_render_dimension(m_width);
    job.settings.field_height = clamp_render_dimension(m_height);
    job.scene_json_text = m_scene_json_text;
    job.scene_path = m_scene_path;
    job.output_png_path = m_output_png_path;
    job.renderstate_path = m_renderstate_path;
    job.target_spp = m_target_spp;
    job.thread_count = m_thread_count;
    job.resume_existing_state = resume_existing_state;

    m_stop_requested.store(false);
    m_pause_requested.store(false);
    m_checkpoint_requested.store(false);
    m_atomic_target_spp.store(m_target_spp);
    set_status(ProductionStatus::Running);
    m_render_start_time = std::chrono::steady_clock::now();
    m_last_progress_time = m_render_start_time;
    m_last_estimate_spp = m_atomic_current_spp.load();
    m_estimate_start_spp = m_last_estimate_spp;
    m_eta_sec = 0.0;
    m_frames_per_sec = 0.0;
    update_log_path_from_output();
    crashlog::set_path(m_log_path);
    {
        std::ostringstream oss;
        oss << (resume_existing_state ? "Continue render" : "Start render")
            << " scene=" << job.scene_path
            << " output=" << job.output_png_path
            << " state=" << job.renderstate_path
            << " resolution=" << job.settings.field_width << "x" << job.settings.field_height
            << " current_spp=" << m_atomic_current_spp.load()
            << " target_spp=" << job.target_spp
            << " threads=" << job.thread_count
            << " integrator=" << static_cast<int>(job.settings.integrator.kind);
        write_log(oss.str());
    }
    m_last_event = resume_existing_state ? "Continuing render" : "Started render";

    m_worker = std::thread(&ProductionApp::render_worker, this, std::move(job));
}

void ProductionApp::request_pause() {
    if (status() == ProductionStatus::Running) {
        m_pause_requested.store(true);
    }
}

void ProductionApp::request_resume() {
    if (status() == ProductionStatus::Paused) {
        m_pause_requested.store(false);
        set_status(ProductionStatus::Running);
        m_last_event = "Resumed";
    }
}

void ProductionApp::request_stop() {
    if (status() == ProductionStatus::Running || status() == ProductionStatus::Paused) {
        set_status(ProductionStatus::Stopping);
        m_stop_requested.store(true);
        m_pause_requested.store(false);
        m_last_event = "Stopping";
    }
}

void ProductionApp::render_worker(RenderJobSnapshot job) {
#ifdef _OPENMP
    if (job.thread_count > 0) {
        omp_set_num_threads(job.thread_count);
    }
#else
    (void)job.thread_count;
#endif

    write_log("Render worker started");

    auto last_preview = std::chrono::steady_clock::now();
    auto last_checkpoint = std::chrono::steady_clock::now();

    try {
        while (m_atomic_current_spp.load() < job.target_spp) {
            if (m_stop_requested.load()) {
                break;
            }

            if (m_pause_requested.load()) {
                set_status(ProductionStatus::Paused);
                while (m_pause_requested.load() && !m_stop_requested.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(40));
                }
                if (m_stop_requested.load()) {
                    break;
                }
                set_status(ProductionStatus::Running);
            }

            const int sample_index = m_atomic_current_spp.load();

            PhotonMap photon_map;
            const PhotonMap* photon_map_ptr = nullptr;
            if (job.settings.integrator.kind == IntegratorKind::PhotonMapping) {
                const uint64_t photon_seed = hash_combine(sample_seed_base(job.settings.integrator), static_cast<uint64_t>(sample_index) ^ 0x70686f746f6eULL);
                photon_map = build_photon_map(job.scene, job.settings.integrator, photon_seed);
                photon_map_ptr = &photon_map;
            }

            {
                std::lock_guard<std::mutex> lock(m_data_mutex);
                ensure_ris_directions(job.settings.integrator);
            }

            const int width = m_width;
            const int height = m_height;
            const Vec2 bounds_min = m_bounds_min;
            const Vec2 bounds_max = m_bounds_max;
            std::vector<Color>& accum = m_accum;
            std::vector<RISDirection>& ris = m_ris_directions;
            const bool use_ris = job.settings.integrator.kind == IntegratorKind::RISDirection;

            std::atomic<bool> pixel_error{false};
            std::string pixel_error_message;
            std::mutex pixel_error_mutex;

#pragma omp parallel for schedule(dynamic)
            for (int index = 0; index < width * height; ++index) {
                if (pixel_error.load(std::memory_order_relaxed)) {
                    continue;
                }
                try {
                    const int x = index % width;
                    const int y = index / width;
                    Sampler sampler(seed_for_pixel(x, y, sample_index, job.settings.integrator));
                    const Vec2 position = bounds_sample_position(bounds_min, bounds_max, x, y, width, height);
                    RISDirection* ris_ptr = use_ris ? &ris[static_cast<std::size_t>(index)] : nullptr;
                    const Color contribution = estimate_at(job.scene, position, sampler, job.settings.integrator, nullptr, photon_map_ptr, ris_ptr);
                    accum[static_cast<std::size_t>(index)] += contribution;
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(pixel_error_mutex);
                    if (!pixel_error.load(std::memory_order_relaxed)) {
                        std::ostringstream oss;
                        oss << "Pixel render exception at sample=" << sample_index
                            << " index=" << index
                            << " message=" << e.what();
                        pixel_error_message = oss.str();
                        pixel_error.store(true, std::memory_order_relaxed);
                    }
                } catch (...) {
                    std::lock_guard<std::mutex> lock(pixel_error_mutex);
                    if (!pixel_error.load(std::memory_order_relaxed)) {
                        std::ostringstream oss;
                        oss << "Unknown pixel render exception at sample=" << sample_index
                            << " index=" << index;
                        pixel_error_message = oss.str();
                        pixel_error.store(true, std::memory_order_relaxed);
                    }
                }
            }

            if (pixel_error.load()) {
                throw std::runtime_error(pixel_error_message.empty() ? "Pixel render failed" : pixel_error_message);
            }

            const int next_spp = sample_index + 1;
            m_atomic_current_spp.store(next_spp);
            m_current_spp = next_spp;

            const auto now = std::chrono::steady_clock::now();
            const bool preview_due = (now - last_preview) >= std::chrono::seconds(1) || next_spp == job.target_spp;
            if (preview_due) {
                std::lock_guard<std::mutex> lock(m_data_mutex);
                update_preview_from_accum_locked(true);
                last_preview = now;
                if (next_spp == job.target_spp || next_spp % 10 == 0) {
                    std::ostringstream oss;
                    oss << "Progress spp=" << next_spp << "/" << job.target_spp;
                    write_log(oss.str());
                }
            }

            if (m_checkpoint_requested.exchange(false) || (now - last_checkpoint) >= std::chrono::minutes(5)) {
                write_log("Saving checkpoint: " + job.renderstate_path);
                if (!save_renderstate_to_path(job.renderstate_path, job)) {
                    write_log("Checkpoint save failed");
                } else {
                    write_log("Checkpoint saved");
                }
                last_checkpoint = now;
                m_last_event = "Checkpoint saved";
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_data_mutex);
            update_preview_from_accum_locked(true);
        }

        write_log("Saving final PNG: " + job.output_png_path);
        if (!save_current_png_to_path(job.output_png_path)) {
            write_log("Final PNG save failed");
        } else {
            write_log("Final PNG saved");
        }
        write_log("Saving final renderstate: " + job.renderstate_path);
        if (!save_renderstate_to_path(job.renderstate_path, job)) {
            write_log("Final renderstate save failed");
        } else {
            write_log("Final renderstate saved");
        }

        if (m_stop_requested.load()) {
            set_status(ProductionStatus::Idle);
            m_last_event = "Stopped, partial state saved";
        } else {
            set_status(ProductionStatus::Completed);
            m_last_event = "Completed and saved";
        }
    } catch (const std::exception& e) {
        m_status_message = std::string("Render error: ") + e.what();
        write_log(m_status_message);
        set_status(ProductionStatus::Error);
    } catch (...) {
        m_status_message = "Unknown render error";
        write_log(m_status_message);
        set_status(ProductionStatus::Error);
    }
    write_log("Render worker finished with status=" + std::string(status_label()));
}

bool ProductionApp::load_scene_from_path(const std::string& path) {
    if (path.empty()) {
        m_status_message = "Scene path is empty";
        write_log(m_status_message);
        set_status(ProductionStatus::Error);
        return false;
    }

    Scene scene;
    SceneDocumentSettings settings = m_doc_settings;
    std::string error;
    write_log("Loading scene: " + path);
    if (!load_scene_json(path, scene, settings, &error)) {
        m_status_message = error.empty() ? "Failed to load scene" : error;
        write_log("Scene load failed: " + m_status_message);
        set_status(ProductionStatus::Error);
        return false;
    }

    std::string scene_text;
    if (!read_file_text(path, scene_text)) {
        m_status_message = "Loaded scene, but failed to keep JSON text for renderstate";
        scene_text.clear();
    }

    m_scene = std::move(scene);
    m_doc_settings = settings;
    m_bounds_min = m_doc_settings.field_bounds_min;
    m_bounds_max = m_doc_settings.field_bounds_max;
    apply_aspect_lock_to_settings(m_doc_settings, true);
    m_scene_json_text = std::move(scene_text);
    m_scene_path = path;
    m_status_message.clear();
    m_last_event = "Scene loaded";
    write_log("Scene loaded");
    initialize_render_buffers(m_doc_settings);
    set_status(ProductionStatus::Idle);
    return true;
}

bool ProductionApp::load_renderstate_from_path(const std::string& path) {
    if (path.empty()) {
        m_status_message = "Renderstate path is empty";
        write_log(m_status_message);
        set_status(ProductionStatus::Error);
        return false;
    }

    write_log("Loading renderstate: " + path);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        m_status_message = "Could not open renderstate: " + path;
        set_status(ProductionStatus::Error);
        return false;
    }

    char magic[8]{};
    in.read(magic, sizeof(magic));
    if (std::memcmp(magic, kRenderStateMagic, sizeof(magic)) != 0) {
        m_status_message = "Invalid .renderstate magic";
        set_status(ProductionStatus::Error);
        return false;
    }

    std::uint32_t version = 0;
    if (!read_pod(in, version) || version != kRenderStateVersion) {
        m_status_message = "Unsupported .renderstate version";
        set_status(ProductionStatus::Error);
        return false;
    }

    std::string scene_text;
    std::string scene_path;
    std::string output_png;
    std::string state_path;
    if (!read_string(in, scene_text) || !read_string(in, scene_path) || !read_string(in, output_png) || !read_string(in, state_path)) {
        m_status_message = "Failed to read .renderstate strings";
        set_status(ProductionStatus::Error);
        return false;
    }

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t current_spp = 0;
    std::uint32_t target_spp = 0;
    std::uint32_t ris_bins = 0;
    if (!read_pod(in, width) || !read_pod(in, height) || !read_pod(in, current_spp) || !read_pod(in, target_spp) || !read_pod(in, ris_bins)) {
        m_status_message = "Failed to read .renderstate header";
        set_status(ProductionStatus::Error);
        return false;
    }

    if (width == 0 || height == 0 || width > 16384 || height > 16384) {
        m_status_message = "Invalid .renderstate resolution";
        set_status(ProductionStatus::Error);
        return false;
    }

    const std::filesystem::path temp_path = temp_scene_path_from_state();
    {
        std::ofstream out(temp_path, std::ios::binary);
        out << scene_text;
    }

    Scene scene;
    SceneDocumentSettings settings;
    std::string error;
    const bool loaded_scene = load_scene_json(temp_path.string(), scene, settings, &error);
    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
    if (!loaded_scene) {
        m_status_message = "Failed to load scene inside renderstate: " + error;
        set_status(ProductionStatus::Error);
        return false;
    }

    std::vector<Color> accum(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (Color& c : accum) {
        if (!read_pod(in, c.r) || !read_pod(in, c.g) || !read_pod(in, c.b)) {
            m_status_message = "Failed to read accum buffer";
            set_status(ProductionStatus::Error);
            return false;
        }
    }

    std::vector<std::vector<float>> ris_scores;
    if (settings.integrator.kind == IntegratorKind::RISDirection) {
        if (ris_bins == 0 || ris_bins > 4096) {
            m_status_message = "Invalid RIS state in .renderstate";
            set_status(ProductionStatus::Error);
            return false;
        }
        ris_scores.resize(accum.size());
        for (std::vector<float>& scores : ris_scores) {
            scores.resize(ris_bins);
            for (float& score : scores) {
                if (!read_pod(in, score)) {
                    m_status_message = "Failed to read RIS scores";
                    set_status(ProductionStatus::Error);
                    return false;
                }
            }
        }
    }

    m_scene = std::move(scene);
    m_doc_settings = settings;
    m_scene_json_text = std::move(scene_text);
    m_scene_path = scene_path;
    m_output_png_path = output_png.empty() ? m_output_png_path : output_png;
    m_renderstate_path = path;
    m_width = static_cast<int>(width);
    m_height = static_cast<int>(height);
    m_doc_settings.field_width = m_width;
    m_doc_settings.field_height = m_height;
    m_bounds_min = settings.field_bounds_min;
    m_bounds_max = settings.field_bounds_max;
    m_current_spp = static_cast<int>(current_spp);
    m_target_spp = std::max(static_cast<int>(target_spp), m_current_spp + 1);
    m_atomic_current_spp.store(m_current_spp);
    m_atomic_target_spp.store(m_target_spp);
    m_last_estimate_spp = m_current_spp;
    m_estimate_start_spp = m_current_spp;
    m_eta_sec = 0.0;
    m_frames_per_sec = 0.0;
    m_accum = std::move(accum);
    m_last_convergence_average.clear();
    m_convergence_values.clear();

    m_ris_directions.clear();
    if (settings.integrator.kind == IntegratorKind::RISDirection) {
        ensure_ris_directions(settings.integrator);
        for (std::size_t i = 0; i < std::min(m_ris_directions.size(), ris_scores.size()); ++i) {
            m_ris_directions[i].set_scores(ris_scores[i]);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        update_preview_from_accum_locked(false);
    }

    set_status(ProductionStatus::Idle);
    m_status_message.clear();
    m_last_event = "Renderstate loaded";
    write_log("Renderstate loaded");
    return true;
}

bool ProductionApp::save_renderstate_to_path(const std::string& path, const RenderJobSnapshot& job) {
    if (path.empty()) {
        write_log("Renderstate save failed: empty path");
        return false;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        m_status_message = "Could not write renderstate: " + path;
        write_log(m_status_message);
        return false;
    }

    out.write(kRenderStateMagic, sizeof(kRenderStateMagic));
    if (!write_pod(out, kRenderStateVersion)) return false;
    if (!write_string(out, job.scene_json_text)) return false;
    if (!write_string(out, job.scene_path)) return false;
    if (!write_string(out, job.output_png_path)) return false;
    if (!write_string(out, path)) return false;

    const std::uint32_t width = static_cast<std::uint32_t>(m_width);
    const std::uint32_t height = static_cast<std::uint32_t>(m_height);
    const std::uint32_t current_spp = static_cast<std::uint32_t>(std::max(0, m_atomic_current_spp.load()));
    const std::uint32_t target_spp = static_cast<std::uint32_t>(std::max(job.target_spp, m_atomic_current_spp.load()));
    const std::uint32_t ris_bins = job.settings.integrator.kind == IntegratorKind::RISDirection
        ? static_cast<std::uint32_t>(std::max(0, job.settings.integrator.ris_direction.num_bins))
        : 0u;
    if (!write_pod(out, width) || !write_pod(out, height) || !write_pod(out, current_spp) || !write_pod(out, target_spp) || !write_pod(out, ris_bins)) {
        return false;
    }

    for (const Color& c : m_accum) {
        if (!write_pod(out, c.r) || !write_pod(out, c.g) || !write_pod(out, c.b)) {
            return false;
        }
    }

    if (job.settings.integrator.kind == IntegratorKind::RISDirection) {
        for (const RISDirection& ris : m_ris_directions) {
            const std::vector<float>& scores = ris.scores();
            for (std::uint32_t i = 0; i < ris_bins; ++i) {
                const float score = i < scores.size() ? scores[static_cast<std::size_t>(i)] : 0.0f;
                if (!write_pod(out, score)) {
                    return false;
                }
            }
        }
    }

    return static_cast<bool>(out);
}

bool ProductionApp::save_current_png_to_path(const std::string& path) {
    if (path.empty()) {
        write_log("PNG save failed: empty path");
        return false;
    }
    std::vector<unsigned char> rgba;
    {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        make_rgba_from_accum(m_accum, m_atomic_current_spp.load(), m_width, m_height, rgba);
    }
    const bool ok = write_png_rgba8(path.c_str(), m_width, m_height, rgba.data());
    if (!ok) {
        m_status_message = "Failed to save PNG: " + path;
    }
    return ok;
}

void ProductionApp::initialize_render_buffers(const SceneDocumentSettings& settings) {
    m_width = std::max(1, settings.field_width);
    m_height = std::max(1, settings.field_height);
    m_bounds_min = settings.field_bounds_min;
    m_bounds_max = settings.field_bounds_max;
    m_current_spp = 0;
    m_atomic_current_spp.store(0);
    m_atomic_target_spp.store(std::max(1, m_target_spp));
    m_last_estimate_spp = 0;
    m_estimate_start_spp = 0;
    m_eta_sec = 0.0;
    m_frames_per_sec = 0.0;
    m_accum.assign(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height), make_color(0.0f));
    m_last_convergence_average.clear();
    m_convergence_values.clear();
    m_ris_directions.clear();
    if (settings.integrator.kind == IntegratorKind::RISDirection) {
        ensure_ris_directions(settings.integrator);
    }
    update_preview_from_accum_locked(false);
}

void ProductionApp::ensure_ris_directions(const IntegratorSettings& settings) {
    if (settings.kind != IntegratorKind::RISDirection) {
        m_ris_directions.clear();
        return;
    }

    const std::size_t pixel_count = static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    if (m_ris_directions.size() == pixel_count) {
        return;
    }

    m_ris_directions.clear();
    m_ris_directions.reserve(pixel_count);
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const uint64_t ris_seed = seed_for_pixel(x, y, 0, settings) ^ 0x7269736469726563ULL;
            m_ris_directions.emplace_back(
                settings.ris_direction.num_bins,
                settings.ris_direction.min_probability_percent,
                settings.ris_direction.smooth_sigma_deg,
                ris_seed);
        }
    }
}

uint64_t ProductionApp::seed_for_pixel(int x, int y, int sample_index, const IntegratorSettings& settings) const {
    uint64_t seed = sample_seed_base(settings);
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, x)));
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, y)));
    seed = hash_combine(seed, static_cast<uint64_t>(std::max(0, sample_index)));
    return seed;
}

Vec2 ProductionApp::pixel_center_to_world(int x, int y) const {
    return bounds_sample_position(m_bounds_min, m_bounds_max, x, y, m_width, m_height);
}

void ProductionApp::update_preview_from_accum_locked(bool update_convergence) {
    const int samples = m_atomic_current_spp.load();
    make_rgba_from_accum(m_accum, samples, m_width, m_height, m_preview_rgba);
    m_preview_dirty = true;

    if (!update_convergence || samples <= 0) {
        return;
    }

    std::vector<Color> current_average(m_accum.size(), make_color(0.0f));
    const float inv_samples = 1.0f / static_cast<float>(samples);
    for (std::size_t i = 0; i < m_accum.size(); ++i) {
        current_average[i] = m_accum[i] * inv_samples;
    }

    if (!m_last_convergence_average.empty() && m_last_convergence_average.size() == current_average.size()) {
        double delta = 0.0;
        for (std::size_t i = 0; i < current_average.size(); ++i) {
            delta += std::abs(luminance(current_average[i]) - luminance(m_last_convergence_average[i]));
        }
        delta /= static_cast<double>(std::max<std::size_t>(1, current_average.size()));
        m_convergence_values.push_back(static_cast<float>(delta));
        if (m_convergence_values.size() > 256) {
            m_convergence_values.erase(m_convergence_values.begin(), m_convergence_values.begin() + static_cast<std::ptrdiff_t>(m_convergence_values.size() - 256));
        }
    }
    m_last_convergence_average = std::move(current_average);
}

void ProductionApp::upload_preview_texture_if_needed() {
    std::vector<unsigned char> rgba;
    int width = 0;
    int height = 0;
    {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        if (!m_preview_dirty || m_preview_rgba.empty()) {
            return;
        }
        rgba = m_preview_rgba;
        width = m_width;
        height = m_height;
        m_preview_dirty = false;
    }

    glBindTexture(GL_TEXTURE_2D, m_preview_texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}

void ProductionApp::update_progress_estimate() {
    const ProductionStatus s = status();
    if (s != ProductionStatus::Running && s != ProductionStatus::Paused && s != ProductionStatus::Stopping) {
        if (s == ProductionStatus::Completed) {
            m_eta_sec = 0.0;
        }
        m_frames_per_sec = 0.0;
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const int current = m_atomic_current_spp.load();
    if (current == m_last_estimate_spp) {
        return;
    }
    m_last_estimate_spp = current;
    m_elapsed_sec = std::chrono::duration<double>(now - m_render_start_time).count();
    const int target = std::max(1, m_atomic_target_spp.load());
    const int completed_since_start = std::max(0, current - m_estimate_start_spp);
    if (completed_since_start > 0 && m_elapsed_sec > 0.0) {
        const double spp_per_sec = static_cast<double>(completed_since_start) / m_elapsed_sec;
        m_frames_per_sec = spp_per_sec;
        m_eta_sec = spp_per_sec > 0.0 ? static_cast<double>(std::max(0, target - current)) / spp_per_sec : 0.0;
    } else {
        m_frames_per_sec = 0.0;
        m_eta_sec = 0.0;
    }
}


float ProductionApp::scene_bounds_aspect() const {
    const float w = std::abs(m_bounds_max.x - m_bounds_min.x);
    const float h = std::abs(m_bounds_max.y - m_bounds_min.y);
    if (w <= 1.0e-6f || h <= 1.0e-6f) {
        return 1.0f;
    }
    return w / h;
}

int ProductionApp::locked_height_for_width(int width) const {
    const float aspect = scene_bounds_aspect();
    if (aspect <= 1.0e-6f) {
        return clamp_render_dimension(width);
    }
    return clamp_render_dimension(static_cast<int>(std::lround(static_cast<float>(clamp_render_dimension(width)) / aspect)));
}

int ProductionApp::locked_width_for_height(int height) const {
    const float aspect = scene_bounds_aspect();
    return clamp_render_dimension(static_cast<int>(std::lround(static_cast<float>(clamp_render_dimension(height)) * aspect)));
}

void ProductionApp::apply_aspect_lock_to_settings(SceneDocumentSettings& settings, bool preserve_width) const {
    if (!m_lock_aspect_to_scene_bounds) {
        settings.field_width = clamp_render_dimension(settings.field_width);
        settings.field_height = clamp_render_dimension(settings.field_height);
        return;
    }

    if (preserve_width) {
        settings.field_width = clamp_render_dimension(settings.field_width);
        settings.field_height = locked_height_for_width(settings.field_width);
    } else {
        settings.field_height = clamp_render_dimension(settings.field_height);
        settings.field_width = locked_width_for_height(settings.field_height);
    }
}

void ProductionApp::update_log_path_from_output() {
    m_log_path = default_log_path_for_output(m_output_png_path);
}

void ProductionApp::write_log(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_log_mutex);
    if (m_log_path.empty()) {
        m_log_path = "production_output.log";
    }
    std::ofstream out(m_log_path, std::ios::app | std::ios::binary);
    if (!out) {
        return;
    }
    out << "[" << timestamp_now_string() << "] " << message << "\n";
    out.flush();
}

ProductionStatus ProductionApp::status() const {
    return static_cast<ProductionStatus>(m_status.load());
}

void ProductionApp::set_status(ProductionStatus status) {
    m_status.store(static_cast<int>(status));
}

const char* ProductionApp::status_label() const {
    switch (status()) {
        case ProductionStatus::Idle: return "Idle";
        case ProductionStatus::Running: return "Running";
        case ProductionStatus::Paused: return "Paused";
        case ProductionStatus::Stopping: return "Stopping";
        case ProductionStatus::Completed: return "Completed";
        case ProductionStatus::Error: return "Error";
    }
    return "Unknown";
}

ImVec4 ProductionApp::status_color() const {
    switch (status()) {
        case ProductionStatus::Idle: return ImVec4(0.55f, 0.62f, 0.70f, 1.0f);
        case ProductionStatus::Running: return ImVec4(0.30f, 0.90f, 0.45f, 1.0f);
        case ProductionStatus::Paused: return ImVec4(1.00f, 0.75f, 0.30f, 1.0f);
        case ProductionStatus::Stopping: return ImVec4(1.00f, 0.50f, 0.30f, 1.0f);
        case ProductionStatus::Completed: return ImVec4(0.35f, 0.72f, 1.00f, 1.0f);
        case ProductionStatus::Error: return ImVec4(1.00f, 0.30f, 0.30f, 1.0f);
    }
    return ImVec4(1, 1, 1, 1);
}

} // namespace pt2d
