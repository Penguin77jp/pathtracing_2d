#include "pt2d/Scene.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace pt2d {

int Scene::add_material(Material material) {
    materials.push_back(std::move(material));
    return static_cast<int>(materials.size() - 1);
}

int Scene::find_material_by_name(const std::string& name) const {
    for (int i = 0; i < static_cast<int>(materials.size()); ++i) {
        if (materials[i].name == name) {
            return i;
        }
    }
    return -1;
}

void Scene::rebuild_object_ids() {
    for (int i = 0; i < static_cast<int>(segments.size()); ++i) {
        segments[i].object_id = i;
    }
    for (int i = 0; i < static_cast<int>(circles.size()); ++i) {
        circles[i].object_id = i;
    }
}

void Scene::rebuild_light_segment_ids() {
    rebuild_object_ids();
    light_segment_ids.clear();
    for (const Segment& segment : segments) {
        if (segment.material_id >= 0 && segment.material_id < static_cast<int>(materials.size()) &&
            materials[segment.material_id].is_light()) {
            light_segment_ids.push_back(segment.object_id);
        }
    }
}

void Scene::refresh_segment_normal_keep_side(int segment_id) {
    if (segment_id < 0 || segment_id >= static_cast<int>(segments.size())) {
        return;
    }

    Segment& segment = segments[segment_id];
    const Vec2 edge = segment.b - segment.a;
    if (length_squared(edge) <= 1.0e-10f) {
        return;
    }

    const Vec2 old_normal = segment.normal;
    Vec2 candidate = normalize(perpendicular(edge));
    if (dot(candidate, old_normal) < 0.0f) {
        candidate = -candidate;
    }
    segment.normal = candidate;
}

int Scene::add_segment(Vec2 a, Vec2 b, Vec2 normal, int material_id) {
    Segment s;
    s.a = a;
    s.b = b;
    s.normal = normalize(normal);
    s.material_id = material_id;
    s.object_id = static_cast<int>(segments.size());
    segments.push_back(s);
    if (material_id >= 0 && material_id < static_cast<int>(materials.size()) && materials[material_id].is_light()) {
        light_segment_ids.push_back(s.object_id);
    }
    return s.object_id;
}

int Scene::add_circle(Vec2 center, float radius, int material_id) {
    Circle c;
    c.center = center;
    c.radius = std::max(0.02f, radius);
    c.material_id = material_id;
    c.object_id = static_cast<int>(circles.size());
    circles.push_back(c);
    return c.object_id;
}

void Scene::erase_segment(int segment_id) {
    if (segment_id < 0 || segment_id >= static_cast<int>(segments.size())) {
        return;
    }
    segments.erase(segments.begin() + segment_id);
    rebuild_light_segment_ids();
}

void Scene::erase_circle(int circle_id) {
    if (circle_id < 0 || circle_id >= static_cast<int>(circles.size())) {
        return;
    }
    circles.erase(circles.begin() + circle_id);
    rebuild_light_segment_ids();
}

HitInfo Scene::intersect(const Ray2& ray, float min_t, float max_t) const {
    HitInfo best;
    best.t = max_t;

    for (const Segment& segment : segments) {
        const Vec2 edge = segment.b - segment.a;
        const float denom = cross(ray.dir, edge);
        if (std::abs(denom) < 1.0e-7f) {
            continue;
        }

        const Vec2 ao = segment.a - ray.origin;
        const float t = cross(ao, edge) / denom;
        const float u = cross(ao, ray.dir) / denom;

        if (t < min_t || t >= best.t) {
            continue;
        }
        if (u < 0.0f || u > 1.0f) {
            continue;
        }

        best.hit = true;
        best.t = t;
        best.position = ray.origin + ray.dir * t;
        best.normal = segment.normal;
        best.object_id = segment.object_id;
        best.material_id = segment.material_id;
        best.primitive_kind = PrimitiveKind::Segment;
    }

    for (const Circle& circle : circles) {
        const Vec2 oc = ray.origin - circle.center;
        const float a = dot(ray.dir, ray.dir);
        const float half_b = dot(oc, ray.dir);
        const float c = dot(oc, oc) - circle.radius * circle.radius;
        const float discriminant = half_b * half_b - a * c;
        if (discriminant < 0.0f) {
            continue;
        }

        const float sqrt_d = std::sqrt(discriminant);
        float t = (-half_b - sqrt_d) / a;
        if (t < min_t || t >= best.t) {
            t = (-half_b + sqrt_d) / a;
        }
        if (t < min_t || t >= best.t) {
            continue;
        }

        best.hit = true;
        best.t = t;
        best.position = ray.origin + ray.dir * t;
        best.normal = normalize(best.position - circle.center);
        best.object_id = circle.object_id;
        best.material_id = circle.material_id;
        best.primitive_kind = PrimitiveKind::Circle;
    }

    return best;
}

bool Scene::occluded(const Ray2& ray, float max_t) const {
    return intersect(ray, kEpsilon, max_t).hit;
}

float Scene::total_light_length() const {
    float total = 0.0f;
    for (int id : light_segment_ids) {
        if (id >= 0 && id < static_cast<int>(segments.size())) {
            const Segment& s = segments[id];
            total += length(s.b - s.a);
        }
    }
    return total;
}

LightSample Scene::sample_light(float u_select, float u_segment) const {
    LightSample sample;
    if (light_segment_ids.empty()) {
        return sample;
    }

    const float total = total_light_length();
    float target = u_select * total;

    int chosen = light_segment_ids.back();
    float accumulated = 0.0f;
    for (int id : light_segment_ids) {
        if (id < 0 || id >= static_cast<int>(segments.size())) {
            continue;
        }
        const Segment& s = segments[id];
        const float len = length(s.b - s.a);
        if (target <= accumulated + len) {
            chosen = id;
            break;
        }
        accumulated += len;
    }

    const Segment& light = segments[chosen];
    const Material& material = materials[light.material_id];
    sample.position = lerp(light.a, light.b, u_segment);
    sample.normal = light.normal;
    sample.emission = material.emission;
    sample.pdf_length = total > 0.0f ? 1.0f / total : 0.0f;
    sample.light_object_id = light.object_id;
    return sample;
}

Scene make_default_scene() {
    Scene scene;

    const int white = scene.add_material({"white diffuse", make_color(0.78f, 0.78f, 0.74f), make_color(0.0f), MaterialKind::Diffuse, 1.0f});
    const int red = scene.add_material({"red diffuse", make_color(0.90f, 0.25f, 0.20f), make_color(0.0f), MaterialKind::Diffuse, 1.0f});
    const int blue = scene.add_material({"blue diffuse", make_color(0.25f, 0.35f, 0.95f), make_color(0.0f), MaterialKind::Diffuse, 1.0f});
    const int glass = scene.add_material({"glass dielectric", make_color(0.96f, 0.98f, 1.0f), make_color(0.0f), MaterialKind::Dielectric, 1.5f});
    const int light = scene.add_material({"area light", make_color(0.0f), make_color(8.0f, 7.5f, 6.5f), MaterialKind::Diffuse, 1.0f});

    // A tiny 2D Cornell-box-like scene. Segment normals point into the room.
    scene.add_segment({-3.0f, -2.0f}, { 3.0f, -2.0f}, { 0.0f,  1.0f}, white); // floor
    scene.add_segment({ 3.0f, -2.0f}, { 3.0f,  2.0f}, {-1.0f,  0.0f}, blue);  // right wall
    scene.add_segment({ 3.0f,  2.0f}, { 0.65f, 2.0f}, { 0.0f, -1.0f}, white); // ceiling right
    scene.add_segment({-0.65f, 2.0f}, {-3.0f, 2.0f}, { 0.0f, -1.0f}, white); // ceiling left
    scene.add_segment({-3.0f,  2.0f}, {-3.0f,-2.0f}, { 1.0f,  0.0f}, red);   // left wall

    scene.add_segment({-0.65f, 2.0f}, {0.65f, 2.0f}, {0.0f, -1.0f}, light);    // area light

    // Two blockers to make debug paths and NEE visibility tests interesting.
    scene.add_segment({-1.1f, -2.0f}, {-0.35f, -0.55f}, { 0.90f, -0.45f}, white);
    scene.add_segment({ 0.75f, -2.0f}, { 1.25f, -0.85f}, {-0.92f,  0.40f}, white);

    // A default 2D glass sphere/circle for refraction debugging.
    scene.add_circle({0.0f, -0.65f}, 0.42f, glass);

    scene.rebuild_light_segment_ids();
    return scene;
}

} // namespace pt2d
