#pragma once

#include <vector>

#include "material.h"
#include "math.h"

namespace pg {

struct SurfaceHit8 {
    HitRecord8 geometry{};
    Material8 material{};


};

struct Sphere {
    Vec3f center{};
    float radius = 1.0f;
    Material material{};

    SurfaceHit8 hit_packet(
        const Ray8& rays,
        const Float8& ray_tmin,
        const Float8& ray_tmax) const {
        const Vec3f8 center8 = splat(center);
        const Vec3f8 oc = rays.origin - center8;

        const Float8 a = rays.direction.length_squared();
        const Float8 half_b = oc.dot(rays.direction);
        const Float8 c = oc.length_squared() - Float8(radius * radius);

        const Float8 discriminant = half_b * half_b - a * c;
        const Bool8 has_real_root = discriminant >= Float8(0.0f);
        const Float8 sqrt_discriminant = Float8::sqrt(
            Float8::max(discriminant, Float8(0.0f))
        );

        const Float8 near_root = (-half_b - sqrt_discriminant) / a;
        const Bool8 near_valid =
            has_real_root & (ray_tmin < near_root) & (near_root < ray_tmax);

        const Float8 far_root = (-half_b + sqrt_discriminant) / a;
        const Bool8 far_valid =
            has_real_root & (ray_tmin < far_root) & (far_root < ray_tmax);

        const Bool8 hit_mask = near_valid | far_valid;
        const Float8 root = select(near_valid, near_root, far_root);

        const Vec3f8 hit_point = rays.at(root);
        const Vec3f8 outward_normal = (hit_point - center8) / Float8(radius);
        const Bool8 front_face =
            rays.direction.dot(outward_normal) < Float8(0.0f);
        const Vec3f8 normal = select(
            front_face,
            outward_normal,
            -outward_normal
        );

        HitRecord8 record;
        record.p = hit_point;
        record.normal = normal;
        record.t = root;
        record.front_face = front_face;
        record.hit = hit_mask;

        return SurfaceHit8{
            record,
            Material8::splat(material)
        };
    }
};

class Scene {
public:
    void add_sphere(
        const Vec3f& center,
        float radius,
        const Material& material) {
        m_spheres.push_back(Sphere{center, radius, material});
    }

    SurfaceHit8 hit_packet(
        const Ray8& rays,
        const Float8 ray_tmin,
        const Float8 ray_tmax) const {
        SurfaceHit8 closest;
        Float8 closest_so_far(ray_tmax);

        // Try to hit each spheres
        for (const Sphere& sphere : m_spheres) {
            const SurfaceHit8 candidate = sphere.hit_packet(
                rays,
                ray_tmin,
                closest_so_far
            );

            // Update closest geometry and material
            closest.geometry = select(
                candidate.geometry.hit,
                candidate.geometry,
                closest.geometry
            );

            closest.material = select(
                candidate.geometry.hit,
                candidate.material,
                closest.material
            );

			// Update closest_so_far to the nearest hit distance
            closest_so_far = select(
                candidate.geometry.hit,
                candidate.geometry.t,
                closest_so_far
            );
        }

        return closest;
    }

    [[nodiscard]] std::size_t sphere_count() const noexcept {
        return m_spheres.size();
    }

private:
    std::vector<Sphere> m_spheres;
};

inline Color8 normal_color_packet(const HitRecord8& hit) {
    const Float8 half(0.5f);
    const Float8 one(1.0f);
    return Color8(
        (hit.normal.x + one) * half,
        (hit.normal.y + one) * half,
        (hit.normal.z + one) * half
    );
}

inline Color8 background_color_packet(const Ray8& rays) {
    const Vec3f8 unit_direction = rays.direction.normalized();
    const Float8 t = (unit_direction.y + Float8(1.0f)) * Float8(0.5f);
    const Color8 white(Float8(1.0f), Float8(1.0f), Float8(1.0f));
    const Color8 sky_blue(Float8(0.5f), Float8(0.7f), Float8(1.0f));
    return white * (Float8(1.0f) - t) + sky_blue * t;
}

inline Color8 ray_color_normal_packet(
    const Ray8& rays,
    const Scene& world) {
    // Completed shadow-acne guard. Reuse the same t_min in the path tracer.
    constexpr float ray_tmin = 0.001f;
    const SurfaceHit8 hit = world.hit_packet(rays, ray_tmin, 1.0e30f);
    return select(
        hit.geometry.hit,
        normal_color_packet(hit.geometry),
        background_color_packet(rays)
    );
}

} // namespace pg
