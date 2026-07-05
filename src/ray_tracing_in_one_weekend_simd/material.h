#pragma once

#include <algorithm>
#include <cstdint>
#include <immintrin.h>

#include "math.h"

namespace pg {

enum class MaterialKind : std::int32_t {
    Lambertian = 0,
    Metal = 1,
    Dielectric = 2
};

struct Material {
    MaterialKind kind = MaterialKind::Lambertian;
    Vec3f albedo{0.5f, 0.5f, 0.5f};
    float fuzz = 0.0f;
    float refraction_index = 1.0f;

    static Material lambertian(const Vec3f& albedo) {
        return Material{
            MaterialKind::Lambertian,
            albedo,
            0.0f,
            1.0f
        };
    }

    static Material metal(const Vec3f& albedo, float fuzz) {
        return Material{
            MaterialKind::Metal,
            albedo,
            std::clamp(fuzz, 0.0f, 1.0f),
            1.0f
        };
    }

    static Material dielectric(float refraction_index) {
        return Material{
            MaterialKind::Dielectric,
            Vec3f(1.0f, 1.0f, 1.0f),
            0.0f,
            refraction_index
        };
    }
};

struct EnumInt8 {
    __m256i values = _mm256_setzero_si256();
    explicit EnumInt8(__m256i value) : values(value) {}
    explicit EnumInt8(std::int32_t value) : values(_mm256_set1_epi32(value)) {}
    friend EnumInt8 select(const Bool8& mask, const EnumInt8& if_true, const EnumInt8& if_false) {
        return EnumInt8{_mm256_blendv_epi8(if_false.values, if_true.values, _mm256_castps_si256(mask.values)) };
	}
    Bool8 operator==(const EnumInt8& a) const {
        return Bool8{_mm256_castsi256_ps(_mm256_cmpeq_epi32(values, a.values))};
	}
};

struct Material8 {
	EnumInt8 kind{ static_cast<std::int32_t>(MaterialKind::Lambertian) };
    Color8 albedo{};
    Float8 fuzz{0.0f};
    Float8 refraction_index{1.0f};

    static Material8 splat(const Material& material) {
        return Material8{
			EnumInt8(static_cast<std::int32_t>(material.kind)),
            pg::splat(material.albedo),
            Float8(material.fuzz),
            Float8(material.refraction_index)
        };
    }

    friend Material8 select(
        const Bool8& hit_mask,
        const Material8& if_true,
        const Material8& if_false) {
        Material8 result;
		result.kind = select(hit_mask, if_true.kind, if_false.kind);
		result.albedo = select(hit_mask, if_true.albedo, if_false.albedo);
		result.fuzz = select(hit_mask, if_true.fuzz, if_false.fuzz);
		result.refraction_index = select(hit_mask, if_true.refraction_index, if_false.refraction_index);
        return result;
    }

};
} // namespace pg
