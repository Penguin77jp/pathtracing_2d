#pragma once

#include "material.h"
#include "random_sampling.h"

namespace pg {

struct ScatterRecord8 {
    Ray8 ray{};
    Color8 attenuation{};
    Bool8 scattered = Bool8::constant(false);
};

inline ScatterRecord8 no_scatter_record() {
    return ScatterRecord8{
        Ray8{},
        Color8{},
        Bool8::constant(false)
    };
}

// SIMD_EXERCISE(4): Lambertian scatter, including the near-zero fallback.
inline ScatterRecord8 scatter_lambertian_packet(
    const Ray8& incoming,
    const HitRecord8& hit,
    const Material8& material,
    const Bool8& material_mask,
    RngPacket8& rng) {
    (void)incoming;
    (void)hit;
    (void)material;
    (void)material_mask;
    (void)rng;
    return no_scatter_record();
}

// Completed example: specular reflection plus fuzz.
inline ScatterRecord8 scatter_metal_packet(
    const Ray8& incoming,
    const HitRecord8& hit,
    const Material8& material,
    const Bool8& material_mask,
    RngPacket8& rng) {
    const Vec3f8 reflected = reflect(
        incoming.direction.normalized(),
        hit.normal
    );
    const Vec3f8 fuzz_offset =
        random_in_unit_sphere_packet(rng) * material.fuzz;
    const Vec3f8 direction = reflected + fuzz_offset;
    const Bool8 above_surface = direction.dot(hit.normal) > Float8(0.0f);
    const Bool8 scattered = material_mask & hit.hit & above_surface;

    return ScatterRecord8{
        Ray8{hit.p, direction},
        material.albedo,
        scattered
    };
}

// SIMD_EXERCISE(5): Select reflection/refraction independently per lane.
inline ScatterRecord8 scatter_dielectric_packet(
    const Ray8& incoming,
    const HitRecord8& hit,
    const Material8& material,
    const Bool8& material_mask,
    RngPacket8& rng) {
    (void)incoming;
    (void)hit;
    (void)material;
    (void)material_mask;
    (void)rng;
    return no_scatter_record();
}

} // namespace pg
