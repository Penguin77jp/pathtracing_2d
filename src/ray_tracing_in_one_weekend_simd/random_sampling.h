#pragma once

#include "math.h"
#include "random.h"

namespace pg {

// SIMD_EXERCISE(3): Rejection-sample each unfinished lane independently.
inline Vec3f8 random_in_unit_sphere_packet(RngPacket8& rng) {
    (void)rng;
    return Vec3f8(Float8(0.0f), Float8(0.0f), Float8(0.0f));
}

// SIMD_EXERCISE(3): Build this from random_in_unit_sphere_packet and handle
// near-zero vectors without scalar lane extraction.
inline Vec3f8 random_unit_vector_packet(RngPacket8& rng) {
    (void)rng;
    return Vec3f8(Float8(0.0f), Float8(1.0f), Float8(0.0f));
}

// SIMD_EXERCISE(3): Rejection-sample x/y in [-1, 1] with z fixed to zero.
inline Vec3f8 random_in_unit_disk_packet(RngPacket8& rng) {
    (void)rng;
    return Vec3f8(Float8(0.0f), Float8(0.0f), Float8(0.0f));
}

} // namespace pg
