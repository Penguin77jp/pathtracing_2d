#pragma once

#include "random.h"
#include "scatter.h"
#include "scene.h"

namespace pg {

// SIMD_EXERCISE(6): Implement an iterative packet path tracer.
// Keep per-lane active state, throughput, accumulated radiance, and ray state.
// Use ray_tmin = 0.001f for every bounce to avoid self-intersections.
inline Color8 ray_color_path_packet(
    const Ray8& initial_rays,
    const Scene& world,
    RngPacket8& rng,
    int max_depth) {
	Ray8 rays = initial_rays;
	Float8 t_min(1e-4);
	Float8 t_max(1e30);
    Color8 throughput = Color8::SetOne();
    for (int d = 0; d < max_depth; ++d) {
        SurfaceHit8 hit = world.hit_packet(rays, t_min, t_max);
        throughput *= select(hit.geometry.hit, hit.material.albedo, background_color_packet(rays));
		rays = next_ray(hit, rays, t_min, rng);
    }
    return throughput;
}

} // namespace pg
