#include "pt2d/SceneSerializer.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>
#include <filesystem>

namespace pt2d {
namespace {

const char* material_kind_to_string(MaterialKind kind) {
    switch (kind) {
        case MaterialKind::Diffuse: return "diffuse";
        case MaterialKind::Dielectric: return "dielectric";
    }
    return "diffuse";
}

MaterialKind material_kind_from_string(const std::string& s) {
    if (s == "dielectric" || s == "Dielectric") {
        return MaterialKind::Dielectric;
    }
    return MaterialKind::Diffuse;
}

const char* integrator_kind_to_string(IntegratorKind kind) {
    switch (kind) {
        case IntegratorKind::PathTracing: return "path_tracing";
        case IntegratorKind::PathTracingNEE: return "path_tracing_nee";
        case IntegratorKind::PhotonMapping: return "photon_mapping";
        case IntegratorKind::RISDirection: return "ris_direction";
    }
    return "path_tracing";
}

IntegratorKind integrator_kind_from_string(const std::string& s) {
    if (s == "path_tracing_nee" || s == "PathTracingNEE") {
        return IntegratorKind::PathTracingNEE;
    }
    if (s == "photon_mapping" || s == "PhotonMapping") {
        return IntegratorKind::PhotonMapping;
    }
    if (s == "ris_direction" || s == "RISDirection") {
        return IntegratorKind::RISDirection;
    }
    return IntegratorKind::PathTracing;
}

std::string escape_json_string(const std::string& s) {
    std::ostringstream out;
    for (char ch : s) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch))
                        << std::dec << std::setfill(' ');
                } else {
                    out << ch;
                }
        }
    }
    return out.str();
}

void write_vec2(std::ostream& os, Vec2 v) {
    os << '[' << v.x << ", " << v.y << ']';
}

void write_color(std::ostream& os, Color c) {
    os << '[' << c.r << ", " << c.g << ", " << c.b << ']';
}

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool bool_value = false;
    double number_value = 0.0;
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::map<std::string, JsonValue> object_value;

    const JsonValue* get(const std::string& key) const {
        if (type != Type::Object) return nullptr;
        auto it = object_value.find(key);
        return it == object_value.end() ? nullptr : &it->second;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    bool parse(JsonValue& out, std::string* error) {
        skip_ws();
        if (!parse_value(out, error)) return false;
        skip_ws();
        if (pos_ != text_.size()) {
            set_error(error, "Unexpected trailing characters");
            return false;
        }
        return true;
    }

private:
    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    bool consume(char ch) {
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == ch) {
            ++pos_;
            return true;
        }
        return false;
    }

    void set_error(std::string* error, const std::string& message) const {
        if (error) {
            std::ostringstream out;
            out << message << " at byte " << pos_;
            *error = out.str();
        }
    }

    bool parse_value(JsonValue& out, std::string* error) {
        skip_ws();
        if (pos_ >= text_.size()) {
            set_error(error, "Unexpected end of JSON");
            return false;
        }
        const char ch = text_[pos_];
        if (ch == '{') return parse_object(out, error);
        if (ch == '[') return parse_array(out, error);
        if (ch == '"') return parse_string_value(out, error);
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) return parse_number(out, error);
        if (match_literal("true")) {
            out.type = JsonValue::Type::Bool;
            out.bool_value = true;
            return true;
        }
        if (match_literal("false")) {
            out.type = JsonValue::Type::Bool;
            out.bool_value = false;
            return true;
        }
        if (match_literal("null")) {
            out.type = JsonValue::Type::Null;
            return true;
        }
        set_error(error, "Unexpected JSON token");
        return false;
    }

    bool match_literal(const char* literal) {
        const std::string s(literal);
        if (text_.compare(pos_, s.size(), s) == 0) {
            pos_ += s.size();
            return true;
        }
        return false;
    }

    bool parse_object(JsonValue& out, std::string* error) {
        if (!consume('{')) {
            set_error(error, "Expected object");
            return false;
        }
        out = JsonValue{};
        out.type = JsonValue::Type::Object;
        skip_ws();
        if (consume('}')) return true;

        while (true) {
            JsonValue key;
            if (!parse_string_value(key, error)) return false;
            if (!consume(':')) {
                set_error(error, "Expected ':' after object key");
                return false;
            }
            JsonValue value;
            if (!parse_value(value, error)) return false;
            out.object_value[key.string_value] = std::move(value);
            if (consume('}')) return true;
            if (!consume(',')) {
                set_error(error, "Expected ',' or '}' in object");
                return false;
            }
        }
    }

    bool parse_array(JsonValue& out, std::string* error) {
        if (!consume('[')) {
            set_error(error, "Expected array");
            return false;
        }
        out = JsonValue{};
        out.type = JsonValue::Type::Array;
        skip_ws();
        if (consume(']')) return true;

        while (true) {
            JsonValue value;
            if (!parse_value(value, error)) return false;
            out.array_value.push_back(std::move(value));
            if (consume(']')) return true;
            if (!consume(',')) {
                set_error(error, "Expected ',' or ']' in array");
                return false;
            }
        }
    }

    bool parse_string_value(JsonValue& out, std::string* error) {
        skip_ws();
        if (pos_ >= text_.size() || text_[pos_] != '"') {
            set_error(error, "Expected string");
            return false;
        }
        ++pos_;
        std::string result;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                out = JsonValue{};
                out.type = JsonValue::Type::String;
                out.string_value = std::move(result);
                return true;
            }
            if (ch == '\\') {
                if (pos_ >= text_.size()) {
                    set_error(error, "Invalid escape");
                    return false;
                }
                const char esc = text_[pos_++];
                switch (esc) {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    case 'u':
                        // Keep the parser small: consume \uXXXX and map ASCII range, otherwise '?'.
                        if (pos_ + 4 > text_.size()) {
                            set_error(error, "Invalid unicode escape");
                            return false;
                        } else {
                            int value = 0;
                            for (int i = 0; i < 4; ++i) {
                                const char h = text_[pos_++];
                                value <<= 4;
                                if (h >= '0' && h <= '9') value += h - '0';
                                else if (h >= 'a' && h <= 'f') value += 10 + h - 'a';
                                else if (h >= 'A' && h <= 'F') value += 10 + h - 'A';
                                else {
                                    set_error(error, "Invalid unicode hex digit");
                                    return false;
                                }
                            }
                            result.push_back(value >= 0 && value < 128 ? static_cast<char>(value) : '?');
                        }
                        break;
                    default:
                        set_error(error, "Unknown escape sequence");
                        return false;
                }
            } else {
                result.push_back(ch);
            }
        }
        set_error(error, "Unterminated string");
        return false;
    }

    bool parse_number(JsonValue& out, std::string* error) {
        skip_ws();
        const char* start = text_.c_str() + pos_;
        char* end = nullptr;
        errno = 0;
        const double value = std::strtod(start, &end);
        if (end == start || errno == ERANGE) {
            set_error(error, "Invalid number");
            return false;
        }
        pos_ += static_cast<std::size_t>(end - start);
        out = JsonValue{};
        out.type = JsonValue::Type::Number;
        out.number_value = value;
        return true;
    }

    std::string text_;
    std::size_t pos_ = 0;
};

bool read_number(const JsonValue& object, const std::string& key, float& out) {
    const JsonValue* v = object.get(key);
    if (!v || v->type != JsonValue::Type::Number) return false;
    out = static_cast<float>(v->number_value);
    return true;
}

bool read_number(const JsonValue& object, const std::string& key, int& out) {
    const JsonValue* v = object.get(key);
    if (!v || v->type != JsonValue::Type::Number) return false;
    out = static_cast<int>(v->number_value);
    return true;
}

bool read_number(const JsonValue& object, const std::string& key, std::uint64_t& out) {
    const JsonValue* v = object.get(key);
    if (!v || v->type != JsonValue::Type::Number) return false;
    out = static_cast<std::uint64_t>(v->number_value);
    return true;
}

bool read_string(const JsonValue& object, const std::string& key, std::string& out) {
    const JsonValue* v = object.get(key);
    if (!v || v->type != JsonValue::Type::String) return false;
    out = v->string_value;
    return true;
}

bool read_bool(const JsonValue& object, const std::string& key, bool& out) {
    const JsonValue* v = object.get(key);
    if (!v || v->type != JsonValue::Type::Bool) return false;
    out = v->bool_value;
    return true;
}

bool read_vec2(const JsonValue& object, const std::string& key, Vec2& out) {
    const JsonValue* v = object.get(key);
    if (!v || v->type != JsonValue::Type::Array || v->array_value.size() != 2) return false;
    if (v->array_value[0].type != JsonValue::Type::Number || v->array_value[1].type != JsonValue::Type::Number) return false;
    out.x = static_cast<float>(v->array_value[0].number_value);
    out.y = static_cast<float>(v->array_value[1].number_value);
    return true;
}

bool read_color(const JsonValue& object, const std::string& key, Color& out) {
    const JsonValue* v = object.get(key);
    if (!v || v->type != JsonValue::Type::Array || v->array_value.size() != 3) return false;
    if (v->array_value[0].type != JsonValue::Type::Number ||
        v->array_value[1].type != JsonValue::Type::Number ||
        v->array_value[2].type != JsonValue::Type::Number) return false;
    out.r = static_cast<float>(v->array_value[0].number_value);
    out.g = static_cast<float>(v->array_value[1].number_value);
    out.b = static_cast<float>(v->array_value[2].number_value);
    return true;
}

void write_indent(std::ostream& os, int indent) {
    for (int i = 0; i < indent; ++i) {
        os << ' ';
    }
}

void write_material(std::ostream& os, const Material& m, int indent) {
    const int inner = indent + 2;
    os << "{\n";
    write_indent(os, inner); os << "\"kind\": \"" << material_kind_to_string(m.kind) << "\",\n";
    write_indent(os, inner); os << "\"albedo\": "; write_color(os, m.albedo); os << ",\n";
    write_indent(os, inner); os << "\"emission\": "; write_color(os, m.emission); os << ",\n";
    write_indent(os, inner); os << "\"ior\": " << m.ior << ",\n";
    write_indent(os, inner); os << "\"cauchy_a\": " << m.cauchy_a << ",\n";
    write_indent(os, inner); os << "\"cauchy_b\": " << m.cauchy_b << ",\n";
    write_indent(os, inner); os << "\"emission_angle_deg\": " << m.emission_angle_deg << "\n";
    write_indent(os, indent); os << "}";
}

bool read_material_object(const JsonValue& value, Material& material, std::string* error_message) {
    if (value.type != JsonValue::Type::Object) {
        if (error_message) *error_message = "material must be an object";
        return false;
    }

    std::string kind;
    read_string(value, "name", material.name);
    if (read_string(value, "kind", kind)) {
        material.kind = material_kind_from_string(kind);
    }
    read_color(value, "albedo", material.albedo);
    read_color(value, "emission", material.emission);
    read_number(value, "ior", material.ior);
    const bool has_cauchy_a = read_number(value, "cauchy_a", material.cauchy_a);
    const bool has_cauchy_b = read_number(value, "cauchy_b", material.cauchy_b);
    read_number(value, "emission_angle_deg", material.emission_angle_deg);
    material.ior = std::max(1.0f, material.ior);
    if (!has_cauchy_a) {
        material.cauchy_a = material.ior;
    }
    if (!has_cauchy_b) {
        material.cauchy_b = 0.0f;
    }
    material.cauchy_a = std::max(1.0f, material.cauchy_a);
    material.cauchy_b = std::max(0.0f, material.cauchy_b);
    material.emission_angle_deg = std::max(0.0f, std::min(360.0f, material.emission_angle_deg));
    return true;
}

} // namespace

bool save_scene_json(const std::string& path, const Scene& scene, const SceneDocumentSettings& settings, std::string* error_message) {
    std::ofstream out(path);
    if (!out) {
        if (error_message) *error_message = "Could not open file for writing: " + path;
        return false;
    }

    out << std::setprecision(9);
    out << "{\n";
    out << "  \"version\": 2,\n";
    out << "  \"renderer\": {\n";
    out << "    \"integrator\": \"" << integrator_kind_to_string(settings.integrator.kind) << "\",\n";
    out << "    \"max_depth\": " << settings.integrator.max_depth << ",\n";
    out << "    \"seed\": " << settings.integrator.seed << ",\n";
    out << "    \"spectral_enabled\": " << (settings.integrator.spectral.enabled ? "true" : "false") << ",\n";
    out << "    \"spectral_wavelength_samples\": " << settings.integrator.spectral.wavelength_samples << ",\n";
    out << "    \"spectral_xyz_importance\": " << (settings.integrator.spectral.xyz_importance ? "true" : "false") << ",\n";
    out << "    \"photon_count\": " << settings.integrator.photon_mapping.photon_count << ",\n";
    out << "    \"photon_max_depth\": " << settings.integrator.photon_mapping.photon_max_depth << ",\n";
    out << "    \"photon_gather_radius\": " << settings.integrator.photon_mapping.gather_radius << ",\n";
    out << "    \"photon_strength\": " << settings.integrator.photon_mapping.strength << ",\n";
    out << "    \"photon_caustics_only\": " << (settings.integrator.photon_mapping.caustics_only ? "true" : "false") << ",\n";
    out << "    \"ris_num_bins\": " << settings.integrator.ris_direction.num_bins << ",\n";
    out << "    \"ris_exploration_percent\": " << settings.integrator.ris_direction.min_probability_percent << ",\n";
    out << "    \"ris_smooth_sigma_deg\": " << settings.integrator.ris_direction.smooth_sigma_deg << ",\n";
    out << "    \"ris_candidate_count\": " << settings.integrator.ris_direction.candidate_count << ",\n";
    out << "    \"ris_spatial_reuse_enabled\": " << (settings.integrator.ris_direction.spatial_reuse_enabled ? "true" : "false") << ",\n";
    out << "    \"ris_spatial_radius\": " << settings.integrator.ris_direction.spatial_radius << ",\n";
    out << "    \"ris_spatial_strength\": " << settings.integrator.ris_direction.spatial_strength << ",\n";
    out << "    \"ris_spatial_interval\": " << settings.integrator.ris_direction.spatial_interval << ",\n";
    out << "    \"field_width\": " << settings.field_width << ",\n";
    out << "    \"field_height\": " << settings.field_height << ",\n";
    out << "    \"field_bounds_min\": "; write_vec2(out, settings.field_bounds_min); out << ",\n";
    out << "    \"field_bounds_max\": "; write_vec2(out, settings.field_bounds_max); out << ",\n";
    out << "    \"samples_per_frame\": " << settings.samples_per_frame << ",\n";
    out << "    \"stop_after_samples\": " << settings.stop_after_samples << "\n";
    out << "  },\n";

    out << "  \"segments\": [\n";
    for (std::size_t i = 0; i < scene.segments.size(); ++i) {
        const Segment& s = scene.segments[i];
        out << "    {\n";
        out << "      \"a\": "; write_vec2(out, s.a); out << ",\n";
        out << "      \"b\": "; write_vec2(out, s.b); out << ",\n";
        out << "      \"normal\": "; write_vec2(out, s.normal); out << ",\n";
        out << "      \"material\": "; write_material(out, s.material, 6); out << "\n";
        out << "    }" << (i + 1 == scene.segments.size() ? "\n" : ",\n");
    }
    out << "  ],\n";

    out << "  \"circles\": [\n";
    for (std::size_t i = 0; i < scene.circles.size(); ++i) {
        const Circle& c = scene.circles[i];
        out << "    {\n";
        out << "      \"center\": "; write_vec2(out, c.center); out << ",\n";
        out << "      \"radius\": " << c.radius << ",\n";
        out << "      \"material\": "; write_material(out, c.material, 6); out << "\n";
        out << "    }" << (i + 1 == scene.circles.size() ? "\n" : ",\n");
    }
    out << "  ]\n";
    out << "}\n";

    if (!out) {
        if (error_message) *error_message = "Failed while writing file: " + path;
        return false;
    }
    if (error_message) error_message->clear();
    return true;
}

bool load_scene_json(const std::string& path, Scene& scene, SceneDocumentSettings& settings, std::string* error_message) {
    std::filesystem::path fpath = path;
    std::filesystem::path abs_path = std::filesystem::absolute(fpath);
    std::ifstream in(fpath);
    if (!in) {
        if (error_message) *error_message = "Could not open file for reading: " + abs_path.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();

    JsonValue root;
    JsonParser parser(buffer.str());
    std::string parse_error;
    if (!parser.parse(root, &parse_error)) {
        if (error_message) *error_message = parse_error;
        return false;
    }
    if (root.type != JsonValue::Type::Object) {
        if (error_message) *error_message = "Scene file root must be a JSON object";
        return false;
    }

    Scene next_scene;
    SceneDocumentSettings next_settings = settings;

    if (const JsonValue* renderer = root.get("renderer")) {
        if (renderer->type != JsonValue::Type::Object) {
            if (error_message) *error_message = "renderer must be an object";
            return false;
        }
        std::string integrator;
        if (read_string(*renderer, "integrator", integrator)) {
            next_settings.integrator.kind = integrator_kind_from_string(integrator);
        }
        read_number(*renderer, "max_depth", next_settings.integrator.max_depth);
        read_number(*renderer, "seed", next_settings.integrator.seed);
        read_bool(*renderer, "spectral_enabled", next_settings.integrator.spectral.enabled);
        read_number(*renderer, "spectral_wavelength_samples", next_settings.integrator.spectral.wavelength_samples);
        read_bool(*renderer, "spectral_xyz_importance", next_settings.integrator.spectral.xyz_importance);
        read_number(*renderer, "photon_count", next_settings.integrator.photon_mapping.photon_count);
        read_number(*renderer, "photon_max_depth", next_settings.integrator.photon_mapping.photon_max_depth);
        read_number(*renderer, "photon_gather_radius", next_settings.integrator.photon_mapping.gather_radius);
        read_number(*renderer, "photon_strength", next_settings.integrator.photon_mapping.strength);
        read_bool(*renderer, "photon_caustics_only", next_settings.integrator.photon_mapping.caustics_only);
        read_number(*renderer, "ris_num_bins", next_settings.integrator.ris_direction.num_bins);
        read_number(*renderer, "ris_exploration_percent", next_settings.integrator.ris_direction.min_probability_percent);
        read_number(*renderer, "ris_smooth_sigma_deg", next_settings.integrator.ris_direction.smooth_sigma_deg);
        read_number(*renderer, "ris_candidate_count", next_settings.integrator.ris_direction.candidate_count);
        read_bool(*renderer, "ris_spatial_reuse_enabled", next_settings.integrator.ris_direction.spatial_reuse_enabled);
        read_number(*renderer, "ris_spatial_radius", next_settings.integrator.ris_direction.spatial_radius);
        read_number(*renderer, "ris_spatial_strength", next_settings.integrator.ris_direction.spatial_strength);
        read_number(*renderer, "ris_spatial_interval", next_settings.integrator.ris_direction.spatial_interval);
        read_number(*renderer, "field_width", next_settings.field_width);
        read_number(*renderer, "field_height", next_settings.field_height);
        read_vec2(*renderer, "field_bounds_min", next_settings.field_bounds_min);
        read_vec2(*renderer, "field_bounds_max", next_settings.field_bounds_max);
        read_number(*renderer, "samples_per_frame", next_settings.samples_per_frame);
        read_number(*renderer, "stop_after_samples", next_settings.stop_after_samples);
    }

    const JsonValue* segments = root.get("segments");
    if (segments && segments->type == JsonValue::Type::Array) {
        for (const JsonValue& value : segments->array_value) {
            if (value.type != JsonValue::Type::Object) continue;
            Segment s;
            read_vec2(value, "a", s.a);
            read_vec2(value, "b", s.b);
            read_vec2(value, "normal", s.normal);
            const JsonValue* material = value.get("material");
            if (!material) {
                if (error_message) *error_message = "Every segment must have a material object";
                return false;
            }
            if (!read_material_object(*material, s.material, error_message)) {
                return false;
            }
            if (length_squared(s.normal) <= 1.0e-10f) {
                s.normal = normalize(perpendicular(s.b - s.a));
            }
            next_scene.add_segment(s.a, s.b, s.normal, s.material);
        }
    }

    const JsonValue* circles = root.get("circles");
    if (circles && circles->type == JsonValue::Type::Array) {
        for (const JsonValue& value : circles->array_value) {
            if (value.type != JsonValue::Type::Object) continue;
            Circle c;
            read_vec2(value, "center", c.center);
            read_number(value, "radius", c.radius);
            const JsonValue* material = value.get("material");
            if (!material) {
                if (error_message) *error_message = "Every circle must have a material object";
                return false;
            }
            if (!read_material_object(*material, c.material, error_message)) {
                return false;
            }
            c.radius = std::max(0.02f, c.radius);
            next_scene.add_circle(c.center, c.radius, c.material);
        }
    }

    next_settings.integrator.max_depth = std::max(1, next_settings.integrator.max_depth);
    next_settings.integrator.seed = std::max<std::uint64_t>(1, next_settings.integrator.seed);
    next_settings.integrator.photon_mapping.photon_count = std::max(0, next_settings.integrator.photon_mapping.photon_count);
    next_settings.integrator.photon_mapping.photon_max_depth = std::max(1, next_settings.integrator.photon_mapping.photon_max_depth);
    next_settings.integrator.photon_mapping.gather_radius = std::max(0.001f, next_settings.integrator.photon_mapping.gather_radius);
    next_settings.integrator.photon_mapping.strength = std::max(0.0f, next_settings.integrator.photon_mapping.strength);
    next_settings.integrator.ris_direction.num_bins = std::clamp(next_settings.integrator.ris_direction.num_bins, 1, 256);
    next_settings.integrator.ris_direction.min_probability_percent = std::clamp(next_settings.integrator.ris_direction.min_probability_percent, 0.0f, 100.0f);
    next_settings.integrator.ris_direction.smooth_sigma_deg = std::clamp(next_settings.integrator.ris_direction.smooth_sigma_deg, 0, 180);
    next_settings.integrator.ris_direction.candidate_count = std::clamp(next_settings.integrator.ris_direction.candidate_count, 1, 128);
    next_settings.integrator.ris_direction.spatial_radius = std::clamp(next_settings.integrator.ris_direction.spatial_radius, 1, 8);
    next_settings.integrator.ris_direction.spatial_strength = std::clamp(next_settings.integrator.ris_direction.spatial_strength, 0.0f, 1.0f);
    next_settings.integrator.ris_direction.spatial_interval = std::clamp(next_settings.integrator.ris_direction.spatial_interval, 1, 16);
    next_settings.field_width = std::max(1, next_settings.field_width);
    next_settings.field_height = std::max(1, next_settings.field_height);
    next_settings.samples_per_frame = std::max(1, next_settings.samples_per_frame);
    next_settings.stop_after_samples = std::max(0, next_settings.stop_after_samples);
    next_settings.integrator.spectral.wavelength_samples = std::clamp(next_settings.integrator.spectral.wavelength_samples, 1, 64);

    next_scene.rebuild_light_segment_ids();
    scene = std::move(next_scene);
    settings = next_settings;
    if (error_message) error_message->clear();
    return true;
}

} // namespace pt2d
