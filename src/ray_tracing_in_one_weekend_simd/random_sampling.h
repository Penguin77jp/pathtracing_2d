#pragma once

#include "util/math.h"
#include "random.h"

namespace pg {

// Rejection-sample one point per lane from the unit sphere. Lanes that have
// already accepted a point keep their result while unfinished lanes retry.
inline Vec3f8 random_in_unit_sphere_packet(RngPacket8& rng) {
    const Float8 two(2.0f);
    const Float8 one(1.0f);
    const Float8 minimum_length_squared(1.0e-12f);

    Vec3f8 result = Vec3f8::SetZero();
    Bool8 unfinished = Bool8::constant(true);

    while (_mm256_movemask_ps(unfinished.values) != 0) {
        const Vec3f8 candidate(
            rng.next_float01() * two - one,
            rng.next_float01() * two - one,
            rng.next_float01() * two - one
        );

        const Float8 length_squared = candidate.length_squared();
        const Bool8 accepted = unfinished
            & (length_squared < one)
            & (length_squared > minimum_length_squared);

        result = select(accepted, candidate, result);
        unfinished = unfinished & ~accepted;
    }

    return result;
}

inline Vec3f8 random_unit_vector_packet(RngPacket8& rng) {
    return random_in_unit_sphere_packet(rng).normalized();
}

// SIMD_EXERCISE(3): Rejection-sample x/y in [-1, 1] with z fixed to zero.
inline Vec3f8 random_in_unit_disk_packet(RngPacket8& rng) {
    (void)rng;
    return Vec3f8(Float8(0.0f), Float8(0.0f), Float8(0.0f));
}

} // namespace pg
