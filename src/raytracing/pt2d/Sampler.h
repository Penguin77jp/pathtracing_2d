#pragma once

#include <cstdint>

namespace pt2d {

inline uint64_t mix_bits(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

inline uint64_t hash_combine(uint64_t a, uint64_t b) {
    return mix_bits(a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2)));
}

class Sampler {
public:
    explicit Sampler(uint64_t seed = 1) : state_(mix_bits(seed ? seed : 1)) {}

    uint64_t next_u64() {
        // xorshift64*, compact and deterministic. Good enough for this MVP/debugger.
        uint64_t x = state_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state_ = x;
        return x * 2685821657736338717ULL;
    }

    float next1D() {
        const uint64_t v = next_u64();
        return static_cast<float>((v >> 40) & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    uint64_t state() const { return state_; }

private:
    uint64_t state_;
};

} // namespace pt2d
