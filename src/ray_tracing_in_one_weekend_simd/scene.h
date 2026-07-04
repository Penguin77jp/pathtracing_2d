#pragma once

#include <vector>

#include "math.h"

namespace pg {

struct Sphere {
    Vec3f center{};
    float radius;

    HitRecord8 hit_packet(const Ray8& rays, const Float8& ray_tmin, const Float8& ray_tmax) const {
        // TODO: implement SIMD ray-sphere intersection and normal setup.
        return {};
    }
};

class Scene {
public:
    void add_sphere(const Vec3f& center, float radius) {
        m_spheres.push_back(Sphere{ center, radius });
    }

    HitRecord8 hit_packet(const Ray8& rays, float ray_tmin, float ray_tmax) const {
        // TODO: iterate spheres and keep closest lane-wise hit.
        return {};
    }

private:
    std::vector<Sphere> m_spheres;
};

inline Color8 normal_color_packet(const HitRecord8& hit_record) {
    // TODO: map surface normal from [-1, 1] to RGB [0, 1].
    return {};
}

inline Color8 background_color_packet(const Ray8& rays) {
    // TODO: implement sky gradient from normalized ray direction.
    return {};
}

inline Color8 ray_color_packet(const Ray8& rays, const Scene& world) {
    // TODO: return normal color on hit, otherwise background color.
    return {};
}


}
