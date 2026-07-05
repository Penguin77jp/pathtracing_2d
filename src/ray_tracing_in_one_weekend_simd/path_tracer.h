#pragma once

#include "scatter.h"

namespace pg {

inline const Float8 kRayTMin{1.0e-4f};
inline const Float8 kRayTMax{1.0e30f};

// The path-tracing loop is material-agnostic. Material dispatch and all
// material-specific behavior live behind scatter_packet().
inline Color8 ray_color_path_packet(
    const Ray8& initial_rays,
    const Scene& world,
    RngPacket8& rng,
    int max_depth) {
    Ray8 rays = initial_rays;
    Color8 throughput = Color8::SetOne();
    Color8 radiance = Color8::SetZero();
    Bool8 active = Bool8::constant(true);

    const Color8 zero = Color8::SetZero();

    for (int depth = 0; depth < max_depth; ++depth) {
        const SurfaceHit8 hit = world.hit_packet(
            rays,
            kRayTMin,
            kRayTMax
        );

        const Bool8 hit_mask = active & hit.geometry.hit;
        const Bool8 miss_mask = active & ~hit.geometry.hit;

        // A ray that reaches the sky has finished. Its current throughput
        // weights the background contribution accumulated into radiance.
        const Color8 background = background_color_packet(rays);
        radiance += select(
            miss_mask,
            throughput * background,
            zero
        );

        // The path tracer does not know which material is present.
        const ScatterRecord8 scatter = scatter_packet(rays, hit, rng);
        const Bool8 continue_mask = hit_mask & scatter.scattered;

        // Only lanes that successfully scattered advance to the next bounce.
        rays = select(continue_mask, scatter.ray, rays);
        throughput = select(
            continue_mask,
            throughput * scatter.attenuation,
            throughput
        );
        active = continue_mask;
    }

    return radiance;
}

} // namespace pg
