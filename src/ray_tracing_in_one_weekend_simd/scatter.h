#pragma once

#include "random_sampling.h"
#include "scene.h"

namespace pg {

struct ScatterRecord8 {
    Ray8 ray{};
    Color8 attenuation{};
    Bool8 scattered = Bool8::constant(false);

    static ScatterRecord8 none() {
        return ScatterRecord8{};
    }

    friend ScatterRecord8 select(
        const Bool8& mask,
        const ScatterRecord8& if_true,
        const ScatterRecord8& if_false) {
        return ScatterRecord8{
            select(mask, if_true.ray, if_false.ray),
            select(mask, if_true.attenuation, if_false.attenuation),
            Bool8{
                _mm256_blendv_ps(
                    if_false.scattered.values,
                    if_true.scattered.values,
                    mask.values
                )
            }
        };
    }
};

inline Vec3f8 lambertian_next_direction_packet(
    const HitRecord8& hit,
    RngPacket8& rng) {
    const Vec3f8 candidate =
        hit.normal + random_unit_vector_packet(rng);

    // When the random vector is almost the exact opposite of the normal,
    // their sum is too close to zero to be a useful ray direction.
    const Float8 epsilon(1.0e-8f);
    const Bool8 near_zero =
        (Float8::abs(candidate.x) < epsilon)
        & (Float8::abs(candidate.y) < epsilon)
        & (Float8::abs(candidate.z) < epsilon);

    return select(near_zero, hit.normal, candidate);
}

inline ScatterRecord8 scatter_lambertian_packet(
    const Ray8& incoming,
    const SurfaceHit8& hit,
    const Bool8& material_mask,
    RngPacket8& rng) {
    (void)incoming;

    const Vec3f8 direction =
        lambertian_next_direction_packet(hit.geometry, rng);

    return ScatterRecord8{
        Ray8{hit.geometry.p, direction},
        hit.material.albedo,
        material_mask & hit.geometry.hit
    };
}

inline ScatterRecord8 scatter_metal_packet(
    const Ray8& incoming,
    const SurfaceHit8& hit,
    const Bool8& material_mask,
    RngPacket8& rng) {
    const Vec3f8 unit_direction = incoming.direction.normalized();
    const Vec3f8 reflected = reflect(
        unit_direction,
        hit.geometry.normal
    );

    const Float8 fuzz = Float8::clamp(
        hit.material.fuzz,
        Float8(0.0f),
        Float8(1.0f)
    );
    const Vec3f8 direction = reflected
        + random_in_unit_sphere_packet(rng) * fuzz;

    // A fuzzy reflection that points below the surface is absorbed.
    const Bool8 above_surface =
        direction.dot(hit.geometry.normal) > Float8(0.0f);
    const Bool8 scattered = material_mask
        & hit.geometry.hit
        & above_surface;

    return ScatterRecord8{
        Ray8{hit.geometry.p, direction},
        hit.material.albedo,
        scattered
    };
}

inline Float8 dielectric_reflectance_packet(
    const Float8& cosine,
    const Float8& refraction_ratio) {
    Float8 r0 =
        (Float8(1.0f) - refraction_ratio)
        / (Float8(1.0f) + refraction_ratio);
    r0 *= r0;

    const Float8 one_minus_cosine = Float8(1.0f) - cosine;
    const Float8 squared = one_minus_cosine * one_minus_cosine;
    const Float8 fifth_power = squared * squared * one_minus_cosine;
    return r0 + (Float8(1.0f) - r0) * fifth_power;
}

inline ScatterRecord8 scatter_dielectric_packet(
    const Ray8& incoming,
    const SurfaceHit8& hit,
    const Bool8& material_mask,
    RngPacket8& rng) {
    const Vec3f8 unit_direction = incoming.direction.normalized();

    const Float8 refraction_ratio = select(
        hit.geometry.front_face,
        Float8(1.0f) / hit.material.refraction_index,
        hit.material.refraction_index
    );

    const Float8 cos_theta = Float8::min(
        (-unit_direction).dot(hit.geometry.normal),
        Float8(1.0f)
    );
    const Float8 sin_theta = Float8::sqrt(
        Float8::max(
            Float8(0.0f),
            Float8(1.0f) - cos_theta * cos_theta
        )
    );

    const Bool8 cannot_refract =
        refraction_ratio * sin_theta > Float8(1.0f);
    const Float8 reflectance = dielectric_reflectance_packet(
        cos_theta,
        refraction_ratio
    );
    const Bool8 reflect_by_probability =
        reflectance > rng.next_float01();
    const Bool8 use_reflection =
        cannot_refract | reflect_by_probability;

    const Vec3f8 reflected = reflect(
        unit_direction,
        hit.geometry.normal
    );
    const Vec3f8 refracted = refract(
        unit_direction,
        hit.geometry.normal,
        refraction_ratio
    );
    const Vec3f8 direction = select(
        use_reflection,
        reflected,
        refracted
    );

    return ScatterRecord8{
        Ray8{hit.geometry.p, direction},
        Color8::SetOne(),
        material_mask & hit.geometry.hit
    };
}

// Material dispatch is kept out of the path tracer. Adding a material only
// changes this layer and its material-specific scatter function.
inline ScatterRecord8 scatter_packet(
    const Ray8& incoming,
    const SurfaceHit8& hit,
    RngPacket8& rng) {
    const Bool8 hit_mask = hit.geometry.hit;

    const Bool8 lambertian_mask = hit_mask & (
        hit.material.kind
        == EnumInt8(static_cast<std::int32_t>(MaterialKind::Lambertian))
    );
    const Bool8 metal_mask = hit_mask & (
        hit.material.kind
        == EnumInt8(static_cast<std::int32_t>(MaterialKind::Metal))
    );
    const Bool8 dielectric_mask = hit_mask & (
        hit.material.kind
        == EnumInt8(static_cast<std::int32_t>(MaterialKind::Dielectric))
    );

    const ScatterRecord8 lambertian = scatter_lambertian_packet(
        incoming,
        hit,
        lambertian_mask,
        rng
    );
    const ScatterRecord8 metal = scatter_metal_packet(
        incoming,
        hit,
        metal_mask,
        rng
    );
    const ScatterRecord8 dielectric = scatter_dielectric_packet(
        incoming,
        hit,
        dielectric_mask,
        rng
    );

    ScatterRecord8 result = ScatterRecord8::none();
    result = select(lambertian_mask, lambertian, result);
    result = select(metal_mask, metal, result);
    result = select(dielectric_mask, dielectric, result);
    return result;
}

} // namespace pg
