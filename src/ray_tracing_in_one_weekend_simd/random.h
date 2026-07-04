#pragma once

#include <cstdint>
#include <immintrin.h>

namespace pg {

    inline std::uint32_t mix32(std::uint32_t x) noexcept
    {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    inline std::uint32_t hash_combine(
        std::uint32_t seed,
        std::uint32_t value) noexcept
    {
        return mix32(
            seed ^
            (
                value +
                0x9e3779b9u +
                (seed << 6) +
                (seed >> 2)
                )
        );
    }

    struct RngPacket8 {
    private:
        __m256i state;

        explicit RngPacket8(__m256i initial_state) noexcept
            : state(initial_state)
        {
        }

        static std::uint32_t make_nonzero(
            std::uint32_t value,
            std::uint32_t lane) noexcept
        {
            if (value != 0u) {
                return value;
            }
            return 0x6d2b79f5u ^ (lane + 1u);
        }

        static __m256i xorshift32x8(__m256i x) noexcept
        {
            x = _mm256_xor_si256(
                x,
                _mm256_slli_epi32(x, 13)
            );

            x = _mm256_xor_si256(
                x,
                _mm256_srli_epi32(x, 17)
            );

            x = _mm256_xor_si256(
                x,
                _mm256_slli_epi32(x, 5)
            );

            return x;
        }

    public:
        RngPacket8() = delete;
        
        static RngPacket8 seeded(std::uint32_t seed) noexcept
        {
            alignas(32) std::uint32_t states[8];

            for (std::uint32_t lane = 0; lane < 8; ++lane) {
                std::uint32_t lane_seed = seed;

                // lane番号を混ぜることで8レーンを異なる系列にする
                lane_seed = hash_combine(lane_seed, lane);

                states[lane] = make_nonzero(
                    lane_seed,
                    lane
                );
            }

            const __m256i initial_state =
                _mm256_load_si256(
                    reinterpret_cast<const __m256i*>(states)
                );

            return RngPacket8{ initial_state };
        }

        // 画素座標とサンプル番号から各レーンのstateを生成する
        static RngPacket8 seeded(
            int u,
            int v) noexcept
        {
            alignas(32) std::uint32_t states[8];

            const std::uint32_t pixel_u =
                static_cast<std::uint32_t>(u);

            const std::uint32_t pixel_v =
                static_cast<std::uint32_t>(v);

            for (std::uint32_t lane = 0; lane < 8; ++lane) {
                // このレーンが担当するサンプル番号
                std::uint32_t lane_seed = 0x243f6a88u;

                lane_seed = hash_combine(lane_seed, pixel_u);
                lane_seed = hash_combine(lane_seed, pixel_v);
                lane_seed = hash_combine(lane_seed, lane);

                states[lane] = make_nonzero(
                    lane_seed,
                    lane
                );
            }

            const __m256i initial_state =
                _mm256_load_si256(
                    reinterpret_cast<const __m256i*>(states)
                );

            return RngPacket8{ initial_state };
        }

        __m256i next_u32() noexcept
        {
            state = xorshift32x8(state);
            return state;
        }

        Float8 next_float01() noexcept
        {
            const __m256i random_bits = next_u32();

            // uint32の上位24ビットを取り出す
            const __m256i random24 =
                _mm256_srli_epi32(random_bits, 8);

            // random24は0～16777215なので、
            // 符号付き整数としてfloatへ変換しても問題ない
            const __m256 random_float =
                _mm256_cvtepi32_ps(random24);

            const __m256 scale =
                _mm256_set1_ps(1.0f / 16777216.0f);

            return Float8{
                _mm256_mul_ps(random_float, scale)
            };
        }
    };

} // namespace pg