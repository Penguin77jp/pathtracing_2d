#pragma once

#include <string>
#include <vector>

#include "pt2d/Color.h"
#include "pt2d/Math.h"

namespace pt2d {

enum class MaterialKind {
    Diffuse,
    Dielectric
};

struct Material {
    std::string name;
    Color albedo = make_color(0.8f);
    Color emission = make_color(0.0f);
    MaterialKind kind = MaterialKind::Diffuse;
    float ior = 1.5f;

    bool is_light() const { return !is_black(emission); }
    bool is_dielectric() const { return kind == MaterialKind::Dielectric; }
};

struct Segment {
    Vec2 a;
    Vec2 b;
    Vec2 normal;
    int material_id = 0;
    int object_id = -1;
};

struct Circle {
    Vec2 center;
    float radius = 0.5f;
    int material_id = 0;
    int object_id = -1;
};

enum class PrimitiveKind {
    None,
    Segment,
    Circle
};

struct HitInfo {
    bool hit = false;
    float t = 0.0f;
    Vec2 position;
    Vec2 normal;
    int object_id = -1;
    int material_id = -1;
    PrimitiveKind primitive_kind = PrimitiveKind::None;
};

struct LightSample {
    Vec2 position;
    Vec2 normal;
    Color emission;
    float pdf_length = 0.0f;
    int light_object_id = -1;
};

class Scene {
public:
    std::vector<Material> materials;
    std::vector<Segment> segments;
    std::vector<Circle> circles;
    std::vector<int> light_segment_ids;

    int add_material(Material material);
    int find_material_by_name(const std::string& name) const;
    int add_segment(Vec2 a, Vec2 b, Vec2 normal, int material_id);
    int add_circle(Vec2 center, float radius, int material_id);
    void erase_segment(int segment_id);
    void erase_circle(int circle_id);
    void rebuild_object_ids();
    void rebuild_light_segment_ids();
    void refresh_segment_normal_keep_side(int segment_id);

    HitInfo intersect(const Ray2& ray, float min_t = kEpsilon, float max_t = 1.0e30f) const;
    bool occluded(const Ray2& ray, float max_t) const;
    float total_light_length() const;
    LightSample sample_light(float u_select, float u_segment) const;
};

Scene make_default_scene();

} // namespace pt2d
